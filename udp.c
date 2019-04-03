#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_arp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <math.h>

#include "udp.h"
#include "const.h"
#include "queue.h"
#include "rf.h"

typedef struct
{
  int sock;
  uint8_t run;
} Udp_t;

Udp_t udp;

uint16_t udpInit(uint16_t port)
{
  int sockopt;
  struct sockaddr_in localAddr;

  /* create the socket */
  if ((udp.sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    fprintf(stderr, "socket failed\n");
    return 1;
  }

  /* fill struct sockaddr */
  memset(&localAddr, 0, sizeof(localAddr));
  localAddr.sin_family = AF_INET;
  localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  localAddr.sin_port = htons(port);

  sockopt = 1;
  setsockopt(udp.sock, SOL_SOCKET, SO_REUSEADDR, (const void *) &sockopt , sizeof(int));
  /* bind socket */
  if (bind(udp.sock, (struct sockaddr *) &localAddr, sizeof(localAddr)) == -1)
  {
    fprintf(stderr, "udp bind failed\n");
    return 2;
  }

  udp.run = 1;

  return 0;
}

uint16_t udpDeInit()
{
  udp.run = 0;
  close(udp.sock);
  return 0;
}

void *udpListener(void *arg)
{
  int i;
  struct sockaddr_in clientAddr;
  socklen_t clientLen;
  int len;
  uint8_t data[MAX_PACKET_LEN];

  while (udp.run)
  {
    memset(data, 0, MAX_PACKET_LEN);
    len = recvfrom(udp.sock, data, MAX_PACKET_LEN, 0, (struct sockaddr *) &clientAddr, &clientLen);
    if (len < 0)
    {
      fprintf(stderr, "recvfrom failed\n");
    }
    else
    {
      rfEnqueuePacket(data, len, udp.sock, &clientAddr, clientLen);
      fprintf(stdout, "udp:  { ");
      for (i = 0; i < len; i += 1)
      {
        printf("%02x ", data[i]);
      }
      printf("}\n");
    }
  }

  pthread_exit(NULL);
}
