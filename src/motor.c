/* motor.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Python class for handling a port's "motor" attribute
 * and attached motors.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>

#include "motor.h"
#include "port.h"


/* The actual Motor type */
typedef struct
{
    PyObject_HEAD
    PyObject *port;
    PyObject *device;
} MotorObject;


static int
Motor_traverse(MotorObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->port);
    Py_VISIT(self->device);
    return 0;
}


static int
Motor_clear(MotorObject *self)
{
    Py_CLEAR(self->port);
    Py_CLEAR(self->device);
    return 0;
}


static void
Motor_dealloc(MotorObject *self)
{
    PyObject_GC_UnTrack(self);
    Motor_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *
Motor_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MotorObject *self = (MotorObject *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->port = Py_None;
        Py_INCREF(Py_None);
        self->device = Py_None;
        Py_INCREF(Py_None);
    }
    return (PyObject *)self;
}


static int
Motor_init(MotorObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "port", "device" };
    PyObject *port = NULL;
    PyObject *device = NULL;
    PyObject *tmp;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist, &port, &device))
        return -1;

    tmp = self->port;
    Py_INCREF(port);
    self->port = port;
    Py_XDECREF(tmp);

    tmp = self->device;
    self->device = device;
    Py_XDECREF(tmp);
    return 0;
}


static PyObject *
Motor_repr(PyObject *self)
{
    MotorObject *motor = (MotorObject *)self;
    int port_id = port_get_id(motor->port);

    return PyUnicode_FromFormat("Motor(%c)", 'A' + port_id);
}


static PyObject *
Motor_get(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *get_fn = PyObject_GetAttrString(motor->device, "get");
    PyObject *result;

    if (get_fn == NULL)
        return NULL;
    result = PyObject_CallObject(get_fn, args);
    Py_DECREF(get_fn);
    return result;
}


static PyObject *
Motor_mode(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *mode_fn = PyObject_GetAttrString(motor->device, "mode");
    PyObject *result;

    if (mode_fn == NULL)
        return NULL;
    result = PyObject_CallObject(mode_fn, args);
    Py_DECREF(mode_fn);
    return result;
}


static PyObject *
Motor_pwm(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *pwm_fn = PyObject_GetAttrString(motor->port, "pwm");
    PyObject *result;

    if (pwm_fn == NULL)
        return NULL;
    result = PyObject_CallObject(pwm_fn, args);
    Py_DECREF(pwm_fn);
    return result;
}


static PyObject *
Motor_float(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;

    /* float() is equivalent to pwm(0) */

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    return PyObject_CallMethod(motor->port, "pwm", "i", 0);
}


static PyObject *
Motor_brake(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;

    /* brake() is equivalent to pwm(127) */

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    return PyObject_CallMethod(motor->port, "pwm", "i", 127);
}


static PyMethodDef Motor_methods[] = {
    { "mode", Motor_mode, METH_VARARGS, "Get or set the current mode" },
    { "get", Motor_get, METH_VARARGS, "Get a set of readings from the motor" },
    { "pwm", Motor_pwm, METH_VARARGS, "Set the PWM level for the motor" },
    {
        "float", Motor_float, METH_VARARGS,
        "Force the motor driver to floating state"
    },
    {
        "brake", Motor_brake, METH_VARARGS,
        "Force the motor driver to brake state"
    },
    { NULL, NULL, 0, NULL }
};


static PyTypeObject MotorType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Motor",
    .tp_doc = "An attached motor",
    .tp_basicsize = sizeof(MotorObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = Motor_new,
    .tp_init = (initproc)Motor_init,
    .tp_dealloc = (destructor)Motor_dealloc,
    .tp_traverse = (traverseproc)Motor_traverse,
    .tp_clear = (inquiry)Motor_clear,
    .tp_methods = Motor_methods,
    .tp_repr = Motor_repr
};


int motor_modinit(void)
{
    if (PyType_Ready(&MotorType) < 0)
        return -1;
    Py_INCREF(&MotorType);
    return 0;
}


void motor_demodinit(void)
{
    Py_DECREF(&MotorType);
}


PyObject *motor_new_motor(PyObject *port, PyObject *device)
{
    PyObject *args = Py_BuildValue("(OO)", port, device);

    if (args == NULL)
        return NULL;
    return PyObject_CallObject((PyObject *)&MotorType, args);
}
