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
#include "motor.h"
#include "pair.h"
#include "callback.h"
#include "firmware.h"

#ifdef DEBUG_I2C
#include "debug-i2c.h"
#endif

/**

This module provides access to the Shortcake hat.

.. note::

    The original module directly contained the functions and
    attributes of the Hub object.  Unfortunately because
    communications with the hat take place in a background thread,
    this unavoidably sometimes caused address exceptions during
    module finalization (i.e. when a program using the hub module
    ended).  Separating the actual hub functionality into its own
    class allows us to avoid this problem, and allows for
    supporting multiple hats in the future.

    To use pre-existing examples of code using the hub module,
    simply replace ``import hub`` with ``from hub import hub``.

.. caution::

    The methods of the original ``hub`` module are not documented.
    The documentation here is for the implemention made for the
    Shortcake hat.  This was created by examining the existing code
    and making deductions from the behaviour of the example Flipper
    hub in our possession.

.. py:exception:: hub.HubProtocolError

    This exception is raised by the hub module when no standard
    exception is more relevant.  It often indicates a problem
    communicating with the Shortcake hat.

    .. note::

        This exception is not supplied in the original.

.. py:class:: hub.Hub

    Represents a Shortcake hat.

    .. note::

        This class is not directly available to the user.  It is
        only used to create the ``hub.hub`` instance. Future
        versions of the module may make the Hub object available
        to support multiple hats with different I2C addresses.

    .. py:attribute:: port

        :type: PortSet

        The collection of ports on the Shortcake hat.

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

        .. note::

            The original info dictionary did not contain the firmware
            version.  It did give the Bluetooth UUID of the hub (not
            present on Shortcake), an undocumented integer as a
            product variant code, and a number of statistics related
            to execution under Micropython.  These were all omitted as
            irrelevant to Shortcake.

            The original info() method allowed a byte string to be
            passed as a parameter.  If this key matched a hash of the
            Bluetooth UUID, a Python module was created on the fly to
            allow testing of various low-level features of the hub
            hardware.  This is not particularly useful for this
            library, so has not been implemented.

    .. py:method:: status() -> dict

        Returns a dictionary containing a single key, ``port``, whose
        value is another dictionary.  That dictionary has the keys
        ``A``, ``B``, ``C``, ``D``, ``E``, and ``F``, whose values are
        the current values of the corresponding ports.  See
        **CROSS-REFERENCE REQUIRED** for details.

        .. note::

            The original status dictionary also contained the current
            values of the accelerometer, gyroscope, position and
            display.  Since none of these are present on Shortcake,
            they have been omitted.

.. py:attribute:: hub.hub

    This is an instance of the :py:class:`hub.Hub` class created to
    communicate with the physical Shortcake hat at the default I2C
    address.  It is automatically created when the hub module is
    loaded.


*/

/* The Hub object, an instance of which we make available */
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


static PyObject *
Hub_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    HubObject *self = (HubObject *)type->tp_alloc(type, 0);

    if (self == NULL)
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
        /* TODO: callback_shutdown() */
        Py_DECREF(self);
        return NULL;
    }

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

    if (cmd_action_reset() < 0)
        return -1;
    if (cmd_wait_for_reset_complete() < 0)
        return -1;
    self->initialised = 1;
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
    .tp_name = "Hub",
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

    hub_obj = PyObject_CallObject((PyObject *)&HubType, NULL);
    if (PyModule_AddObject(hub, "hub", hub_obj) < 0)
    {
        Py_XDECREF(hub_obj);
        Py_CLEAR(hub_obj);
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
