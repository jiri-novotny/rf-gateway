#ifndef UDP_H
#define UDP_H

uint16_t udpInit(uint16_t port);
uint16_t udpDeInit(void);
void *udpListener(void *arg);

#endif // UDP_H
