/* hubmodule.c
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
 * Module to provide Python access to the Build HAT via I2C
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#include "i2c.h"
#include "cmd.h"
#include "port.h"
#include "device.h"
#include "motor.h"
#include "pair.h"
#include "callback.h"
#include "firmware.h"

#ifdef DEBUG_I2C
#include "debug-i2c.h"
#endif


/* Hijinks is required to pass a string through compiler defines */
#define XSTR(s) #s
#define STRING(s) XSTR(s)

/**

This module provides access to the Build HAT.

.. py:exception:: build_hat.HubProtocolError

    This exception is raised by the build_hat module when no standard
    exception is more relevant.  It often indicates a problem
    communicating with the Build HAT.

.. py:class:: build_hat.BuildHAT

    Represents a Build HAT.

    .. note::

        Only one instance of the BuildHAT class may be created.

    .. py:attribute:: port

        :type: PortSet

        The collection of ports on the Build HAT.

    .. py:attribute:: firmware

        :type: Firmware

        The firmware upgrade controller.

    .. py:exception:: HubProtocolError

        A copy of the module's HubProtocolError attribute, for
        convenience when the module namespace is not available.

    .. py:method:: info() -> dict

        Returns a dictionary containing the following keys:

        * ``firmware_revision`` : Firmware version as a string in the form
          ``MAJOR.MINOR.BUGFIX.BUILD``
        * ``hardware_revision`` : Hardware version as a string in the form
          ``MAJOR.MINOR.BUGFIX.BUILD``

    .. py:method:: status() -> dict

        Returns a dictionary containing a single key, ``port``, whose
        value is another dictionary.  That dictionary has the keys
        ``A``, ``B``, ``C``, ``D``, ``E``, and ``F``, whose values are
        the current values of the corresponding ports.  See
        :py:meth:`Device.get()` for details.

*/

/* The BuildHAT object */
typedef struct
{
    PyObject_HEAD
    PyObject *ports;
    PyObject *exception;
    PyObject *firmware;
    int initialised;
} HubObject;


static int
Hub_traverse(HubObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->ports);
    Py_VISIT(self->firmware);
    Py_VISIT(self->exception);
    return 0;
}


static int
Hub_clear(HubObject *self)
{
    Py_CLEAR(self->ports);
    Py_CLEAR(self->firmware);
    Py_CLEAR(self->exception);
    return 0;
}


static void
Hub_dealloc(HubObject *self)
{
    PyObject_GC_UnTrack(self);
    Hub_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


/* Make this class a singleton */
static int build_hat_created = 0;

static PyObject *
Hub_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HubObject *self;

    if (build_hat_created != 0)
    {
        PyErr_SetString(PyExc_RuntimeError,
                        "A BuildHAT() instance already exists");
        return NULL;
    }

    if ((self = (HubObject *)type->tp_alloc(type, 0)) == NULL)
        return NULL;

    self->initialised = 0;
    self->ports = port_init();
    if (self->ports == NULL)
    {
        Py_DECREF(self);
        return NULL;
    }
    Py_INCREF(Py_None);
    self->exception = Py_None;
    Py_INCREF(Py_None);
    self->firmware = Py_None;
    if (callback_init() < 0)
    {
        Py_DECREF(self);
        return NULL;
    }
    if (i2c_open_hat() < 0)
    {
        Py_DECREF(self);
        return NULL;
    }

    build_hat_created = 1;
    return (PyObject *)self;
}


static int
Hub_init(HubObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { NULL };
    PyObject *new;
    PyObject *old;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "", kwlist))
        return -1;
    if (self->initialised)
        return 0;

    new = cmd_get_exception();
    Py_INCREF(new);
    old = self->exception;
    Py_DECREF(old);
    self->exception = new;

    new = firmware_init();
    if (new == NULL)
        return -1;
    old = self->firmware;
    Py_DECREF(old);
    self->firmware = new;

//    if (cmd_action_reset() < 0)
//        return -1;
//    if (cmd_wait_for_reset_complete() < 0)
//        return -1;
    self->initialised = 1;

    /* Give the HAT a chance to recognise what is attached to it */
    Py_BEGIN_ALLOW_THREADS
    usleep(800000);
    Py_END_ALLOW_THREADS

    return 0;
}


static void
Hub_finalize(PyObject *self)
{
    PyObject *etype, *evalue, *etraceback;

    PyErr_Fetch(&etype, &evalue, &etraceback);
    i2c_close_hat();
    callback_finalize();
    PyErr_Restore(etype, evalue, etraceback);
}


/* Make the hub.port object read-only */
static PyObject *
Hub_get_port(HubObject *self, void *closure)
{
    Py_INCREF(self->ports);
    return self->ports;
}

/* Ditto the protocol exception */
static PyObject *
Hub_get_exception(HubObject *self, void *closure)
{
    Py_INCREF(self->exception);
    return self->exception;
}

/* Ditto the firmware upgrader */
static PyObject *
Hub_get_firmware(HubObject *self, void *closure)
{
    Py_INCREF(self->firmware);
    return self->firmware;
}


static PyGetSetDef Hub_getsetters[] =
{
    { "port", (getter)Hub_get_port, NULL, "Ports connected to the hub", NULL },
    {
        "HubProtocolError",
        (getter)Hub_get_exception,
        NULL,
        "The internal protocol error",
        NULL
    },
    {
        "firmware",
        (getter)Hub_get_firmware,
        NULL,
        "The firmware upgrader",
        NULL
    },
    { NULL }
};


static PyObject *
Hub_info(PyObject *self, PyObject *args)
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


static PyObject *
Hub_status(PyObject *self, PyObject *args)
{
    HubObject *hub = (HubObject *)self;
    PyObject *port_status;

    if (!PyArg_ParseTuple(args, "")) /* No args to this function */
        return NULL;

    if ((port_status = ports_get_value_dict(hub->ports)) == NULL)
        return NULL;

    return Py_BuildValue("{sO}", "port", port_status);
}


#ifdef DEBUG_I2C
static PyObject *
Hub_debug_i2c(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) /* No args here either */
        return NULL;

    log_i2c_dump();

    Py_RETURN_NONE;
}
#endif /* DEBUG_I2C */


static PyMethodDef Hub_methods[] = {
    { "info", Hub_info, METH_VARARGS, "Information about the Hub" },
    { "status", Hub_status, METH_VARARGS, "Status of the Hub" },
#ifdef DEBUG_I2C
    { "debug_i2c", Hub_debug_i2c, METH_VARARGS, "Dump recorded I2C traffic" },
#endif
    { NULL, NULL, 0, NULL }
};


static PyTypeObject HubType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "BuildHAT",
    .tp_doc = "Object encapsulating a Shortcake Hat",
    .tp_basicsize = sizeof(HubObject),
    .tp_itemsize = 0,
    .tp_flags = (Py_TPFLAGS_DEFAULT |
                 Py_TPFLAGS_HAVE_GC |
                 Py_TPFLAGS_HAVE_FINALIZE),
    .tp_new = Hub_new,
    .tp_init = (initproc)Hub_init,
    .tp_dealloc = (destructor)Hub_dealloc,
    .tp_traverse = (traverseproc)Hub_traverse,
    .tp_clear = (inquiry)Hub_clear,
    .tp_getset = Hub_getsetters,
    .tp_methods = Hub_methods,
    .tp_finalize = Hub_finalize
};


static struct PyModuleDef hubmodule = {
    PyModuleDef_HEAD_INIT,
    "build_hat",
    NULL, /* Documentation */
    -1,   /* module state is in globals */
};


PyMODINIT_FUNC
PyInit_build_hat(void)
{
    PyObject *hub;

    if ((hub = PyModule_Create(&hubmodule)) == NULL)
        return NULL;
    if (PyType_Ready(&HubType) < 0)
    {
        Py_DECREF(hub);
        return NULL;
    }
    Py_INCREF(&HubType);
    PyModule_AddStringConstant(hub, "__version__", STRING(LIB_VERSION));

    if (device_modinit() < 0)
    {
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }

    if (motor_modinit() < 0)
    {
        device_demodinit();
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }

    if (port_modinit() < 0)
    {
        motor_demodinit();
        device_demodinit();
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }

    if (pair_modinit() < 0)
    {
        port_demodinit();
        motor_demodinit();
        device_demodinit();
        Py_DECREF(&HubType);
        Py_DECREF(hub);
    }

    if (cmd_modinit(hub) < 0)
    {
        pair_demodinit();
        port_demodinit();
        motor_demodinit();
        device_demodinit();
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }

    if (firmware_modinit() < 0)
    {
        cmd_demodinit();
        pair_demodinit();
        port_demodinit();
        motor_demodinit();
        device_demodinit();
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }

    if (PyModule_AddObject(hub, "BuildHAT", (PyObject *)&HubType) < 0)
    {
        firmware_demodinit();
        cmd_demodinit();
        pair_demodinit();
        port_demodinit();
        motor_demodinit();
        device_demodinit();
        Py_DECREF(&HubType);
        Py_DECREF(hub);
        return NULL;
    }

    return hub;
}
