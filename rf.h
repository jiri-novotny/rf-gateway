#ifndef RF_H
#define RF_H

#include <stdint.h>
#include "const.h"

typedef struct rf
{
  uint32_t addr;
  uint8_t key[16];
  uint8_t iv[16];
  uint8_t ctr;
  struct list *packetQueue;
} RfDevice_t;

typedef struct packet
{
  uint8_t data[MAX_PACKET_LEN];
  uint8_t len;
  int origin;
  uint8_t misc[128];
  int miscLen;
} RfPacket_t;

uint16_t rfInit(char *iface);
uint16_t rfDeInit(void);
uint16_t rfEnqueuePacket(RfPacket_t *packet);

void *rfRecvThread(void *arg);

#endif // RF_H
