/* pair.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Python class for handling a pair of motors
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>

#include "pair.h"
#include "port.h"
#include "cmd.h"


#define INVALID_ID (0xff)


/* The actual motor pair type */
typedef struct
{
    PyObject_HEAD
    PyObject *primary;
    PyObject *secondary;
    PyObject *callback_fn;
    uint32_t default_position_pid[3];
    uint8_t id;
    uint8_t primary_id;
    uint8_t secondary_id;
    uint16_t device_type;
} MotorPairObject;


#define PAIR_COUNT 6

MotorPairObject *pairs[PAIR_COUNT] = { NULL };


static int
MotorPair_traverse(MotorPairObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->primary);
    Py_VISIT(self->secondary);
    return 0;
}


static int
MotorPair_clear(MotorPairObject *self)
{
    Py_CLEAR(self->primary);
    Py_CLEAR(self->secondary);
    return 0;
}


static void
MotorPair_dealloc(MotorPairObject *self)
{
    PyObject_GC_UnTrack(self);
    MotorPair_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *
MotorPair_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MotorPairObject *self;
    int i;

    /* Find an empty slot */
    for (i = 0; i < PAIR_COUNT; i++)
        if (pairs[i] == NULL)
            break;
    if (i == PAIR_COUNT)
    {
        /* No available slots.  Abort */
        PyErr_SetString(cmd_get_exception(), "Too many motor pairs");
        return NULL;
    }

    if ((self = (MotorPairObject *)type->tp_alloc(type, 0)) != NULL)
    {
        Py_INCREF(self);
        pairs[i] = self;
        self->primary = Py_None;
        Py_INCREF(Py_None);
        self->secondary = Py_None;
        Py_INCREF(Py_None);
        self->callback_fn = Py_None;
        Py_INCREF(Py_None);
        self->id =
            self->primary_id =
            self->secondary_id = INVALID_ID;
        self->default_position_pid[0] =
            self->default_position_pid[1] =
            self->default_position_pid[2] = 0;
    }
    return (PyObject *)self;
}


static int
MotorPair_init(MotorPairObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "primary", "secondary", NULL };
    PyObject *primary = NULL;
    PyObject *secondary = NULL;
    PyObject *tmp;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist,
                                     &primary, &secondary))
        return -1;

    tmp = self->primary;
    Py_INCREF(primary);
    self->primary = primary;
    Py_XDECREF(tmp);

    tmp = self->secondary;
    Py_INCREF(secondary);
    self->secondary = secondary;
    Py_XDECREF(tmp);

    self->primary_id = port_get_id(primary);
    self->secondary_id = port_get_id(secondary);

    if (cmd_connect_virtual_port(self->primary_id, self->secondary_id) < 0)
        return -1;

    return 0;
}


static PyObject *
MotorPair_repr(PyObject *self)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    return PyUnicode_FromFormat("MotorPair(%d:%c%c)",
                                pair->id,
                                'A' + pair->primary_id,
                                'A' + pair->secondary_id);
}


static PyObject *
MotorPair_primary(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    Py_XINCREF(pair->primary);
    return pair->primary;
}


static PyObject *
MotorPair_secondary(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    Py_XINCREF(pair->secondary);
    return pair->secondary;
}


static PyObject *
MotorPair_id(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    return PyLong_FromUnsignedLong(pair->id);
}


static PyObject *
MotorPair_callback(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    PyObject *callable = NULL;

    if (!PyArg_ParseTuple(args, "|O:callback", &callable))
        return NULL;

    if (callable == NULL)
    {
        /* Just wants the current callback returned */
        Py_INCREF(pair->callback_fn);
        return pair->callback_fn;
    }
    if (callable != Py_None && !PyCallable_Check(callable))
    {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    Py_XINCREF(callable);
    Py_XDECREF(pair->callback_fn);
    pair->callback_fn = callable;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_unpair(PyObject *self, PyObject *args)
{
    int rv;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    if ((rv = pair_unpair(self)) < 0)
        return NULL;
    else if (rv > 0)
        Py_RETURN_FALSE;

    Py_RETURN_TRUE;
}


static PyObject *
MotorPair_pid(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    /* If we have no parameters, return a tuple of the values */
    if (PyTuple_Size(args) == 0)
    {
        return Py_BuildValue("III",
                             pair->default_position_pid[0],
                             pair->default_position_pid[1],
                             pair->default_position_pid[2]);
    }

    /* Otherwise we should have all three values, and set them */
    if (!PyArg_ParseTuple(args, "III:pid",
                          &pair->default_position_pid[0],
                          &pair->default_position_pid[1],
                          &pair->default_position_pid[2]))
        return NULL;

    if (cmd_set_pid(pair->id, pair->default_position_pid) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_float(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    /* float() is equivalent to pwm(0) */

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    if (cmd_set_pwm_pair(pair->id, 0, 0) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_brake(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    /* brake() is equivalent to pwm(127) */

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    if (cmd_set_pwm_pair(pair->id, 127, 127) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyMethodDef MotorPair_methods[] = {
    {
        "primary", MotorPair_primary, METH_VARARGS,
        "Returns the primary port"
    },
    {
        "secondary", MotorPair_secondary, METH_VARARGS,
        "Returns the secondary port"
    },
    {
        "id", MotorPair_id, METH_VARARGS,
        "Returns the ID of the port pair"
    },
    {
        "callback", MotorPair_callback, METH_VARARGS,
        "Sets or returns the current callback function"
    },
    {
        "unpair", MotorPair_unpair, METH_VARARGS,
        "Unpair the motors.  The object is no longer valid"
    },
    {
        "pid", MotorPair_pid, METH_VARARGS,
        "Set the default P, I, D values"
    },
    {
        "float", MotorPair_float, METH_VARARGS,
        "Force the motor drivers to floating state"
    },
    {
        "brake", MotorPair_brake, METH_VARARGS,
        "Force the motor drivers to break state"
    },
    { NULL, NULL, 0, NULL }
};


static PyTypeObject MotorPairType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "MotorPair",
    .tp_doc = "A pair of attached motors",
    .tp_basicsize = sizeof(MotorPairObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = MotorPair_new,
    .tp_init = (initproc)MotorPair_init,
    .tp_dealloc = (destructor)MotorPair_dealloc,
    .tp_traverse = (traverseproc)MotorPair_traverse,
    .tp_clear = (inquiry)MotorPair_clear,
    .tp_methods = MotorPair_methods,
    .tp_repr = MotorPair_repr
};


int pair_modinit(void)
{
    if (PyType_Ready(&MotorPairType) < 0)
        return -1;
    Py_INCREF(&MotorPairType);
    return 0;
}


void pair_demodinit(void)
{
    Py_DECREF(&MotorPairType);
}


PyObject *pair_get_pair(PyObject *primary, PyObject *secondary)
{
    int i;
    uint8_t primary_id = port_get_id(primary);
    uint8_t secondary_id = port_get_id(secondary);

    /* First attempt to find an existing pair with these ports */
    for (i = 0; i < PAIR_COUNT; i++)
    {
        if (pairs[i] != NULL &&
            ((pairs[i]->primary_id == primary_id &&
              pairs[i]->secondary_id == secondary_id) ||
             (pairs[i]->primary_id == secondary_id &&
              pairs[i]->secondary_id == primary_id)))
        {
            return (PyObject *)pairs[i];
        }
    }
    return PyObject_CallFunctionObjArgs((PyObject *)&MotorPairType,
                                        primary, secondary, NULL);
}


int pair_is_ready(PyObject *self)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    return (pair->id != INVALID_ID);
}


int pair_attach_port(uint8_t id,
                     uint8_t primary_id,
                     uint8_t secondary_id,
                     uint16_t device_type)
{
    int i;

    for (i = 0; i < PAIR_COUNT; i++)
    {
        if (pairs[i] != NULL &&
            pairs[i]->primary_id == primary_id &&
            pairs[i]->secondary_id == secondary_id)
        {
            pairs[i]->id = id;
            pairs[i]->device_type = device_type;
            return 0;
        }
    }

    /* Somehow this is not a pair we know about */
    return -1;
}


static MotorPairObject *find_pair(uint8_t id)
{
    int i;

    for (i = 0; i < PAIR_COUNT; i++)
        if (pairs[i] != NULL && pairs[i]->id == id)
            return pairs[i];

    return NULL;
}


int pair_detach_port(uint8_t id)
{
    MotorPairObject *pair = find_pair(id);

    if (pair != NULL)
        pair->id = INVALID_ID;

    /* Most likely any error is caused by timing out an unpair command */
    return 0;
}


int pair_detach_subport(uint8_t id)
{
    int i;

    for (i = 0; i < PAIR_COUNT; i++)
    {
        if (pairs[i] != NULL &&
            (pairs[i]->primary_id == id ||
             pairs[i]->secondary_id == id))
        {
            /* Ignore errors for now */
            cmd_disconnect_virtual_port(pairs[i]->id, 1);
        }
    }
    return 0;
}


int pair_unpair(PyObject *self)
{
    int i;
    MotorPairObject *pair = (MotorPairObject *)self;
    clock_t start;

    for (i = 0; i < PAIR_COUNT; i++)
    {
        if (pairs[i] == pair)
        {
            break;
        }
    }
    if (i == PAIR_COUNT)
        return -1;

    if (pair->id != INVALID_ID)
        if (cmd_disconnect_virtual_port(pair->id, 0) < 0)
            return -1;

    /* Wait for ID to become invalid */
    start = clock();
    while (pair->id != INVALID_ID)
    {
        if (clock() - start > CLOCKS_PER_SEC)
        {
            /* Timeout */
            pairs[i] = NULL;
            return 1;
        }
    }

    pairs[i] = NULL;
    Py_DECREF(pair);
    return 0;
}


int pair_feedback_status(uint8_t port_id, uint8_t status)
{
    MotorPairObject *pair = find_pair(port_id);

    if (pair == NULL)
        return -1;

    /* Turns out we don't track busy here */
    return 0;
}
