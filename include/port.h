/* port.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Constants and types for device ports of a hat
 */

#ifndef RPI_STRAWBERRY_PORT_H_INCLUDED
#define RPI_STRAWBERRY_PORT_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>


/* The "port" object itself just contains six slots for devices */

typedef struct
{
    PyObject_HEAD
    PyObject *A;
    PyObject *B;
    PyObject *C;
    PyObject *D;
    PyObject *E;
    PyObject *F;
} PortObject;

extern int port_init(PyObject *hub);
extern void port_deinit(void);


#endif /* RPI_STRAWBERRY_PORT_H_INCLUDED */
