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

extern int device_init(void);
extern void device_deinit(void);

/* Create a new Device class object for attaching to a port */
extern PyObject *device_new_device(PyObject *port);

#endif /* RPI_STRAWBERRY_DEVICE_H_INCLUDED */
