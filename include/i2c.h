/* i2c.h
 *
 * Copyright (c) Kynesim Ltd, 2020
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

#endif /* RPI_STRAWBERRY_I2C_H_INCLUDED */
