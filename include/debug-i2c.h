/* debug-i2c.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Debug logging for the I2C module
 */

#ifndef RPI_STRAWBERRY_DEBUG_I2C_H_INCLUDED
#define RPI_STRAWBERRY_DEBUG_I2C_H_INCLUDED

#include <stdint.h>

extern int log_i2c_init(void);
extern void log_i2c(uint8_t *buffer, int direction);
extern void log_i2c_dump(void);

#endif /* RPI_STRAWBERRY_DEBUG_I2C_H_INCLUDED */
