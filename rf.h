#ifndef RF_H
#define RF_H

#include <stdint.h>

uint16_t rfInit(char *iface);
uint16_t rfDeInit(void);
uint16_t rfEnqueuePacket(uint8_t *data, uint8_t len, int originFd, void *misc, int miscLen);

void *rfRecvThread(void *arg);

#endif // RF_H
