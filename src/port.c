/* port.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Python classes for handling ports
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include "port.h"


static int
Port_traverse(PortObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->A);
    Py_VISIT(self->B);
    Py_VISIT(self->C);
    Py_VISIT(self->D);
    Py_VISIT(self->E);
    Py_VISIT(self->F);
    return 0;
}


static int
Port_clear(PortObject *self)
{
    Py_CLEAR(self->A);
    Py_CLEAR(self->B);
    Py_CLEAR(self->C);
    Py_CLEAR(self->D);
    Py_CLEAR(self->E);
    Py_CLEAR(self->F);
    return 0;
}


static void
Port_dealloc(PortObject *self)
{
    PyObject_GC_UnTrack(self);
    Port_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *
Port_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PortObject *self;

    self = (PortObject *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->A =
            self->B =
            self->C =
            self->D =
            self->E =
            self->F = Py_None;
        /* Six increments, one for each entry */
        Py_INCREF(Py_None);
        Py_INCREF(Py_None);
        Py_INCREF(Py_None);
        Py_INCREF(Py_None);
        Py_INCREF(Py_None);
        Py_INCREF(Py_None);
    }
    return (PyObject *)self;
}


static PyMemberDef Port_members[] =
{
    { "A", T_OBJECT_EX, offsetof(PortObject, A), 0, "Port A" },
    { "B", T_OBJECT_EX, offsetof(PortObject, B), 0, "Port B" },
    { "C", T_OBJECT_EX, offsetof(PortObject, C), 0, "Port C" },
    { "D", T_OBJECT_EX, offsetof(PortObject, D), 0, "Port D" },
    { "E", T_OBJECT_EX, offsetof(PortObject, E), 0, "Port E" },
    { "F", T_OBJECT_EX, offsetof(PortObject, F), 0, "Port F" },
    { NULL }
};


static PyTypeObject PortType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "hub.Port",
    .tp_doc = "Container for the devices plugged in to the hat's ports",
    .tp_basicsize = sizeof(PortObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = Port_new,
    .tp_dealloc = (destructor)Port_dealloc,
    .tp_traverse = (traverseproc)Port_traverse,
    .tp_clear = (inquiry)Port_clear,
    .tp_members = Port_members
};


int port_init(PyObject *hub)
{
    PyObject *port;
    if (PyType_Ready(&PortType) < 0)
        return -1;

    Py_INCREF(&PortType);
#if 0
    if (PyModule_AddObject(hub, "Port", (PyObject *)&PortType) < 0)
    {
        Py_DECREF(&PortType);
        return -1;
    }
#endif

    port = PyObject_CallObject((PyObject *)&PortType, NULL);
    Py_DECREF(&PortType);
    if (port == NULL)
        return -1;
    if (PyModule_AddObject(hub, "port", port) < 0)
    {
        Py_DECREF(port);
        return -1;
    }
    return 0;
}


void port_deinit(void)
{
}
