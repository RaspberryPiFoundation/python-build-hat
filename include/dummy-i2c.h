/* dummy-i2c.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Functions for faking an I2C bus
 */

#ifndef RPI_STRAWBERRY_DUMMY_I2C_H_INCLUDED
#define RPI_STRAWBERRY_DUMMY_I2C_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

extern int open_dummy_i2c_socket(const char **perrstr);
extern int dummy_i2c_read(int fd, uint8_t *buffer, size_t nbytes);
extern int dummy_i2c_write(int fd, uint8_t *buffer, size_t nbytes);

#endif /* RPI_STRAWBERRY_DUMMY_I2C_H_INCLUDED */
