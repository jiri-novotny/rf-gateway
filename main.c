#include <getopt.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "const.h"
#include "eventloop.h"
#include "hashmap.h"
#include "jsmn.h"
#include "log.h"
#include "rfserver.h"
#include "signals.h"
#include "tcpserver.h"
#include "timer.h"
#include "websocket.h"

/* Defines */
#ifndef COMMIT_VERSION
#define COMMIT_VERSION "Unkonwn"
#endif

#ifndef COMMIT_HASH
#define COMMIT_HASH "Unkonwn"
#endif

#ifndef PROG_NAME
#define PROG_NAME "rbus-unknown"
#endif

#define WS_PING_TIMEOUT 45 /* [s] */

/* Typedefs */
typedef struct
{
  int websock;
  int ysock;
  int tfd;
  uint32_t hwAddress;
  uint8_t session;
} clientCtx_t;

typedef struct
{
  uint8_t logLevel;
  struct hashmap *clients;
  int clientsLen;
  uint16_t tcpServerPort;
  char *rfIface;
  uint32_t rfSn;
} appCtx_t;

/* Module globals */
static globalCtx_t gc;
static appCtx_t app;
static struct option opt[] = {
    {"help",        0, NULL, 'h'},
    {"version",     0, NULL, 'V'},
    {"hash",        0, NULL, 'H'},
    {"log",         0, NULL, 'l'},
    {"server-port", 1, NULL, 'p'},
    {NULL,          0, NULL, 0  }
};

static uint8_t key[16] = {0x93, 0x1E, 0xA4, 0xA5, 0x2D, 0xDC, 0x02, 0x9C,
                          0xB7, 0x9E, 0x84, 0x94, 0x18, 0x52, 0xF1, 0x79};
static uint8_t iv[16] = {0x5A, 0x7C, 0xD6, 0xC6, 0x6B, 0xB2, 0x35, 0x2B,
                         0x7A, 0x67, 0x42, 0xC6, 0x4A, 0xB6, 0x7A, 0x8E};

/* Private functions */
static void printUsage(char *name, int exitcode)
{
  printf("USAGE: %s [options]\nOptions:\n"
         "\t-h/--help\t\tPrint this help\n"
         "\t-V/--version\t\tPrint version\n"
         "\t-H/--hash\t\tPrint commit hash\n"
         "\t-l/--log\t\tUse syslog for logging\n"
         "\t-p/--server-port\t\tWS server port\n"
         "\t-s/--serial-number\t\tRF serial number\n",
         name);
  logDeinit();
  exit(exitcode);
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
  if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0)
  {
    return 0;
  }
  return -1;
}

static void wsSend(clientCtx_t *cc, uint8_t type, char *response)
{
  ssize_t len = 0;
  if (response) len = strlen(response);
  websocketSend(cc->websock, type, (uint8_t *) response, len);
  timerSet(cc->tfd, WS_PING_TIMEOUT, 0);
}

static void rfOnData(int16_t rssi, uint8_t *data, uint32_t len)
{
  writeLog(LOG_DEBUG, "---------\n");
  writeLog(LOG_DEBUG, "DEV: %08x RSSI: %d\n", *(uint32_t *) (data + 4), rssi);
  dumpData(LOG_DEBUG, "PKT: ", data + 9, len - 11, (len > 43) ? 1 : 0);
}

/* Public functions */
int main(int argc, char **argv)
{
  int optval;
  int i = 1;

  /* clear and init defaults */
  memset(&gc, 0, sizeof(globalCtx_t));
  app.logLevel = LOG_DEFAULT;
  app.rfIface = "rf0";
  app.rfSn = 0xf0000001;
  app.tcpServerPort = 8080;
  logInit(argv[0], 0);

  while ((optval = getopt_long(argc, argv, "hVHlp:s:", opt, NULL)) != -1)
  {
    switch (optval)
    {
      case 'l':
        logInit(PROG_NAME, 1);
        break;

      case 'p':
        app.tcpServerPort = (uint16_t) strtoul(optarg, NULL, 10);
        break;

      case 's':
        app.rfSn = (uint32_t) strtoul(optarg, NULL, 16);
        break;

      case 'V':
        printf("%s\n", COMMIT_VERSION);
        exit(0);

      case 'H':
        printf("%s\n", COMMIT_HASH);
        exit(0);

      case 'h':
        i = 0;
        /* fallthrough */
      default:
        printUsage(argv[0], i);
    }
  }

  srand(time(NULL));
  eventloopInit(&gc);
  timerInit(&gc);

  /* INIT section */
  app.clientsLen = 0;
  app.clients = hashmap_create();
  if (app.clients == NULL)
  {
    writeLog(LOG_ERR, "client map failed\n");
    gc.finished = 1;
  }

  signalsInit(&gc);
  int r = rfServerInit(&gc, app.rfIface, app.rfSn, rfOnData);
  rfServerAdd(r, 0x001000d9, key, iv);
  rfServerAdd(r, 0x0050002e, key, iv);
  websocketInit(&gc);
  tcpServerInit(&gc, app.tcpServerPort);

  /* eventloop handler */
  writeLog(LOG_NOTICE, "%s running ...\n", PROG_NAME);
  while (0 == gc.finished)
  {
    eventloopRun();
  }

  /* DEINIT section */
  tcpServerDeinit();
  websocketDeinit();
  rfServerDeinit(r);
  signalsDeinit();

  if (app.clients)
  {
    struct iterator *entries;
    clientCtx_t *cc;

    entries = hashmap_iterator(app.clients);
    while (entries->next(entries))
    {
      cc = ((struct hentry *) entries->current)->value;
      free(cc);
    }
    entries->destroy(entries);
    hashmap_destroy(app.clients);
  }

  /* global deinit */
  logDeinit();
  timerDeinit();
  eventloopDeinit();
  writeLog(LOG_NOTICE, "%s finished\n", PROG_NAME);

  return 0;
}

/* Public callbacks */
static void clientTimer(void *arg)
{
  clientCtx_t *cc = (clientCtx_t *) arg;
  if (cc)
  {
    writeLog(LOG_DEBUG, "WS:  client %d timer\n", cc->websock);
    wsSend(cc, 9, NULL);
  }
}

/* Override */
void signalsSigint(void)
{
  gc.finished = 1;
}

void signalsSigusr1(void)
{
  if (app.logLevel == LOG_DEBUG) app.logLevel = LOG_INFO;
  else if (app.logLevel == LOG_INFO) app.logLevel = LOG_NOTICE;
  else if (app.logLevel == LOG_NOTICE) app.logLevel = LOG_WARNING;
  else app.logLevel = LOG_DEBUG;
  logSetLevel(app.logLevel);
}

void signalsSigusr2(void)
{
  if (app.logLevel == LOG_WARNING) app.logLevel = LOG_NOTICE;
  else if (app.logLevel == LOG_NOTICE) app.logLevel = LOG_INFO;
  else if (app.logLevel == LOG_INFO) app.logLevel = LOG_DEBUG;
  else app.logLevel = LOG_WARNING;
  logSetLevel(app.logLevel);
}

void websocketOnConnect(int sock)
{
  struct hkey hk = {&sock, sizeof(int)};

  clientCtx_t *cc = (clientCtx_t *) malloc(sizeof(clientCtx_t));
  if (cc)
  {
    cc->websock = sock;
    hashmap_set(app.clients, &hk, cc);
    cc->tfd = timerCreate("client", clientTimer, cc);
    if (cc->tfd == -1) writeLog(LOG_WARNING, "WS:  client timer fail\n");
    else timerSet(cc->tfd, WS_PING_TIMEOUT, 0);
    writeLog(LOG_NOTICE, "WS:  client %d connected\n", sock);
  }
  else
  {
    websocketSend(sock, 1, (uint8_t *) "{\"msg\":\"ws failed - ctx\"}", 25);
  }
}

void websocketOnDisconnect(int sock)
{
  struct hkey hk = {&sock, sizeof(int)};

  clientCtx_t *cc = hashmap_remove(app.clients, &hk);
  if (cc)
  {
    cc->websock = -1;
    hk.data = &cc->tfd;
    if (cc->tfd > 0) hashmap_remove(app.clients, &hk);
    else free(cc);
  }
  else writeLog(LOG_WARNING, "WS:  disconnect unknown ctx %d\n", sock);
}

void websocketOnData(int sock, uint8_t opcode, uint8_t *data, ssize_t len)
{
  (void) opcode;
  struct hkey hk = {&sock, sizeof(int)};
  char *tmp = (char *) data;
  jsmn_parser jp;
  jsmntok_t jt[128];
  int i;
  int r;
  uint8_t cmd = 0;
  char *payload = NULL;
  int payloadLen = 0;
  clientCtx_t *cc;

  jsmn_init(&jp);
  r = jsmn_parse(&jp, tmp, len, jt, 128);

  for (i = 1; i < r; i++)
  {
    if (jsoneq(tmp, &jt[i], "cmd") == 0)
    {
      if (jsoneq(tmp, &jt[i + 1], "open") == 0) cmd = 1;
    }
    else if (jsoneq(tmp, &jt[i], "data") == 0)
    {
      payloadLen = jt[i + 1].end - jt[i + 1].start;
      payload = &tmp[jt[i + 1].start];
    }
  }

  cc = hashmap_get(app.clients, &hk);
  if (cc)
  {
    
  }

  switch (cmd)
  {
    default:
      writeLog(LOG_WARNING, "WS:  invalid cmd %.*s\n", payloadLen, payload);
      break;
  }
}
