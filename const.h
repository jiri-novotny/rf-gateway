#ifndef CONST_H
#define CONST_H

/* global constants and config */
#define MAX_PACKET_LEN              256

#define I_DST                       0
#define I_SRC                       4
#define I_CTRL                      8
#define I_CTR                       9
#define I_RND                       10
#define I_CMD                       11
#define I_LEN                       12
#define I_DATA                      13

#define C_DISCOVER                  0x01 /* sink -> device */
#define C_DISCOVER_RESP             0x81
#define C_READ_REG                  0x02 /* sink -> device */
#define C_RW_RESP                   0x82
#define C_WRITE_REG                 0x03 /* sink -> device */
#define C_NOTIFY                    0x84
#define C_ACK                       0x04 /* sink -> device */
#define C_POLL                      0x85

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
