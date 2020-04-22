/* debug-i2c.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Debug logging for the I2C module
 */

#ifndef RPI_STRAWBERRY_DEBUG_I2C_H_INCLUDED
#define RPI_STRAWBERRY_DEBUG_I2C_H_INCLUDED

#ifdef DEBUG_I2C

#include <stdint.h>

extern int log_i2c_init(void);
extern void log_i2c(uint8_t *buffer, int direction);
extern void log_i2c_dump(void);

#define DBG_MODULE_I2C    0xff
#define DBG_MODULE_QUEUE  0xfe
#define DBG_MODULE_PORT   0xfd

#define DBG_REASON_QUEUE_GET_LOCK_FAILED        0x00
#define DBG_REASON_QUEUE_GET_LOOP_UNLOCK_FAILED 0x01
#define DBG_REASON_QUEUE_GET_POLL_FAILED        0x02
#define DBG_REASON_QUEUE_GET_LOOP_LOCK_FAILED   0x03
#define DBG_REASON_QUEUE_GET_UNLOCK_FAILED      0x04
#define DBG_REASON_QUEUE_GET_LOCKING            0x05
#define DBG_REASON_QUEUE_GET_POLLING            0x06
#define DBG_REASON_QUEUE_GET_RELOCKING          0x07
#define DBG_REASON_QUEUE_GET_UNLOCKING          0x08
#define DBG_REASON_QUEUE_RETURNING_BUFFER       0x09
#define DBG_REASON_QUEUE_RETURNED_BUFFER        0x0a

#define DBG_REASON_I2C_WAIT_FOR_RX   0x00
#define DBG_REASON_I2C_WAIT_DONE     0x01
#define DBG_REASON_I2C_START_IOCTL   0x02
#define DBG_REASON_I2C_READ_LEN      0x03
#define DBG_REASON_I2C_READ_LEN_2    0x04
#define DBG_REASON_I2C_READ_BODY     0x05
#define DBG_REASON_I2C_READ_DONE     0x06
#define DBG_REASON_I2C_TX_IOCTL_DONE 0x07
#define DBG_REASON_I2C_TX_DONE       0x08
#define DBG_REASON_I2C_CHECK_GPIO    0x09

#define DBG_REASON_PORT_CLAIM_GIL       0x00
#define DBG_REASON_PORT_CLAIMED_GIL     0x01
#define DBG_REASON_PORT_NEW_DEVICE      0x02
#define DBG_REASON_PORT_RELEASED_GIL    0x03
#define DBG_REASON_PORT_NV_CLAIM_GIL    0x04
#define DBG_REASON_PORT_NV_CLAIMED_GIL  0x05
#define DBG_REASON_PORT_NV_RELEASED_GIL 0x06

#define DEBUG0(m,r) do { \
    uint8_t debug[3] = { 3 }; \
    debug[1] = DBG_MODULE_ ## m; \
    debug[2] = DBG_REASON_ ## m ## _ ## r; \
    log_i2c(debug, -1); \
    } while (0)

#define DEBUG1(m,r, p0) do {     \
    uint8_t debug[4] = { 4 }; \
    debug[1] = DBG_MODULE_ ## m; \
    debug[2] = DBG_REASON_ ## m ## _ ## r; \
    debug[3] = p0; \
    log_i2c(debug, -1); \
    } while (0)

#else /* DEBUG_I2C */

#define DEBUG0(m,r)
#define DEBUG1(m,r,p0)

#endif /* DEBUG_I2C */


#endif /* RPI_STRAWBERRY_DEBUG_I2C_H_INCLUDED */
