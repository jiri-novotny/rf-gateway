#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "config.h"
#include "rf.h"
#include "udp.h"
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
  pthread_t thr;
  char *interface;
  RfDevice_t gw;

  if (argc <= 1)
  {
    fprintf(stdout, "USAGE: %s /path/to/config/json\n", argv[0]);
    return 0;
  }

  if (rfInit())
  {
    fprintf(stderr, "rfInit failed\n");
    return 1;
  }
  memset(&gw, 0, sizeof(RfDevice_t));

  if (configParse(argv[1], &interface, &gw))
  {
    fprintf(stderr, "config failed\n");
    rfDeInit();
    return 1;
  }

  if (rfOpen(interface, &gw))
  {
    fprintf(stderr, "rfOpen failed\n");
    rfDeInit();
    free(interface);
    return 2;
  }
  pthread_create(&thr, NULL, rfRecvThread, NULL);
  pthread_detach(thr);

  if (udpInit(62000))
  {
    fprintf(stderr, "udpInit failed\n");
    rfDeInit();
    free(interface);
    return 3;
  }
  pthread_create(&thr, NULL, udpListener, NULL);
  pthread_detach(thr);

  if (wsInit(65000))
  {
    fprintf(stderr, "wsInit failed\n");
    udpDeInit();
    rfDeInit();
    free(interface);
    return 4;
  }
  pthread_create(&thr, NULL, wsAcceptThread, NULL);
  pthread_detach(thr);

  line = (char *) malloc(80);
  app.run = 1;
  while (app.run)
  {
    getline(&line, &len, stdin);
    if (0 == strncmp("quit", line, 4))
    {
      app.run = 0;
      wsDeInit();
      udpDeInit();
      rfDeInit();
      free(interface);
    }
  }
  free(line);

  return 0;
}
