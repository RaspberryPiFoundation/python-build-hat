/* motor.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Motor operations on a port
 */

#ifndef RPI_STRAWBERRY_MOTOR_H_INCLUDED
#define RPI_STRAWBERRY_MOTOR_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "device.h"

extern int motor_modinit(void);
extern void motor_demodinit(void);

/* Creates a new Motor class object for attaching to a port */
extern PyObject *motor_new_motor(PyObject *port, PyObject *device);

/* Fire the Python-level callback function, if one is registered */
/* Called from the callback thread */
extern int motor_callback(PyObject *self, int event);

/* Mark the motor object as detached from the port */
extern void motor_detach(PyObject *self);

/* Check for the device type being one of the formally recognised motors */
#define motor_is_motor(dt)                     \
    ((dt) == ID_MOTOR_MEDIUM            ||     \
     (dt) == ID_MOTOR_LARGE             ||     \
     (dt) == ID_MOTOR_SMALL             ||     \
     (dt) == ID_STONE_GREY_MOTOR_MEDIUM ||     \
     (dt) == ID_STONE_GREY_MOTOR_LARGE)

#endif /* RPI_STRAWBERRY_MOTOR_H_INCLUDED */
