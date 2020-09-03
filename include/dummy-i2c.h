/* dummy-i2c.h
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Limited
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *     Copyright (c) 2020 Kynesim Ltd
 *     Copyright (c) 2017-2020 LEGO System A/S
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
