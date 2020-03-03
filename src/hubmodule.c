/* hubmodule.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Module to provide Python access to the Lego Hat via I2C
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>
#include <stddef.h>

#include "i2c.h"
#include "cmd.h"
#include "port.h"
#include "device.h"

#ifdef DEBUG_I2C
#include "debug-i2c.h"
#endif

/* The Hub object, an instance of which we make available */
typedef struct
{
    PyObject_HEAD
    PyObject *ports;
} HubObject;


static int
Hub_traverse(HubObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->ports);
    return 0;
}


static int
Hub_clear(HubObject *self)
{
    Py_CLEAR(self->ports);
    return 0;
}


static void
Hub_dealloc(HubObject *self)
{
    PyObject_GC_UnTrack(self);
    Hub_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *
Hub_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HubObject *self = (HubObject *)type->tp_alloc(type, 0);

    if (self == NULL)
        return NULL;

    self->ports = port_init();
    if (self->ports == NULL)
    {
        Py_DECREF(self);
        return NULL;
    }
    if (i2c_open_hat() < 0)
    {
        Py_DECREF(self);
        return NULL;
    }

    return (PyObject *)self;
}


static void
Hub_finalize(PyObject *self)
{
    PyObject *etype, *evalue, *etraceback;

    PyErr_Fetch(&etype, &evalue, &etraceback);
    i2c_close_hat();
    PyErr_Restore(etype, evalue, etraceback);
}


/* Make the hub.port object read-only */
static PyObject *
Hub_get_port(HubObject *self, void *closure)
{
    Py_INCREF(self->ports);
    return self->ports;
}


static PyGetSetDef Hub_getsetters[] =
{
    { "port", (getter)Hub_get_port, NULL, "Ports connected to the hub", NULL },
    { NULL }
};


static PyObject *
hub_info(PyObject *self, PyObject *args)
{
    PyObject *dict;
    PyObject *hw_revision;
    PyObject *fw_revision;

    if (!PyArg_ParseTuple(args, "")) /* No args to this function */
        return NULL;

    /* Add the hardware revision to the dictionary */
    hw_revision = cmd_get_hardware_version();
    if (hw_revision == NULL)
        return NULL;

    /* ...and the firmware revision while we're at it */
    fw_revision = cmd_get_firmware_version();
    if (fw_revision == NULL)
    {
        Py_DECREF(hw_revision);
        return NULL;
    }

    dict = Py_BuildValue("{sOsO}",
                         "hardware_revision", hw_revision,
                         "firmware_revision", fw_revision);
    Py_DECREF(hw_revision);
    Py_DECREF(fw_revision);
    return dict;
}

#ifdef DEBUG_I2C
static PyObject *
hub_debug_i2c(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) /* No args here either */
        return NULL;

    log_i2c_dump();

    Py_RETURN_NONE;
}
#endif /* DEBUG_I2C */


static PyMethodDef Hub_methods[] = {
    { "info", hub_info, METH_VARARGS, "Information about the Hub" },
#ifdef DEBUG_I2C
    { "debug_i2c", hub_debug_i2c, METH_VARARGS, "Dump recorded I2C traffic" },
#endif
    { NULL, NULL, 0, NULL }
};


static PyTypeObject HubType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Hub",
    .tp_doc = "Object encapsulating a Shortcake Hat",
    .tp_basicsize = sizeof(HubObject),
    .tp_itemsize = 0,
    .tp_flags = (Py_TPFLAGS_DEFAULT |
                 Py_TPFLAGS_HAVE_GC |
                 Py_TPFLAGS_HAVE_FINALIZE),
    .tp_new = Hub_new,
    .tp_dealloc = (destructor)Hub_dealloc,
    .tp_traverse = (traverseproc)Hub_traverse,
    .tp_clear = (inquiry)Hub_clear,
    .tp_getset = Hub_getsetters,
    .tp_methods = Hub_methods,
    .tp_finalize = Hub_finalize
};


static struct PyModuleDef hubmodule = {
    PyModuleDef_HEAD_INIT,
    "hub",
    NULL, /* Documentation */
    -1,   /* module state is in globals */
};


PyMODINIT_FUNC
PyInit_hub(void)
{
    PyObject *hub;
    PyObject *hub_obj;

    if ((hub = PyModule_Create(&hubmodule)) == NULL)
        return NULL;
    if (PyType_Ready(&HubType) < 0)
    {
        Py_DECREF(hub);
        return NULL;
    }
    Py_INCREF(&HubType);

    if (device_modinit() < 0)
    {
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }

    if (port_modinit() < 0)
    {
        device_demodinit();
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }

    if (cmd_modinit(hub) < 0)
    {
        port_demodinit();
        device_demodinit();
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }

    hub_obj = PyObject_CallObject((PyObject *)&HubType, NULL);
    Py_XINCREF(hub_obj);
    if (PyModule_AddObject(hub, "hub", hub_obj) < 0)
    {
        Py_XDECREF(hub_obj);
        Py_CLEAR(hub_obj);
        cmd_demodinit();
        port_demodinit();
        device_demodinit();
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }
    return hub;
}
