#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

void initQueue(Queue_t *f)
{
  f->head = NULL;
  f->tail = NULL;
  f->size = 0;
  pthread_mutex_init(&f->lock, NULL);
}

void push(Queue_t *f, uint8_t *data, uint8_t len, int originFd, void *misc, int miscLen)
{
  Packet_t *tmp = (Packet_t *) malloc(sizeof(Packet_t));

  if (tmp != NULL)
  {
    memcpy(tmp->data, data, len);
    memset(tmp->data + len, 0, MAX_PACKET_LEN - len);
    tmp->len = len;
    tmp->origin = originFd;
    memcpy(tmp->misc, misc, miscLen);
    tmp->miscLen = miscLen;
    tmp->next = NULL;

    if (f != NULL)
    {
      pthread_mutex_lock(&f->lock);
      if (NULL == f->head && NULL == f->tail)
      {
        f->head = f->tail = tmp;
      }
      else
      {
        f->tail->next = tmp;
        f->tail = tmp;
      }
      f->size += 1;
      pthread_mutex_unlock(&f->lock);
    }
    else
    {
      free(tmp);
    }
  }
}

Packet_t *pop(Queue_t *f)
{
  Packet_t *tmp = NULL;

  if (f != NULL)
  {
    pthread_mutex_lock(&f->lock);
    if (!queueEmpty(f))
    {
      tmp = f->head;
      f->head = tmp->next;
      /* The element tail was pointing to is free(), so we need an update */
      if (NULL == f->head) f->tail = f->head;
      f->size -= 1;
    }
    pthread_mutex_unlock(&f->lock);
  }

  return tmp;
}

Packet_t *first(Queue_t *f)
{
  return f->head;
}

void queueClear(Queue_t *f)
{
  Packet_t *tmp = NULL;

  if (f != NULL)
  {
    pthread_mutex_lock(&f->lock);
    while (f->head)
    {
      tmp = pop(f);
      free(tmp);
    }
    pthread_mutex_unlock(&f->lock);
  }
}

int queueEmpty(Queue_t *f)
{
  return (NULL == f->head && NULL == f->tail) ? 1 : 0;
}
