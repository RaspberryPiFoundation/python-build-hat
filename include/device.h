/* device.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Basic device operations on a port
 */

#ifndef RPI_STRAWBERRY_DEVICE_H_INCLUDED
#define RPI_STRAWBERRY_DEVICE_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

extern int device_modinit(void);
extern void device_demodinit(void);

/* Create a new Device class object for attaching to a port */
extern PyObject *device_new_device(PyObject *port);

/* Parse an input buffer and update the stored values.  Returns the number
 * of bytes in the buffer consumed, or -1 on failure.
 */
extern int device_new_value(PyObject *self, uint8_t *buffer, uint16_t nbytes);

#endif /* RPI_STRAWBERRY_DEVICE_H_INCLUDED */
