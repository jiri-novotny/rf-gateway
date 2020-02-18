#ifndef CONFIG_H
#define CONFIG_H

#include "rf.h"

#define SUPPORTED_VERSION       1

uint16_t configParse(char *path, char *interface, RfDevice_t *gw);

#endif // CONFIG_H
