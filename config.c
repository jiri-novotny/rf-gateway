#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "jsmn.h"
#include "base64.h"
#include "list.h"
#include "rf.h"

static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0)
  {
    return 0;
  }
  return -1;
}

uint16_t configParse(char *path, char **interface, RfDevice_t *gw)
{
  int i;
  int r;
  size_t len;
  uint16_t version = 0;
  uint8_t ingw = 0;
  uint8_t indev = 0;
  FILE *fdConf;
  char *config;
  RfDevice_t *rd;
  unsigned char *tmp;
  jsmn_parser jp;
  jsmntok_t tok[128];

  fdConf = fopen(path, "r");
  if (fdConf)
  {
    fseek(fdConf, 0, SEEK_END);
    len = ftell(fdConf);
    rewind(fdConf);
    config = (char *) malloc(len);
    if (config)
    {    
      fread(config, 1, len, fdConf);
      fclose(fdConf);
    }
    else
    {
      fclose(fdConf);
      return 2;
    }
  }
  else
  {
    return 1;
  }

  jsmn_init(&jp);
  r = jsmn_parse(&jp, config, len, tok, sizeof(tok) / sizeof(tok[0]));
  for (i = 1; i < r; i++)
  {
    if (ingw)
    {
      ingw--;
    }
    
    if (indev && (tok[i].type == JSMN_OBJECT || i == (r - 1)))
    {
      rd->ctr = 0;
      rd->packetQueue = list_create();
      rfAddDevice(rd);
      indev--;
      if (indev) rd = (RfDevice_t *) calloc(1, sizeof(RfDevice_t));
    }

    if (jsoneq(config, &tok[i], "version") == 0)
    {
      i++;
      tmp = (unsigned char *) strndup(config + tok[i].start, tok[i].end - tok[i].start);
      sscanf((char *) tmp, "%hu", &version);
      free(tmp);
    }
    else if (jsoneq(config, &tok[i], "gateway") == 0)
    {
      i++;
      ingw = tok[i].size + 1;
      rd = gw;
    }
    else if (jsoneq(config, &tok[i], "interface") == 0)
    {
      i++;
      *interface = strndup(config + tok[i].start, tok[i].end - tok[i].start);
    }
    else if (jsoneq(config, &tok[i], "id") == 0)
    {
      i++;
      tmp = (unsigned char *) strndup(config + tok[i].start, tok[i].end - tok[i].start);
      sscanf((char *) tmp, "%u", &rd->sn);
      free(tmp);
    }
    else if (jsoneq(config, &tok[i], "bcast") == 0)
    {
      i++;
      tmp = (unsigned char *) strndup(config + tok[i].start, tok[i].end - tok[i].start);
      sscanf((char *) tmp, "%u", &rd->addr);
      free(tmp);
    }
    else if (jsoneq(config, &tok[i], "key") == 0)
    {
      i++;
      tmp = base64_decode((unsigned char *) config + tok[i].start, tok[i].end - tok[i].start, &len);
      if (tmp)
      {
        if (len >= 32)
        {
          memcpy(rd->key, tmp, 16);
          memcpy(rd->iv, tmp + 16, 16);
        }
        else
        {
          fprintf(stderr, "Invalid key length - using default!\n");
          memset(rd->key, 0xff, 16);
          memset(rd->iv, 0x00, 16);
        }
        free(tmp);
      }
    }
    else if (jsoneq(config, &tok[i], "devices") == 0)
    {
      rd = (RfDevice_t *) calloc(1, sizeof(RfDevice_t));
      /* store size of array */
      indev = tok[i + 1].size;
      /* jump to first item */
      i += 2;
    }
  }
  free(config);

  if (version != SUPPORTED_VERSION)
  {
    fprintf(stderr, "Unsupported config version\n");
    if (*interface) free(*interface);
    return 1;
  }

  return 0;
}