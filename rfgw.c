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

#include "rf.h"
#include "udp.h"
#include "http.h"
#include "ws.h"
#include "list.h"

typedef struct {
  int run;
  int sock;
  uint32_t devices[2];
} RfHandler_t;

RfHandler_t app;

int main(int argc, char * argv[])
{
  char *line;
  size_t len;
  int i;
  pthread_t thr;
  RfDevice_t *rd;

  if (argc <= 1)
  {
    fprintf(stdout, "USAGE: %s rf_name\n", argv[0]);
    return 0;
  }

  if (rfInit(argv[1]))
  {
    fprintf(stderr, "rfInit failed");
    return 1;
  }
  pthread_create(&thr, NULL, rfRecvThread, NULL);
  pthread_detach(thr);

  if (udpInit(62000))
  {
    fprintf(stderr, "udpInit failed");
    rfDeInit();
    return 2;
  }
  pthread_create(&thr, NULL, udpListener, NULL);
  pthread_detach(thr);

  if (httpInit(65001))
  {
    fprintf(stderr, "httpInit failed");
    udpDeInit();
    rfDeInit();
    return 3;
  }
  pthread_create(&thr, NULL, httpAcceptThread, NULL);
  pthread_detach(thr);

  if (wsInit(65000))
  {
    fprintf(stderr, "wsInit failed");
    udpDeInit();
    httpDeInit();
    rfDeInit();
    return 3;
  }
  pthread_create(&thr, NULL, wsAcceptThread, NULL);
  pthread_detach(thr);

  /* FIXME: load devices from conf */
  app.devices[0] = 0xa3a4e4ad;
  app.devices[1] = 0xa3ef74f4;
  for (i = 0; i < 2; i++)
  {
    rd = (RfDevice_t *) malloc(sizeof(RfDevice_t));
    rd->addr = app.devices[i];
    memset(rd->key, 0xFF, 16);
    memset(rd->iv, 0xAA, 16);
    rd->ctr = 0;
    rd->packetQueue = list_create();
    rfAddDevice(rd);
  }

  line = (char *) malloc(80);
  app.run = 1;
  while (app.run)
  {
    getline(&line, &len, stdin);
    if (0 == strncmp("quit", line, 4))
    {
      app.run = 0;
      wsDeInit();
      httpDeInit();
      udpDeInit();
      rfDeInit();
    }
  }
  free(line);

  return 0;
}
