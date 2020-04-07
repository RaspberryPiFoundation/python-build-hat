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

extern int motor_modinit(void);
extern void motor_demodinit(void);

/* Creates a new Motor class object for attaching to a port */
extern PyObject *motor_new_motor(PyObject *port, PyObject *device);

/* Fire the Python-level callback function, if one is registered */
extern int motor_callback(PyObject *self, int event);

/* Mark the motor object as detached from the port */
extern void motor_detach(PyObject *self);

/* Check for the device type being Motor (0001), System Train Motor (0002),
 * External Motor With Tacho (0026), Internal Motor With Tacho (0027),
 * Large Tacho Motor (002e), Extra Large Tacho Motor (002f), Medium
 * Angular Motor (0030), Large Angular Motor (0031) or Micro Angular
 * Motor (0041).  The latter are listed in code rather than current
 * documentation, unfortunately.
 */
#define motor_is_motor(dt) \
    ((dt) == 0x0001 ||     \
     (dt) == 0x0002 ||     \
     (dt) == 0x0026 ||     \
     (dt) == 0x0027 ||     \
     (dt) == 0x002e ||     \
     (dt) == 0x002f ||     \
     (dt) == 0x0030 ||     \
     (dt) == 0x0031 ||     \
     (dt) == 0x0041)

#endif /* RPI_STRAWBERRY_MOTOR_H_INCLUDED */
