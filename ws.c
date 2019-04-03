#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <openssl/sha.h>

#include "base64.h"
#include "jsmn.h"
#include "ws.h"

typedef struct 
{
  int sock;
  int run;
} Ws_t;

Ws_t ws;

uint16_t wsInit(uint16_t port)
{
  int sockopt;
  struct sockaddr_in localAddr;

  if ((ws.sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
  {
    fprintf(stderr, "ws socket() failed\n");
    return 1;
  }

  memset(&localAddr, 0, sizeof(localAddr));
  localAddr.sin_family = AF_INET;
  localAddr.sin_port = htons(port);

  sockopt = 1;
  setsockopt(ws.sock, SOL_SOCKET, SO_REUSEADDR, (const void *) &sockopt , sizeof(int));
  
  if (bind(ws.sock, (struct sockaddr *) &localAddr, sizeof(localAddr)) == -1)
  {
    fprintf(stderr, "ws bind() failed\n");
    close(ws.sock);
    return 1;
  }

  if (listen(ws.sock, 25) == -1)
  {
    fprintf(stderr, "ws listen() failed\n");
    close(ws.sock);
    return 3;
  }

  ws.run = 1;

  return 0;
}

uint16_t wsDeInit()
{
  ws.run = 0;
  close(ws.sock);
  return 0;
}

void *wsAcceptThread(void *arg)
{
  int remoteSockfd;
  struct sockaddr_in remoteAddr;
  unsigned int remoteAddrLen = sizeof(remoteAddr);
  pthread_t client;

  while (ws.run)
  {
    if ((remoteSockfd = accept(ws.sock, (struct sockaddr *) &remoteAddr, &remoteAddrLen)) == -1)
    {
      fprintf(stderr, "ws accept failed\n");
    }
    else
    {
      pthread_create(&client, NULL, wsRecvThread, (void *) &remoteSockfd);
      pthread_detach(client);
    }
  }

  pthread_exit(NULL);
}

void *wsRecvThread(void *arg)
{
  unsigned char buffer[WS_BUFFER_SIZE];
  char response[WS_BUFFER_SIZE];
  unsigned char *data;
  char *tmp;
  char keyIn[128];
  unsigned char keyOut[128];
	ssize_t length;
  int sockfd;
  int i;
  int j;
  size_t baseLen;
  unsigned char *baseData;
  unsigned long long wsLen;
  unsigned keyOffset;
  unsigned char mask[4];
#if 0
  jsmn_parser jsonParser;
  jsmntok_t jsonToken[16];
#endif

  fprintf(stdout, "thread %ld start\n", syscall(__NR_gettid));
  memcpy((void *) &sockfd, arg, sizeof(int));
  memset(keyIn, 0, 128);
  memset(keyOut, 0, 128);

  if ((length = recv(sockfd, buffer, WS_BUFFER_SIZE, 0)) > 0)
  {
    buffer[length] = 0;
    data = buffer;
    tmp = strsep((char **) &data, "\n");
    if (strstr(tmp, "GET") != NULL)
    {
      do
      {
        tmp = strsep((char **) &data, "\n");
        if (strstr(tmp, "Sec-WebSocket-Key:") != NULL)
        {
          strcpy(keyIn, tmp + strlen("Sec-WebSocket-Key: "));
          keyIn[strlen(keyIn) - 1] = 0;
          strcat(keyIn, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        }
      } while (strlen(tmp) > 1);
      fprintf(stdout, "key %s\n", (char *) keyIn);
      SHA1((unsigned char *)keyIn, strlen(keyIn), keyOut);
      strcpy((char *) buffer, "HTTP/1.1 101 Switching Protocols\nConnection: Upgrade\nUpgrade: websocket\nSec-WebSocket-Accept: ");
      j = strlen((char *) buffer);
      strcat((char *)buffer, (char *) base64_encode(keyOut, strlen((char *) keyOut), NULL));
      strcat((char *)buffer, "\n\n");
      write(sockfd, buffer, strlen((char *)buffer));

      while (length)
      {
        if ((length = recv(sockfd, buffer, WS_BUFFER_SIZE, 0)) > 0)
        {
          if ((buffer[0] & 0x0f) == 0x08)
          {
            fprintf(stdout, "connection closed\n");
            buffer[0] = 0x88;
            buffer[1] = 0x00;
            write(sockfd, buffer, 2);
            length = 0;
          }
          else if ((buffer[0] & 0x0f) == 0x09)
          {
            /* set pong */
            buffer[0] &= 0xF0;
            buffer[0] |= 0x0A;
            write(sockfd, buffer, length);
          }
          else if (buffer[1] > 0x80)
          {
            /* parse ws data */
            keyOffset = 2;
            wsLen = (buffer[1] - 0x80);

            if (wsLen == 126)
            {
              memcpy((void *) &wsLen, buffer + 2, 2);
              wsLen = htons(wsLen);
              keyOffset = 4;
            }
            else if (wsLen == 127)
            {
              memcpy((void *) &wsLen, buffer + 2, 8);
              wsLen = htobe64(wsLen);
              keyOffset = 10;
            }

            if (wsLen >= 126)
            {
              while (length != (wsLen + keyOffset + 4)) {
                i = recv(sockfd, buffer + length, WS_BUFFER_SIZE, 0);
                length += i;
              }
            }

            fprintf(stdout, "%llu\n", wsLen);

            memcpy((void *) &mask, buffer + keyOffset, 4);
            data = (unsigned char *) malloc(wsLen + 1);
            for (i = keyOffset + 4, j = 0; j < wsLen; i++, j++)
            {
               data[j] = buffer[i] ^ mask[j & 0x3];
            }
            data[j] = 0;
#if 0
            /* packet should be in json format */
            jsmn_init(&jsonParser);
            j = jsmn_parse(&jsonParser, (char *) data, strlen((char *) data), jsonToken, sizeof(jsonToken) / sizeof(jsonToken[0]));
            if (j < 0)
            {
              sprintf(response, "{\"response\":\"ERR\", \"data\":[{\"id\":\"text%d""\", \"value\":\"%s\"}]}", 1, "Invalid request");
              fprintf(stdout, "%s\n", response);
            }
            else
            {
              /* ws packet received */
              fprintf(stdout, "%s\n", data);

              /* parse json structure */
              for (i = 1; i < j; i++) {
                if (strncmp(((char *) data) + jsonToken[i].start, "data", jsonToken[i].end - jsonToken[i].start) == 0)
                {
                  sprintf(response, "{\"response\":\"OK\", \"data\":[{\"id\":\"text%d""\", \"value\":\"%.*s\"}]}", 1, jsonToken[i + 1].end - jsonToken[i + 1].start, data + jsonToken[i + 1].start);
                  i++;
                }
              }

              fprintf(stdout, "%s\n", response);
            }
#endif
            baseData = base64_decode(data, j, &baseLen);
            for (j = 0; j < baseLen; j++)
            {
              printf("%02x ", baseData[j]);
            }
            printf("\n");
            for (j = 0; j < baseLen; j++)
            {
              printf("%c ", baseData[j]);
            }
            printf("\n");
            sprintf(response, "{\"response\":\"OK\", \"data\":[]}");
            buffer[0] = 0x81; /* fin + text frame */
            wsLen = strlen(response);
            if (wsLen >= 126)
            {
              buffer[1] = 0x7E;
              buffer[2] = (wsLen >> 8) & 0xFF;
              buffer[3] = wsLen & 0xFF;
              keyOffset = 4;
            }
            else
            {
              buffer[1] = wsLen & 0xFF;
              keyOffset = 2;
            }
#ifdef USE_MASK_RESP
            buffer[1] |= 0x80;
            memcpy(buffer + keyOffset, mask, 4);
            keyOffset += 4;
#endif
            for (i = keyOffset, j = 0; j < wsLen; i++, j++)
            {
#ifdef USE_MASK_RESP
              buffer[i] = response[j] ^ mask[j & 0x3];
#else
              buffer[i] = response[j];
#endif
            }
            write(sockfd, buffer, wsLen + keyOffset);
            free(baseData);
            free(data);
          }
        }
      }
    }
    else
    {
      fprintf(stderr, "Invalid get reguest: %s\n", tmp);
    }
  }

  close(sockfd);
  fprintf(stdout, "thread %ld end\n", syscall(__NR_gettid));
  pthread_exit(NULL);
}