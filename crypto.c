#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "const.h"

int encryptPacket(unsigned char *inBuf, int inLen, unsigned char *key, unsigned char *iv, unsigned char *outBuf)
{
  int len;
  int outLen;
  EVP_CIPHER_CTX *ctx;

  ctx = EVP_CIPHER_CTX_new();

  if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv))
  {
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    if (EVP_EncryptUpdate(ctx, outBuf, &len, inBuf, inLen))
    {
      outLen = len;
      if (EVP_EncryptFinal_ex(ctx, outBuf + len, &len))
      {
        outLen += len;
      }
      else
      {
        outLen = 0;
      }
    }
    else
    {
      outLen = 0;
    }
  }
  else
  {
    outLen = 0;
  }

  /* Clean up */
  EVP_CIPHER_CTX_free(ctx);

  return outLen;
}

int decryptPacket(unsigned char *inBuf, int inLen, unsigned char *key, unsigned char *iv, unsigned char *outBuf)
{
  int len;
  int outLen;
  EVP_CIPHER_CTX *ctx;

  ctx = EVP_CIPHER_CTX_new();

  if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv))
  {
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    if (EVP_DecryptUpdate(ctx, outBuf, &len, inBuf, inLen))
    {
      outLen = len;
      if (EVP_DecryptFinal_ex(ctx, outBuf + len, &len))
      {
        outLen += len;
      }
      else
      {
        outLen = 0;
      }
    }
    else
    {
      outLen = 0;
    }
  }
  else
  {
    outLen = 0;
  }

  EVP_CIPHER_CTX_free(ctx);

  return outLen;
}
