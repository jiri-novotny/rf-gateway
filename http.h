
#ifndef _HTTPTHREAD_H
#define	_HTTPTHREAD_H

#define HTTP_BUFFER_SIZE 2048

uint16_t httpInit(uint16_t port);
uint16_t httpDeInit();
void *httpAcceptThread(void *arg);
void *httpRecvThread(void *arg);

#endif	/* _HTTPTHREAD_H */

