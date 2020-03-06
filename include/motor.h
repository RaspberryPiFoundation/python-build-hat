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
extern PyObject *motor_new_motor(PyObject *port);

/* Check for the device type being Motor (0001), System Train Motor (0002),
 * External Motor With Tacho (0026) or Internal Motor With Tacho (0027)
 */
#define motor_is_motor(dt) \
    ((dt) == 0x0001 ||     \
     (dt) == 0x0002 ||     \
     (dt) == 0x0026 ||     \
     (dt) == 0x0027)

#endif /* RPI_STRAWBERRY_MOTOR_H_INCLUDED */
