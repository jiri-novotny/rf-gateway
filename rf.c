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
#include "queue.h"
#include "thpool.h"

#define ETH_P_NONE 0x00FF

typedef struct
{
  uint8_t run;
  int sock;
  uint32_t addr;
  uint8_t key[16];
  uint8_t iv[16];
  uint8_t ctr;
  Queue_t packetQueue;
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
  memcpy(&rf.addr, req.ifr_hwaddr.sa_data, 4);
  memset(rf.key, 0xff, 16);
  memset(rf.iv, 0xAA, 16);
  rf.ctr = 0;
  initQueue(&rf.packetQueue);
  rf.thpool = thpool_init(8);
  
  return 0;
}

uint16_t rfDeInit()
{
  rf.run = 0;
  thpool_destroy(rf.thpool);
  queueClear(&rf.packetQueue);
  close(rf.sock);
  return 0;
}

uint16_t rfEnqueuePacket(uint8_t *data, uint8_t len, int originFd, void *misc, int miscLen)
{
  memcpy(data + I_SRC, &rf.addr, 4);
  data[I_FLAGS] &= 0xe0;
  data[I_FLAGS] |= len - I_PAYLOAD;
  data[I_CTR] = rf.ctr++;
  push(&rf.packetQueue, data, MAX_PACKET_LEN, originFd, misc, miscLen);
  thpool_add_work(rf.thpool, rfSendPacket, NULL);
  return 0;
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

      decryptPacket(rxbuf + I_SRC, 32, rf.key, rf.iv, rxbuf + I_SRC);
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
  Packet_t *tmp;

  while (!queueEmpty(&rf.packetQueue))
  {
    tmp = first(&rf.packetQueue);
    RAND_bytes(tmp->data + I_RAND, 1);
    crc = crc16(tmp->data + I_SRC, 30);
    memcpy(tmp->data + I_CRC, &crc, 2);
    encryptPacket(tmp->data + I_SRC, 32, rf.key, rf.iv, tmp->data + I_SRC);
    send(rf.sock, tmp->data, MAX_PACKET_LEN, 0);
    usleep(2000);
  }
}