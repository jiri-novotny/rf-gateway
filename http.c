#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <netinet/in.h>

#include "http.h"

typedef struct 
{
  int sock;
  int run;
} Http_t;

Http_t http;

uint16_t httpInit(uint16_t port)
{
  int sockopt;
  struct sockaddr_in localAddr;

  if ((http.sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
  {
    fprintf(stderr, "http socket() failed\n");
    return 1;
  }

  memset(&localAddr, 0, sizeof(localAddr));
  localAddr.sin_family = AF_INET;
  localAddr.sin_port = htons(port);

  sockopt = 1;
  setsockopt(http.sock, SOL_SOCKET, SO_REUSEADDR, (const void *) &sockopt , sizeof(int));
  
  if (bind(http.sock, (struct sockaddr *) &localAddr, sizeof(localAddr)) == -1)
  {
    fprintf(stderr, "http bind() failed\n");
    close(http.sock);
    return 1;
  }

  if (listen(http.sock, 25) == -1)
  {
    fprintf(stderr, "http listen() failed\n");
    close(http.sock);
    return 3;
  }

  http.run = 1;

  return 0;
}

uint16_t httpDeInit()
{
  http.run = 0;
  close(http.sock);
  return 0;
}

void *httpAcceptThread(void *arg)
{
  int remoteSockfd;
  struct sockaddr_in remoteAddr;
  unsigned int remoteAddrLen = sizeof(remoteAddr);
  pthread_t client;

  while (http.run)
  {
    if ((remoteSockfd = accept(http.sock, (struct sockaddr *) &remoteAddr, &remoteAddrLen)) == -1)
    {
      fprintf(stderr, "http accept failed\n");
    }
    else
    {
      pthread_create(&client, NULL, httpRecvThread, (void *) &remoteSockfd);
      pthread_detach(client);
    }
  }

  pthread_exit(NULL);
}

void *httpRecvThread(void *arg)
{
  unsigned char buffer[HTTP_BUFFER_SIZE];
  char response[HTTP_BUFFER_SIZE];
  unsigned char *data;
  char *tmp;
  int sockfd;
	ssize_t length;

  fprintf(stdout, "thread %ld start\n", syscall(__NR_gettid));
  memcpy((void *) &sockfd, arg, sizeof(int));

  if ((length = recv(sockfd, buffer, HTTP_BUFFER_SIZE, 0)) > 0)
  {
    buffer[length] = 0;
    data = buffer;
    tmp = strsep((char **) &data, "\n");
    if (strstr(tmp, "GET / ") != NULL)
    {
      sprintf(response, "HTTP/1.1 200 OK\nContent-Type: text/html; charset=UTF-8\nContent-Length: %u\n\n<!DOCTYPE html><html lang=en><title>Page OK</title><div id=content>Page embedded</div>", strlen("<!DOCTYPE html><html lang=en><title>Page OK</title><div id=content>Page embedded</div>"));
    }
    else
    {
      sprintf(response, "HTTP/1.1 404 Not Found\nContent-Type: text/html; charset=UTF-8\nContent-Length: %u\n\n<!DOCTYPE html><html lang=en><title>Page Not Found</title><div id=content>Page not found</div>", strlen("<!DOCTYPE html><html lang=en><title>Page Not Found</title><div id=content>Page not found</div>"));
    }
    write(sockfd, response, strlen(response));
  }

  close(sockfd);
  fprintf(stdout, "thread %ld end\n", syscall(__NR_gettid));
  pthread_exit(NULL);
}
