/* port.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Python classes for handling ports
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>

#include "port.h"
#include "cmd.h"


/* The actual Port type */

typedef struct
{
    PyObject_HEAD
    PyObject *device;
    PyObject *callback;
    PyObject *hw_revision;
    PyObject *fw_revision;
    uint16_t type_id;
    /* XXX: etc */
} PortObject;


static int Port_traverse(PortObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->device);
    return 0;
}


static int
Port_clear(PortObject *self)
{
    Py_CLEAR(self->device);
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
        self->device = Py_None;
        Py_INCREF(Py_None);
        self->callback = Py_None;
        Py_INCREF(Py_None);
    }
    return (PyObject *)self;
}


static PyMemberDef Port_members[] =
{
    {
        "device", T_OBJECT_EX, offsetof(PortObject, device), 0,
        "Generic device handler"
    },
    { NULL }
};


static int
Port_set_callback(PortObject *self, PyObject *value, void *closure)
{
    PyObject *temp;

    if (!PyCallable_Check(value))
    {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return -1;
    }

    temp = self->callback;
    Py_XINCREF(value);
    self->callback = value;
    Py_XDECREF(temp);

    return 0;
}


static PyObject *
Port_get_callback(PortObject *self, void *closure)
{
    Py_INCREF(self->callback);
    return self->callback;
}

static PyGetSetDef Port_getsetters[] =
{
    {
        "callback", (getter)Port_get_callback, (setter)Port_set_callback,
        "Callback function for hot-plugging events", NULL
    },
    { NULL }
};


static PyTypeObject PortType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Port",
    .tp_doc = "An individual port",
    .tp_basicsize = sizeof(PortObject),
    .tp_itemsize= 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = Port_new,
    .tp_dealloc = (destructor)Port_dealloc,
    .tp_traverse = (traverseproc)Port_traverse,
    .tp_clear = (inquiry)Port_clear,
    .tp_members = Port_members,
    .tp_getset = Port_getsetters
};


/* The "port" object itself just contains six slots for Ports */
#define NUM_HUB_PORTS 6
typedef struct
{
    PyObject_HEAD
    PyObject *ports[NUM_HUB_PORTS];
} PortSetObject;


static int
PortSet_traverse(PortSetObject *self, visitproc visit, void *arg)
{
    int i;

    for (i = 0; i < NUM_HUB_PORTS; i++)
        Py_VISIT(self->ports[i]);
    return 0;
}


static int
PortSet_clear(PortSetObject *self)
{
    int i;

    for (i = 0; i < NUM_HUB_PORTS; i++)
        Py_CLEAR(self->ports[i]);
    return 0;
}


static void
PortSet_dealloc(PortSetObject *self)
{
    PyObject_GC_UnTrack(self);
    PortSet_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *
PortSet_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PortSetObject *self;
    int i;

    self = (PortSetObject *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        for (i = 0; i < NUM_HUB_PORTS; i++)
        {
            if ((self->ports[i] = PyObject_CallObject((PyObject *)&PortType,
                                                      NULL)) == NULL)
            {
                for (i = 0; i < NUM_HUB_PORTS; i++)
                {
                    Py_XDECREF(self->ports[i]);
                }
                Py_XDECREF(self);
                return NULL;
            }
        }
    }
    return (PyObject *)self;
}


static PyObject *
PortSet_get_port(PortSetObject *self, void *closure)
{
    intptr_t port = (intptr_t)closure;

    if (port >= NUM_HUB_PORTS)
    {
        PyErr_SetString(PyExc_AttributeError,
                        "Internal error reading port: bad port ID");
        return NULL;
    }
    Py_INCREF(self->ports[port]);
    return self->ports[port];
}


static PyGetSetDef PortSet_getsetters[] =
{
    { "A", (getter)PortSet_get_port, NULL, "Port A", (void *)0 },
    { "B", (getter)PortSet_get_port, NULL, "Port B", (void *)1 },
    { "C", (getter)PortSet_get_port, NULL, "Port C", (void *)2 },
    { "D", (getter)PortSet_get_port, NULL, "Port D", (void *)3 },
    { "E", (getter)PortSet_get_port, NULL, "Port E", (void *)4 },
    { "F", (getter)PortSet_get_port, NULL, "Port F", (void *)5 },
    { NULL }
};


static PyTypeObject PortSetType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "hub.PortSet",
    .tp_doc = "Container for the devices plugged in to the hat's ports",
    .tp_basicsize = sizeof(PortSetObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = PortSet_new,
    .tp_dealloc = (destructor)PortSet_dealloc,
    .tp_traverse = (traverseproc)PortSet_traverse,
    .tp_clear = (inquiry)PortSet_clear,
    .tp_getset = PortSet_getsetters
};


/* The hub's (one and only) collection of ports */
static PortSetObject *port_set;

int port_init(PyObject *hub)
{
    if (PyType_Ready(&PortSetType) < 0)
        return -1;
    Py_INCREF(&PortSetType);

    if (PyType_Ready(&PortType) < 0)
    {
        Py_DECREF(&PortSetType);
        return -1;
    }
    Py_INCREF(&PortType);

    port_set =
        (PortSetObject *)PyObject_CallObject((PyObject *)&PortSetType, NULL);
    Py_DECREF(&PortType);
    Py_DECREF(&PortSetType);
    if (port_set == NULL)
        return -1;
    if (PyModule_AddObject(hub, "port", (PyObject *)port_set) < 0)
    {
        Py_DECREF(port_set);
        return -1;
    }
    return 0;
}


void port_deinit(void)
{
}


/* Called from the communications thread */
int port_attach_port(uint8_t port_id,
                     uint16_t type_id,
                     uint8_t *hw_revision,
                     uint8_t *fw_revision)
{
    /* First we must claim the global interpreter lock */
    PyGILState_STATE gstate = PyGILState_Ensure();
    PortObject *port = (PortObject *)port_set->ports[port_id];
    PyObject *version;
    PyObject *arg_list;
    int rv = 0;

    port->type_id = type_id;
    version = cmd_version_as_unicode(hw_revision);
    Py_XDECREF(port->hw_revision);
    port->hw_revision = version;
    version = cmd_version_as_unicode(fw_revision);
    Py_XDECREF(port->fw_revision);
    port->fw_revision = version;

    if (port->callback != Py_None)
    {
        arg_list = Py_BuildValue("(i)", 1); /* ATTACHED */
        rv = (PyObject_CallObject(port->callback, arg_list) != NULL) ? 0 : -1;
    }

    /* Release the GIL */
    PyGILState_Release(gstate);

    return rv;
}


int port_detach_port(uint8_t port_id)
{
    /* First claim the global interpreter lock */
    PyGILState_STATE gstate = PyGILState_Ensure();
    PortObject *port = (PortObject *)port_set->ports[port_id];
    PyObject *arg_list;
    int rv = 0;

    port->type_id = 0;
    Py_XDECREF(port->hw_revision);
    port->hw_revision = NULL;
    Py_XDECREF(port->fw_revision);
    port->fw_revision = NULL;

    if (port->callback != Py_None)
    {
        arg_list = Py_BuildValue("(i)", 0); /* DETATCHED */
        rv = (PyObject_CallObject(port->callback, arg_list) != NULL) ? 0 : 1;
    }

    /* Release the penguins^WGIL */
    PyGILState_Release(gstate);

    return rv;
}

