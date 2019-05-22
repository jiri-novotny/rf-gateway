#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_arp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/rand.h>

#include "const.h"
#include "crypto.h"
#include "rf.h"
#include "thpool.h"
#include "hashmap.h"
#include "list.h"

#define ETH_P_NONE 0x00FF

typedef struct
{
  uint8_t run;
  int sock;
  RfDevice_t gw;
  struct hashmap *devices;
  threadpool thpool;
} Rf_t;

void rfSendPacket(void *arg);
uint16_t crc16(const uint8_t* data, uint8_t length);

Rf_t rf;

uint16_t rfInit(char *iface)
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

  /* Bind our raw socket to this interface */
  sll.sll_family = AF_PACKET,
  sll.sll_protocol = htons(ETH_P_NONE),
  sll.sll_ifindex = req.ifr_ifindex;
  if ((bind(rf.sock, (struct sockaddr *) &sll, sizeof(sll))) < 0)
  {
    fprintf(stderr, "Socket bind failed for %s\n", iface);
    return 2;
  }

  if (ioctl(rf.sock, SIOCGIFHWADDR, &req) < 0)
  {
    fprintf(stderr, "Get address failed");
    close(rf.sock);
    return 3;
  }

  rf.run = 1;
  memcpy(&rf.gw.addr, req.ifr_hwaddr.sa_data, 4);
  rf.devices = hashmap_create();

  memset(rf.gw.key, 0xff, 16);
  memset(rf.gw.iv, 0xAA, 16);
  rf.thpool = thpool_init(4);

  return 0;
}

uint16_t rfDeInit()
{
  rf.run = 0;
  struct iterator *entries;
  RfDevice_t *dev;

  rf.run = 0;
  entries = hashmap_iterator(rf.devices);
  while (entries->next(entries)) {
    struct hentry *entry = entries->current;
    dev = (RfDevice_t *) entry->value;
    list_destroy(dev->packetQueue);
  }
  entries->destroy(entries);
  hashmap_destroy(rf.devices);
  thpool_destroy(rf.thpool);

  close(rf.sock);
  return 0;
}

uint16_t rfEnqueuePacket(RfPacket_t *packet)
{
  uint32_t dst;
  RfDevice_t *dev;

  if (packet != NULL)
  {
    memcpy(&dst, packet->data + I_DST, 4);
    memcpy(packet->data + I_SRC, &rf.gw.addr, 4);
    dev = hashmap_get(rf.devices, dst);
    if (dev != NULL)
    {
      packet->data[I_FLAGS] &= 0xe0;
      packet->data[I_FLAGS] |= packet->len - I_PAYLOAD;
      packet->data[I_CTR] = dev->ctr++;
      list_push(dev->packetQueue, packet);
      thpool_add_work(rf.thpool, rfSendPacket, dev);
      return 0;
    }
  }
  return 1;
}

void *rfRecvThread(void *arg)
{
  int i;
  int len;
  uint8_t rxbuf[MAX_PACKET_LEN];
  uint16_t crc;

  while (rf.run)
  {
    len = recvfrom(rf.sock, rxbuf, sizeof(rxbuf), 0, NULL, NULL);
    if (len < 0) continue;
    if (len == 0)
    {
      printf("EOF received.\n");
    }
    else
    {
      printf("enc:  { ");
      for (i = 0; i < len; i += 4)
      {
        printf("%02x %02x %02x %02x ", rxbuf[i], rxbuf[i + 1], rxbuf[i + 2], rxbuf[i + 3]);
      }
      printf("}\n");

      decryptPacket(rxbuf + I_SRC, 32, rf.gw.key, rf.gw.iv, rxbuf + I_SRC);
      crc = crc16(rxbuf + I_SRC, 30);
      printf("data: { ");
      for (i = 0; i < len; i += 4)
      {
        printf("%02x %02x %02x %02x ", rxbuf[i], rxbuf[i + 1], rxbuf[i + 2], rxbuf[i + 3]);
      }
      printf("}, crc %s\n", (memcmp(rxbuf + 34, &crc, 2) == 0) ? "ok" : "fail");

      //parse();
      //pop(&rf.packetQueue);
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
    x ^= x>>4;
    crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
  }

  return crc;
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
    RAND_bytes(tmp->data + I_RAND, 1);
    RAND_bytes((uint8_t *) &sleep, 2);
    crc = crc16(tmp->data + I_SRC, 30);
    memcpy(tmp->data + I_CRC, &crc, 2);
    encryptPacket(tmp->data + I_SRC, 32, dev->key, dev->iv, tmp->data + I_SRC);
    send(rf.sock, tmp->data, MAX_PACKET_LEN, 0);
    usleep(2000 + sleep);
  }
}
