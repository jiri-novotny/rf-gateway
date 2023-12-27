#ifndef CRC_H_
#define CRC_H_

#include <stdint.h>

uint16_t crc_ccitt(uint8_t const *buffer, int len);

#endif /* CRC_H_ */
