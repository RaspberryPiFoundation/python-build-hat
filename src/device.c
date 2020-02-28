/* device.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Python class for handling a port's "device" attribute
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>

#include "device.h"
#include "port.h"


/* The actual Device type */
typedef struct
{
    PyObject_HEAD
    PyObject *port;
    int current_mode;
} DeviceObject;


static int
Device_traverse(DeviceObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->port);
    return 0;
}


static int
Device_clear(DeviceObject *self)
{
    Py_CLEAR(self->port);
    return 0;
}


static void
Device_dealloc(DeviceObject *self)
{
    PyObject_GC_UnTrack(self);
    Device_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *
Device_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    DeviceObject *self = (DeviceObject *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->port = Py_None;
        Py_INCREF(Py_None);
        self->current_mode = 0;
    }
    return (PyObject *)self;
}


static int
Device_init(DeviceObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "port" };
    PyObject *port = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &port))
        return -1;

    if (port != NULL)
    {
        PyObject *tmp = self->port;
        Py_INCREF(port);
        self->port = port;
        Py_XDECREF(tmp);
    }
    return 0;
}


static PyObject *
Device_repr(PyObject *self)
{
    DeviceObject *device = (DeviceObject *)self;
    int port_id = port_get_id(device->port);

    return PyUnicode_FromFormat("Device(%c)", 'A' + port_id);
}


static PyObject *
Device_mode(PyObject *self, PyObject *args)
{
    DeviceObject *device = (DeviceObject *)self;
    PyObject *arg1 = NULL;
    PyObject *arg2 = NULL;

    if (!PyArg_ParseTuple(args, "|OO:mode", &arg1, &arg2))
        return NULL;

    if (arg1 == NULL)
    {
        PyObject *results;
        int i;

        /* Return the current mode and dataset(s) */
        mode_info_t *mode = port_get_mode(device->port,
                                          device->current_mode);

        if (mode == NULL)
        {
            if (PyErr_Occurred() == NULL)
            {
                /* This shouldn't be possible unless the device
                 * detaches in mid call somehow.
                 */
                PyErr_SetString(cmd_get_exception(),
                                "Unexpectedly detached device");
            }
            return NULL;
        }

        /* Now create the response list */
        if ((results = PyList_New(mode->format.datasets)) == NULL)
            return NULL;
        for (i = 0; i < mode->format.datasets; i++)
        {
            PyObject *entry = Py_BuildValue("(ii)", device->current_mode, i);

            if (entry == NULL)
            {
                Py_DECREF(results);
                return NULL;
            }
            PyList_SET_ITEM(results, i, entry);
        }
        return results;
    }
    else if (PyLong_Check(arg1))
    {
        int mode = PyLong_AsLong(arg1);
        int check;

        /* Check the type conversion worked */
        if (PyErr_Occurred() != NULL)
            return NULL;

        check = port_check_mode(device->port, mode);
        if (check == 0)
        {
            PyErr_SetString(PyExc_ValueError, "Invalid mode number");
            return NULL;
        }
        else if (check < 0)
            return NULL; /* Exception already set */

        if (cmd_set_mode(port_get_id(device->port), mode) < 0)
            return NULL;
        device->current_mode = mode;

        if (arg2 == NULL)
        {
            /* This is just setting the mode */
            Py_RETURN_NONE;
        }

        /* If we have a second parameter, it must be a byte string
         * that will be sent to the device as Mode Data.
         */
        if (PyBytes_Check(arg2))
        {
            char *buffer;
            Py_ssize_t nbytes;

            if (PyBytes_AsStringAndSize(arg2, &buffer, &nbytes) < 0)
                return NULL;
            if (cmd_write_mode_data(port_get_id(device->port),
                                    device->current_mode,
                                    nbytes,
                                    buffer) < 0)
                return NULL;
            Py_RETURN_NONE;
        }
        else if (PyByteArray_Check(arg2))
        {
            if (cmd_write_mode_data(port_get_id(device->port),
                                    device->current_mode,
                                    PyByteArray_Size(arg2),
                                    PyByteArray_AsString(arg2)) < 0)
                return NULL;
            Py_RETURN_NONE;
        }
        PyErr_SetString(PyExc_TypeError,
                        "Second arg to mode() must be a bytes or bytearray object");
        return NULL;
    }
    /* TODO: write the rest of this */
    PyErr_SetString(PyExc_NotImplementedError,
                    "mode() with non-int parameters not yet implemented");
    return NULL;
}


#if 0
static PyObject *
Device_get(PyObject *self, PyObject *args)
{
    DeviceObject *device = (DeviceObject *)self;
    PyObject *arg1 = NULL;
    PyObject *results;
    int format = DEVICE_FORMAT_SI;

    if (!PyArg_ParseTuple(args, "|O:get", &arg1))
        return NULL;

    if (arg1 != NULL)
    {
        if (PyLong_Check(arg1))
        {
            format = PyLong_AsLong(arg1);
            if (PyErr_Occurred() != NULL)
                return NULL;
            if (format < DEVICE_FORMAT_min || format > DEVICE_FORMAT_max)
            {
                PyErr_SetString(PyExc_ValueError, "Invalid format number");
                return NULL;
            }
        }
    }

    /* "format" now contains the format code to use: Raw (0), Pct (1) or
     * SI (2).
     */

    /* Get the current mode */
    mode_info_t *mode = port_get_mode(device->port,
                                      device->current_mode);
    if (mode == NULL)
    {
        if (PyErr_Occurred() == NULL)
        {
            /* This shouldn't be possible unless the device detaches
             * in mid call.
             */
            PyErr_SetString(cmd_get_exception(),
                            "Unpexpectedly detached device");
        }
        return NULL;
    }

    /* We wish to return a list with "mode->format->datasets" data
     * values.  Make that list ahead of time.
     */
    if ((results = PyList_New(mode->format.datasets)) == NULL)
        return NULL;
#endif

static PyMethodDef Device_methods[] = {
    { "mode", Device_mode, METH_VARARGS, "Get or set the current mode" },
    { NULL, NULL, 0, NULL }
};


static PyTypeObject DeviceType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Device",
    .tp_doc = "Generic device on a port",
    .tp_basicsize = sizeof(DeviceObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = Device_new,
    .tp_init = (initproc)Device_init,
    .tp_dealloc = (destructor)Device_dealloc,
    .tp_traverse = (traverseproc)Device_traverse,
    .tp_clear = (inquiry)Device_clear,
    .tp_methods = Device_methods,
    .tp_repr = Device_repr
};


int device_init(void)
{
    if (PyType_Ready(&DeviceType) < 0)
        return -1;
    Py_INCREF(&DeviceType);
    return 0;
}


void device_deinit(void)
{
}


PyObject *device_new_device(PyObject *port)
{
    PyObject *args = Py_BuildValue("(O)", port);

    if (args == NULL)
        return NULL;
    return PyObject_CallObject((PyObject *)&DeviceType, args);
}
