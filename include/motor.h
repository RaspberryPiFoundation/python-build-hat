/* motor.h
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
 * Motor operations on a port
 */

#ifndef RPI_STRAWBERRY_MOTOR_H_INCLUDED
#define RPI_STRAWBERRY_MOTOR_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>
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

/* Read the position of the motor relative to the preset mark */
extern int motor_get_position(PyObject *self, long *pposition);

/* Set the "preset" marker position relative to zero */
extern int motor_set_preset(PyObject *self, long position);

/* Updates the "preset" marker (for MotorPair.preset()) */
extern void motor_update_preset(PyObject *self, long position);

/* Ensure foregrounded initialisation is done (for MotorPairs) */
extern int motor_ensure_mode_info(PyObject *self);

/* Get the offset to use when determining absolute motor positions */
/* NB: the mode information must have been acquired at some point
 * before calling this.  This is taken care of at init time for
 * MotorPairs, but if you call this function from any other situation
 * you should first call motor_ensure_mode_info()
 */
extern int32_t motor_get_position_offset(PyObject *self);

#endif /* RPI_STRAWBERRY_MOTOR_H_INCLUDED */
