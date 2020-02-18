#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <linux/if_packet.h>
#include <linux/if_arp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/rand.h>

#include "const.h"
#include "crypto.h"
#include "rf.h"
#include "hashmap.h"
#include "list.h"

#define ETH_P_NONE 0x00FF

typedef struct
{
  uint8_t run;
  int sock;
  RfDevice_t gw;
  struct hashmap *devices;
} Rf_t;

uint16_t crc16(const uint8_t* data, uint8_t length);
uint32_t fnv1aHash(const void *data, size_t length);
void rfSendPacket(void *arg);

static Rf_t rf;

uint16_t rfInit()
{
  uint16_t ret = 1;

  rf.sock = -1;
  rf.devices = hashmap_create();
  if (rf.devices)
  {
    ret = 0;
  }

  return ret;
}

uint16_t rfOpen(char *iface, RfDevice_t *gw)
{
  struct ifreq req;
  struct sockaddr_ll sll;

  rf.sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_NONE));
  strncpy((char *) req.ifr_name, iface, IFNAMSIZ);
  if ((ioctl(rf.sock, SIOCGIFINDEX, &req)) < 0)
  {
    fprintf(stderr, "Socket index failed for %s\n", iface);
    return 1;
  }
  sll.sll_family = AF_PACKET,
  sll.sll_protocol = htons(ETH_P_NONE),
  sll.sll_ifindex = req.ifr_ifindex;

  if (ioctl(rf.sock, SIOCGIFFLAGS, &req) < 0)
  {
    fprintf(stderr, "Get flags failed\n");
    close(rf.sock);
    return 2;
  }

  if (req.ifr_flags & IFF_UP)
  {
    fprintf(stderr, "Interface is already UP\n");
  }
  else
  {
    req.ifr_hwaddr.sa_family = ARPHRD_NONE;
    /* we use addr as placeholder for bcast */
    memcpy(req.ifr_hwaddr.sa_data + 4, &gw->addr, 4);
    /* real address is computed as hash */
    gw->addr = fnv1aHash(&gw->sn, 4);
    memcpy(req.ifr_hwaddr.sa_data, &gw->addr, 4);
    req.ifr_hwaddr.sa_data[8] = 0x00;
    req.ifr_hwaddr.sa_data[9] = (char) 0xf0;
    if (ioctl(rf.sock, SIOCSIFHWADDR, &req) < 0)
    {
      fprintf(stderr, "Set address failed\n");
      close(rf.sock);
      return 4;
    }

    req.ifr_flags |= IFF_UP;
    if (ioctl(rf.sock, SIOCSIFFLAGS, &req) < 0)
    {
      fprintf(stderr, "Set flags failed\n");
      close(rf.sock);
      return 5;
    }
  }

  /* Bind our raw socket to this interface */
  if ((bind(rf.sock, (struct sockaddr *) &sll, sizeof(sll))) < 0)
  {
    fprintf(stderr, "Socket bind failed for %s\n", iface);
    return 6;
  }

  rf.run = 1;

  return 0;
}

uint16_t rfDeInit()
{
  struct iterator *entries;
  RfDevice_t *dev;

  rf.run = 0;
  entries = hashmap_iterator(rf.devices);
  while (entries->next(entries)) {
    struct hentry *entry = entries->current;
    dev = (RfDevice_t *) entry->value;
    printf("freeing dev %08x\n", dev->sn);
    list_destroy(dev->packetQueue);
    free(dev);
  }
  entries->destroy(entries);
  hashmap_destroy(rf.devices);

  if (rf.sock > 0)
    close(rf.sock);

  return 0;
}

uint16_t rfAddDevice(RfDevice_t *newDev)
{
  newDev->addr = fnv1aHash((const void *) &newDev->sn, 4);
  RfDevice_t *old = hashmap_set(rf.devices, newDev->addr, newDev);
  if (old != NULL)
  {
    printf("old dev found!\n");
    free(old);
  }
  printf("dev %08x (%08x) stored\n", newDev->sn, newDev->addr);
  return 0;
}

uint16_t rfEnqueuePacket(RfPacket_t *packet)
{
  RfDevice_t *dev;

  if (packet != NULL)
  {
    memcpy(packet->data + I_SRC, &rf.gw.addr, 4);
    printf("packet to %08x\n", *((uint32_t *)(packet->data + I_DST)));
    dev = hashmap_get(rf.devices, *((uint32_t *)(packet->data + I_DST)));
    if (dev != NULL)
    {
      packet->data[I_CTR] = dev->ctr++;
      list_push(dev->packetQueue, packet);
      printf("scheduling packet %d\n", packet->data[I_CTR]);
      /* fixme */
      return 0;
    }
    else
    {
      printf("dev not found\n");
    }
  }
  return 1;
}

void *rfRecvThread(void *arg)
{
  int i;
  int len;
  uint8_t rxbuf[MAX_PACKET_LEN];
  uint8_t txbuf[MAX_PACKET_LEN];
  uint16_t crc;
  RfDevice_t *rd;
  RfPacket_t *next;

  while (rf.run)
  {
    memset(txbuf, 0, MAX_PACKET_LEN);
    memcpy(txbuf + I_SRC, &rf.gw.addr, 4);
    len = recvfrom(rf.sock, rxbuf, sizeof(rxbuf), 0, NULL, NULL);
    if (len < 0) continue;
    if (len == 0)
    {
      printf("EOF received.\n");
    }
    else
    {
      printf("recv: { ");
      for (i = 0; i < len; i += 1)
      {
        printf("%02x ", rxbuf[i]);
      }
      printf("}");

      rd = hashmap_get(rf.devices, *((uint32_t *) &rxbuf[4]));
      if (rd != NULL)
      {
        if (rxbuf[I_CTRL] & 0x80)
        {
          decryptPacket(rxbuf + I_CTR, 16, rd->key, rd->iv, rxbuf + I_CTR);
          printf("\ndec:  { ");
          for (i = 0; i < len; i += 1)
          {
            printf("%02x ", rxbuf[i]);
          }
          printf("}");
        }
        crc = crc16(rxbuf + I_SRC, 9 + rxbuf[I_LEN]);

        if (memcmp(rxbuf + 13 + rxbuf[I_LEN], &crc, 2) == 0)
        {
          printf(", crc ok\n");
          memcpy(txbuf, rxbuf + I_SRC, 4);
          switch (rxbuf[I_CMD])
          {
            /* check if resp for request */
          case C_NOTIFY:
            /* do smth */

            /* prepare response - check queue or ack */
            txbuf[I_CTRL] = 0x80;
            txbuf[I_CMD] = C_ACK;
            txbuf[I_LEN] = 0;
            break;

          case C_POLL:
            txbuf[I_CTRL] = 0x80;
            if (list_empty(rd->packetQueue))
            {
              txbuf[I_CMD] = C_ACK;
              txbuf[I_LEN] = 0;
            }
            else
            {
              next = list_pop(rd->packetQueue);
              memcpy(txbuf, next->data, next->len);
            }
            break;
          default:
            break;
          }

          txbuf[I_CTR] = rxbuf[I_CTR] | 0x80;
          RAND_bytes(txbuf + I_RND, 1);
          crc = crc16(txbuf + I_SRC, 9 + txbuf[I_LEN]);
          memcpy(txbuf + I_DATA + txbuf[I_LEN], &crc, 2);
          printf("resp: { ");
          for (i = 0; i < 25; i += 1)
          {
            printf("%02x ", txbuf[i]);
          }
          printf("}\n");

          if (txbuf[I_CTRL] & 0x80)
          {
            encryptPacket(txbuf + I_CTR, 16, rd->key, rd->iv, txbuf + I_CTR);
          }
          send(rf.sock, txbuf, 25, 0);

          printf("enc:  { ");
          for (i = 0; i < 25; i += 1)
          {
            printf("%02x ", txbuf[i]);
          }
          printf("}\n");
        } else printf(", crc fail\n");

      } else printf("\nunknown device %08x\n", *((uint32_t *) (rxbuf + I_SRC)));
    }
  }
  pthread_exit(NULL);
}

/* --- private functions --- */
uint16_t crc16(const uint8_t* data, uint8_t length)
{
  uint8_t x;
  uint16_t crc = 0xFFFF;

  while (length--)
  {
    x = crc >> 8 ^ *data++;
    x ^= x >> 4;
    crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x << 5)) ^ ((uint16_t) x);
  }

  return crc;
}

/* Private: Computes the FNV-1a hash of the key data.
 *
 * data   - The bytes to hash.
 * length - The number of bytes in data.
 *
 * Returns the numerical hash of the input bytes.
 */
uint32_t fnv1aHash(const void *data, size_t length)
{
    const unsigned char *bytes = data;
    uint32_t hash = 2166136261;

    for (size_t i = 0; i < length; i++) {
        hash = (hash ^ bytes[i]) * 16777619;
    }
    return hash;
}

void rfSendPacket(void *arg)
{
  uint16_t crc;
  uint16_t sleep;
  RfPacket_t *tmp;
  RfDevice_t *dev = (RfDevice_t *) arg;

  while (false == list_empty(dev->packetQueue))
  {
    tmp = list_first(dev->packetQueue);
    RAND_bytes(tmp->data + I_RND, 1);
    RAND_bytes((uint8_t *) &sleep, 2);
    crc = crc16(tmp->data + I_SRC, 9 + tmp->data[I_LEN]);
    memcpy(tmp->data + I_DATA + tmp->data[I_LEN], &crc, 2);
    if (tmp->data[I_CTRL] & 0x80)
    {
      encryptPacket(tmp->data + I_CTR, 16, dev->key, dev->iv, tmp->data + I_CTR);
      tmp->len = 25;
    }
    send(rf.sock, tmp->data, tmp->len, 0);
    usleep(5000 + sleep);
  }
}
