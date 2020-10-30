/* i2c.h
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
 * Functions for handling I^2C comms
 */

#ifndef RPI_STRAWBERRY_I2C_H_INCLUDED
#define RPI_STRAWBERRY_I2C_H_INCLUDED

/* Initialise the I2C subsystem, creating the background thread that
 * communicates with Shortcake.  Returns the file descriptor of the
 * (open) I2C bus, or -1 on error.  On failure, a Python exception
 * will have been raised.
 */
extern int i2c_open_hat(void);

/* Finalise the I2C subsystem, closing the file descriptor.  Returns 0
 * for success, -1 on failure.  If the close call fails, an exception
 * will be raised.
 */
extern int i2c_close_hat(void);

/* Register the firmware object with the I2C subsystem so it knows
 * where to direct firmware callbacks
 */
extern void i2c_register_firmware_object(PyObject *firmware);

/* Pulses the reset line to the HAT, with the BOOT0 pin held low to
 * boot into firmware rather than the embedded bootloader.  Note that
 * this is AN EXTREMELY DANGEROUS THING TO DO: you must be sure no I2C
 * communications are in progress, otherwise the library's comms will
 * break.
 */
extern int i2c_reset_hat(void);

/* Checks to see if there has been an error on the comms background
 * threads.  Returns 0 if all is well, otherwise returns -1 and raises
 * an appropriate exception.
 */
extern int i2c_check_comms_error(void);


#endif /* RPI_STRAWBERRY_I2C_H_INCLUDED */
