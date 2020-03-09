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
    PyObject *values;
    int current_mode;
    uint8_t is_unreported;
    uint8_t is_mode_busy;
    uint8_t is_motor_busy;
} DeviceObject;


#define DEVICE_FORMAT_RAW     0
#define DEVICE_FORMAT_PERCENT 1
#define DEVICE_FORMAT_SI      2

#define DEVICE_FORMAT_min 0
#define DEVICE_FORMAT_max 2


static int
Device_traverse(DeviceObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->port);
    Py_VISIT(self->values);
    return 0;
}


static int
Device_clear(DeviceObject *self)
{
    Py_CLEAR(self->port);
    Py_CLEAR(self->values);
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
        self->values = Py_BuildValue("[O]", Py_None);
        if (self->values == NULL)
        {
            type->tp_free((PyObject *)self);
            return NULL;
        }
        Py_INCREF(Py_None);
        self->port = Py_None;
        Py_INCREF(Py_None);
        self->current_mode = 0;
        self->is_unreported = 0;
        self->is_mode_busy = 0;
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
        mode_info_t *mode_info;

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

        /* Set up list for handling input values */
        if ((mode_info = port_get_mode(device->port, mode)) == NULL)
            return NULL;
        if (mode_info->format.datasets != PyList_Size(device->values))
        {
            /* Need to resize the list */
            PyObject *new_list = PyList_New(mode_info->format.datasets);
            PyObject *old_list;
            int i;

            if (new_list == NULL)
                return NULL;
            for (i = 0; i < mode_info->format.datasets; i++)
            {
                PyList_SET_ITEM(new_list, i, Py_None);
            }
            old_list = device->values;
            device->values = new_list;
            Py_XDECREF(old_list);
            device->is_unreported = 0;
        }

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


static float rescale_float(float value,
                           min_max_t *inrange,
                           min_max_t *outrange)
{
    float in_interval = inrange->max - inrange->min;
    float out_interval = outrange->max - outrange->min;

    return  ((value - inrange->min) *
             out_interval / in_interval) + outrange->min;
}


static long rescale_long(long value, min_max_t *inrange, min_max_t *outrange)
{
    return (long)(rescale_float((float)value, inrange, outrange) + 0.5);
}


static PyObject *
Device_get(PyObject *self, PyObject *args)
{
    DeviceObject *device = (DeviceObject *)self;
    PyObject *arg1 = NULL;
    PyObject *results;
    int format = DEVICE_FORMAT_SI;
    mode_info_t *mode;
    int result_count, i;

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

    if (!device->is_unreported)
    {
        if (port_ensure_mode_info(device->port) < 0 ||
            cmd_get_port_value(port_get_id(device->port)) < 0)
        {
            return NULL;
        }
    }

    /* "format" now contains the format code to use: Raw (0), Pct (1) or
     * SI (2).
     */

    /* Get the current mode */
    if ((mode = port_get_mode(device->port, device->current_mode)) == NULL)
    {
        if (PyErr_Occurred() == NULL)
        {
            /* This shouldn't be possible unless the device detaches
             * in mid call.
             */
            PyErr_SetString(cmd_get_exception(),
                            "Unexpectedly detached device");
        }
        return NULL;
    }
    result_count = PyList_Size(device->values);
    if (result_count != mode->format.datasets)
    {
        PyErr_SetString(cmd_get_exception(),
                        "Device value length mismatch");
        return NULL;
    }

    /* We wish to return a list with "mode->format->datasets" data
     * values.
     */
    if ((results = PyList_New(mode->format.datasets)) == NULL)
        return NULL;

    /* device->values is a list containing result_count elements */
    for (i = 0; i < result_count; i++)
    {
        PyObject *value = PyList_GetItem(device->values, i);
        if (PyLong_Check(value))
        {
            long long_value = PyLong_AsLong(value);

            if (long_value == -1 && PyErr_Occurred() == NULL)
            {
                Py_DECREF(results);
                return NULL;
            }
            switch (format)
            {
                case DEVICE_FORMAT_PERCENT:
                    long_value = rescale_long(long_value,
                                              &mode->raw,
                                              &mode->percent);
                    break;

                case DEVICE_FORMAT_SI:
                    long_value = rescale_long(long_value,
                                              &mode->raw,
                                              &mode->si);
                    break;

                default:
                    break;
            }
            value = PyLong_FromLong(long_value);
            if (value == NULL)
            {
                Py_DECREF(results);
                return NULL;
            }
            PyList_SET_ITEM(results, i, value);
        }
        else if (PyFloat_Check(value))
        {
            float fvalue = (float)PyFloat_AsDouble(value);

            if (fvalue == -1.0 && PyErr_Occurred() == NULL)
            {
                Py_DECREF(results);
                return NULL;
            }
            switch (format)
            {
                case DEVICE_FORMAT_PERCENT:
                    fvalue = rescale_float(fvalue,
                                           &mode->raw,
                                           &mode->percent);
                    break;

                case DEVICE_FORMAT_SI:
                    fvalue = rescale_float(fvalue,
                                           &mode->raw,
                                           &mode->si);
                    break;

                default:
                    break;
            }
            value = PyFloat_FromDouble((double)fvalue);
            if (value == NULL)
            {
                Py_DECREF(results);
                return NULL;
            }
            PyList_SET_ITEM(results, i, value);
        }
        else if (value == Py_None)
        {
            Py_INCREF(Py_None);
            PyList_SET_ITEM(results, i, Py_None);
        }
        else
        {
            PyErr_SetString(cmd_get_exception(), "Invalid value");
            Py_DECREF(results);
            return NULL;
        }
    }

    device->is_unreported = 0;
    return results;
}



static PyMethodDef Device_methods[] = {
    { "mode", Device_mode, METH_VARARGS, "Get or set the current mode" },
    { "get", Device_get, METH_VARARGS, "Get a set of readings from the device" },
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


int device_modinit(void)
{
    if (PyType_Ready(&DeviceType) < 0)
        return -1;
    Py_INCREF(&DeviceType);
    return 0;
}


void device_demodinit(void)
{
    Py_DECREF(&DeviceType);
}


PyObject *device_new_device(PyObject *port)
{
    PyObject *args = Py_BuildValue("(O)", port);

    if (args == NULL)
        return NULL;
    return PyObject_CallObject((PyObject *)&DeviceType, args);
}


int device_new_value(PyObject *self, uint8_t *buffer, uint16_t nbytes)
{
    DeviceObject *device = (DeviceObject *)self;
    PyObject *values;
    PyObject *value;
    mode_info_t *mode;
    int i;
    uint16_t bytes_consumed = 1;

    device->is_mode_busy = 0;

    if ((mode = port_get_mode(device->port, device->current_mode)) == NULL)
        /* Can't get the mode info */
        return -1;

    /* Construct the list to contain these results */
    if ((values = PyList_New(mode->format.datasets)) == NULL)
        return -1;

    for (i = 0; i < mode->format.datasets; i++)
    {
        value = NULL;
        switch (mode->format.type)
        {
            case FORMAT_8BIT:
                if (nbytes < 1)
                {
                    Py_DECREF(values);
                    return -1;
                }
                if ((value = PyLong_FromLong(buffer[0])) == NULL)
                {
                    Py_DECREF(values);
                    return -1;
                }
                buffer++;
                nbytes--;
                bytes_consumed++;
                break;

            case FORMAT_16BIT:
                if (nbytes < 2)
                {
                    Py_DECREF(values);
                    return -1;
                }
                if ((value = PyLong_FromLong(buffer[0] |
                                             (buffer[1] << 8))) == NULL)
                {
                    Py_DECREF(values);
                    return -1;
                }
                buffer += 2;
                nbytes -= 2;
                bytes_consumed += 2;
                break;

            case FORMAT_32BIT:
                if (nbytes < 4)
                {
                    Py_DECREF(values);
                    return -1;
                }
                if ((value = PyLong_FromLong(buffer[0] |
                                             (buffer[1] << 8) |
                                             (buffer[2] << 16) |
                                             (buffer[3] << 24))) == NULL)
                {
                    Py_DECREF(values);
                    return -1;
                }
                buffer += 4;
                nbytes -= 4;
                bytes_consumed += 4;
                break;

            case FORMAT_FLOAT:
            {
                uint32_t bytes;
                float fvalue;

                if (nbytes < 4)
                {
                    Py_DECREF(values);
                    return -1;
                }
                bytes = (buffer[0] |
                         (buffer[1] << 8) |
                         (buffer[2] << 16) |
                         (buffer[3] << 24));
                memcpy(&fvalue, &bytes, 4);
                if ((value = PyFloat_FromDouble((double)fvalue)) == NULL)
                {
                    Py_DECREF(values);
                    return -1;
                }
                buffer += 4;
                nbytes -= 4;
                bytes_consumed += 4;
                break;
            }
        }

        PyList_SET_ITEM(values, i, value);
    }

    /* Swap the new data into place */
    PyObject *old_values = device->values;
    device->values = values;
    Py_XDECREF(old_values);
    device->is_unreported = 1;

    return bytes_consumed;
}


int device_new_format(PyObject *self)
{
    DeviceObject *device = (DeviceObject *)self;

    /* A device is considered busy with its mode inbetween sending
     * the Port Input Format message confirming a mode/format change
     * and the next Port Value message.  This may supercede the
     * device->is_unreported flag.
     */
    device->is_mode_busy = 1;
    return 0;
}


int device_set_port_busy(PyObject *self, uint8_t is_busy)
{
    DeviceObject *device = (DeviceObject *)self;

    device->is_motor_busy = is_busy ? 1 : 0;
    return 0;
}


PyObject *device_is_busy(PyObject *self, int type)
{
    DeviceObject *device = (DeviceObject *)self;

    switch (type)
    {
        case 0:
            return PyBool_FromLong(device->is_mode_busy);

        case 1:
            return PyBool_FromLong(device->is_motor_busy);

        default:
            break;
    }

    PyErr_Format(PyExc_ValueError, "Invalid busy type %d\n", type);
    return NULL;
}
