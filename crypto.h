#ifndef CRYPTO_H
#define CRYPTO_H

int encryptPacket(unsigned char *inBuf, int inLen, unsigned char *key, unsigned char *iv, unsigned char *outBuf);
int decryptPacket(unsigned char *inBuf, int inLen, unsigned char *key, unsigned char *iv, unsigned char *outBuf);

#endif // CRYPTO_H
