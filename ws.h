
#ifndef WS_H
#define WS_H

#define WS_BUFFER_SIZE 262144

uint16_t wsInit(uint16_t port);
uint16_t wsDeInit();
void *wsAcceptThread(void *arg);
void *wsRecvThread(void *arg);

#endif	/* WS_H */

