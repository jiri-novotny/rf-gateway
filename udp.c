#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <errno.h>
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
  RfPacket_t *packet;

  while (udp.run)
  {
    packet = (RfPacket_t *) calloc(1, sizeof(RfPacket_t));
    if (packet != NULL)
    {
      packet->len = recvfrom(udp.sock, packet->data, MAX_PACKET_LEN, 0, (struct sockaddr *) &packet->misc, (socklen_t *) &packet->miscLen);
      if (packet->len < 0)
      {
        fprintf(stderr, "recvfrom failed\n");
        free(packet);
      }
      else
      {
        packet->origin = udp.sock;
        rfEnqueuePacket(packet);
#if 0
        fprintf(stdout, "udp:  { ");
        for (int i = 0; i < packet->len; i += 1)
        {
          printf("%02x ", packet->data[i]);
        }
        printf("}\n");
#endif
      }
    }
  }

  pthread_exit(NULL);
}
