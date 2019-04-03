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

typedef struct {
  int run;
  int sock;
} RfHandler_t;

RfHandler_t app;

int main(int argc, char * argv[])
{
  char *line;
  size_t len;
  pthread_t uthr;
  pthread_t rthr;
  pthread_t hthr;
  pthread_t wthr;

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
  pthread_create(&rthr, NULL, rfRecvThread, NULL);
  pthread_detach(rthr);

  if (udpInit(62000))
  {
    fprintf(stderr, "udpInit failed");
    rfDeInit();
    return 2;
  }
  pthread_create(&uthr, NULL, udpListener, NULL);
  pthread_detach(uthr);

  if (httpInit(65001))
  {
    fprintf(stderr, "httpInit failed");
    udpDeInit();
    rfDeInit();
    return 3;
  }
  pthread_create(&hthr, NULL, httpAcceptThread, NULL);
  pthread_detach(hthr);

  if (wsInit(65000))
  {
    fprintf(stderr, "wsInit failed");
    udpDeInit();
    httpDeInit();
    rfDeInit();
    return 3;
  }
  pthread_create(&wthr, NULL, wsAcceptThread, NULL);
  pthread_detach(wthr);

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
