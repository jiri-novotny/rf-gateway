#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <pthread.h>
#include "const.h"

typedef struct packet
{
  uint8_t data[MAX_PACKET_LEN];
  uint8_t len;
  int origin;
  uint8_t misc[128];
  int miscLen;
  struct packet *next;
} Packet_t;

typedef struct
{
  Packet_t *head;
  Packet_t *tail;
  uint16_t size;
  pthread_mutex_t lock;
} Queue_t;

void initQueue(Queue_t *f);

void push(Queue_t *f, uint8_t *data, uint8_t len, int originFd, void *misc, int miscLen);
Packet_t *pop(Queue_t *f);
Packet_t *first(Queue_t *f);
void queueClear(Queue_t *f);
int queueEmpty(Queue_t *f);

#endif /* QUEUE_H */
