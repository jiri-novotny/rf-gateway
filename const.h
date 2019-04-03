#ifndef CONST_H
#define CONST_H

/* global constants and config */
#define MAX_PACKET_LEN              36

#define I_DST                       0
#define I_SRC                       4
#define I_RAND                      8
#define I_CTR                       9
#define I_FLAGS                     10
#define I_CMD                       11
#define I_PAYLOAD                   12
#define I_CRC                       34

/**
 * Get byte length of the register
 */
#define REG_BYTE_LEN_ID(id)           (REG_LENGTH[((id) & 0x0007)])

/* time helpers */
#define timspecadd(s,t,a) (void) ( (a)->tv_sec = (s)->tv_sec + (t)->tv_sec, \
  ((a)->tv_nsec = (s)->tv_nsec + (t)->tv_nsec) >= 1000000000 && \
  ((a)->tv_nsec -= 1000000000, (a)->tv_sec++) )

#define timspecdiff(s,t,a) (void) ( (a)->tv_sec = (s)->tv_sec - (t)->tv_sec, \
  ((a)->tv_nsec = (s)->tv_nsec - (t)->tv_nsec) < 0 && \
  ((a)->tv_nsec += 1000000000, (a)->tv_sec--) )

/* Public functions */
void writeLog(char *logline, ...);

#endif // CONST_H
