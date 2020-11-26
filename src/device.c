/* device.c
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
 * Python class for handling a port's "device" attribute
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>

#include "device.h"
#include "port.h"
#include "motor.h"

/**

.. py:class:: Device

    The ports on the Build HAT are able to autodetect the
    capabilities of the device that is plugged in.  When a generic
    device is detected, then an enhanced set of methods is available.

    The ``Device`` object provides the base API that is available to
    generic devices when they are plugged into a port.

    .. note::

        This class is not directly available to the user.  It is only
        used by the port object, and its instances are provided
        already initialised.

    .. py:attribute:: FORMAT_RAW

        :type: int
        :value: 0

        The value to pass to the :py:meth:`Device.get()` method to retrieve
        raw data from the device.

    .. py:attribute:: FORMAT_PCT

        :type: int
        :value: 1

        The value to pass to the :py:meth:`Device.get()` method to retrieve
        percentage-scaled data from the device.

    .. py:attribute:: FORMAT_SI

        :type: int
        :value: 2

        The value to pass to the :py:meth:`Device.get()` method to retrieve
        SI unit-scaled data from the device.

    .. py:method:: get([format=2])

        Returns a list of value(s) that the currently active device
        mode makes available.

        :param int format: The format to return the value.  Must be 0,
            1 or 2 (for RAW, PCT and SI respectively).  Defaults to
            SI.  :py:const:`FORMAT_RAW`, :py:const:`FORMAT_PCT` and
            :py:const:`FORMAT_SI` are provided for convenience.
        :return: a list of values
        :rtype: list[int] or list[float]
        :raises ValueError: if an invalid format number is given.

        If the device is in a single mode, the length of the list is
        given by the ``datasets`` field of the mode's format, and the
        type of the values by the ``type`` field.  If the device is a
        combination mode, the result is defined by the particular
        combination of modes and datasets specified to the
        :py:meth:`Device.mode()` method.

        ``format`` is a position-only argument.

    .. py:method:: pwm(value)

        Sets the PWM level generated at the port, if output is
        permitted.

        :param int value: The PWM level generated, ranging from -100
            to +100, plus the special value 127 to put a motor into
            brake state.  The polarity of the PWM signal matches the
            sign of the value.
        :raises ValueError: if the input is greater than 100 or less
            than -100.

        Calling ``pwm(0)`` stops the PWM signal and leaves the port
        driver in a floating state.

        ``value`` is a position-only argument.

        .. note::

            The original silently clips out of range input values back
            to the -100 to +100 range, and disallows pwm(127).

    .. py:method:: mode([mode[, mode_data]])

        Puts the device in the specified mode(s)

        :param mode: The mode to put the device into.
        :type mode: int or list[tuple[int, int]]
        :param mode_data: Mode data to be written to the device
        :type mode_data: bytes or bytearray
        :raises ValueError: if an invalid mode number is given

        If no parameters are given, this method returns a list of
        ``(mode_number, dataset_number)`` tuples.  Each tuple
        corresponds to one entry in the list of values returned by
        :py:meth:`Device.get()`.  Otherwise the method returns
        ``None``.

        If ``mode`` is an integer, the device is put into that mode.
        :py:meth:`Device.get()` will return the list of values as read
        for that mode.

        If the mode specifier is a list, it must contain
        ``(mode_number, dataset_number)`` tuples.  In this case the
        device will return each value for that mode and dataset, in
        order, when the :py:meth:`Device.get()` method is invoked.
        The modes must be a permitted combination (as in the
        ``combi_modes`` entry of the :py:meth:`Port.info()`
        dictionary.

        If the ``mode`` is an integer (simple mode), it may be
        followed by ``mode_data``.  The sequence of bytes is sent
        directly to the device via an output mode write, details of
        which are beyond the scope of this document.

        ``mode`` and ``mode_data`` are position-only parameters.

*/

#define MAX_DATASETS 8
#define MODE_IS_COMBI (-1)

/* The actual Device type */
typedef struct
{
    PyObject_HEAD
    PyObject *port;
    PyObject *values;
    PyObject *hw_revision;
    PyObject *fw_revision;
    int      current_mode;
    int      saved_current_mode;
    uint16_t type_id;
    uint16_t input_mode_mask;
    uint16_t output_mode_mask;
    uint8_t  flags;
    uint8_t  rx_error;
    uint8_t  num_modes;
    uint8_t  is_mode_busy;
    uint8_t  is_motor_busy;
    uint8_t  num_combi_modes;
    uint8_t  combi_index;
    uint8_t  saved_num_combi_modes;
    uint8_t  saved_combi_index;
    uint8_t  combi_mode[MAX_DATASETS];
    uint8_t  saved_combi_mode[MAX_DATASETS];
    combi_mode_t combi_modes;
    mode_info_t modes[16];
    PyObject *callback;
} DeviceObject;

#define DO_FLAGS_GOT_MODE_INFO 0x01
#define DO_FLAGS_COMBINABLE    0x02
#define DO_FLAGS_DETACHED      0x80

#define DO_RXERR_NONE         0x00
#define DO_RXERR_NO_MODE_INFO 0x01
#define DO_RXERR_BAD_MODE     0x02
#define DO_RXERR_INTERNAL     0x03
#define DO_RXERR_BAD_FORMAT   0x04

#define DEVICE_FORMAT_RAW     0
#define DEVICE_FORMAT_PERCENT 1
#define DEVICE_FORMAT_SI      2

#define DEVICE_FORMAT_min 0
#define DEVICE_FORMAT_max 2


typedef struct
{
    uint16_t type_id;
    uint16_t mode_mask;
    int num_modes;
    uint8_t mode_list[MAX_DATASETS];
} default_mode_t;

#define NUM_DEFAULT_MODES 7

#define MODE(m,d) (((m) << 4) | (d))
static default_mode_t default_modes[NUM_DEFAULT_MODES] = {
    {
        ID_MOTOR_SMALL,
        0x000f,
        4,
        /* Speed      Pos        APos       Power */
        { MODE(1,0), MODE(2,2), MODE(3,1), MODE(0,0) },
    },
    {
        ID_MOTOR_MEDIUM,
        0x000f,
        4,
        /* Speed      Pos        APos       Power */
        { MODE(1,0), MODE(2,2), MODE(3,1), MODE(0,0) },
    },
    {
        ID_MOTOR_LARGE,
        0x000f,
        4,
        /* Speed      Pos        APos       Power */
        { MODE(1,0), MODE(2,2), MODE(3,1), MODE(0,0) },
    },
    {
        ID_STONE_GREY_MOTOR_MEDIUM,
        0x000f,
        4,
        /* Speed      Pos        APos       Power */
        { MODE(1,0), MODE(2,2), MODE(3,1), MODE(0,0) },
    },
    {
        ID_STONE_GREY_MOTOR_LARGE,
        0x000f,
        4,
        /* Speed      Pos        APos       Power */
        { MODE(1,0), MODE(2,2), MODE(3,1), MODE(0,0) },
    },
    {
        ID_COLOUR,
        0x0023,
        5,
        /* Colour     Reflect    Red        Green      Blue */
        { MODE(1,0), MODE(0,0), MODE(5,0), MODE(5,1), MODE(5,2) },
    },
    {
        ID_FORCE,
        0x0013,
        3,
        { MODE(0,0), MODE(1,0), MODE(4,0) },
    }
};
#undef MODE


/* Forward declarations */
static int get_value(DeviceObject *self);
static int extract_value(DeviceObject *self, uint8_t index, long *pvalue);


static default_mode_t *
get_default_mode(uint16_t id)
{
    int i;

    for (i = 0; i < NUM_DEFAULT_MODES; i++)
        if (default_modes[i].type_id == id)
            return &default_modes[i];
    return NULL;
}


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
        self->port = Py_None;
        Py_INCREF(Py_None);
        self->current_mode = 0;
        self->saved_current_mode = 0;
        self->flags = 0;
        self->rx_error = 0;
        self->is_mode_busy = 0;
        self->is_motor_busy = 0;
        self->num_combi_modes = 0;
        self->callback = Py_None;
        Py_INCREF(Py_None);
    }
    return (PyObject *)self;
}


static int
Device_init(DeviceObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "port", NULL };
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
Device_pwm(PyObject *self, PyObject *args)
{
    DeviceObject *device = (DeviceObject *)self;
    int pwm_level;

    if (!PyArg_ParseTuple(args, "i:pwm", &pwm_level))
        return NULL;

    if ((device->flags & DO_FLAGS_DETACHED) != 0)
    {
        PyErr_SetString(cmd_get_exception(), "Device is detached");
        return NULL;
    }

    /* Check the pwm value is within range */
    if (pwm_level < -100 || (pwm_level > 100 && pwm_level != 127))
    {
        PyErr_Format(PyExc_ValueError,
                     "PWM value %d out of range",
                     pwm_level);
        return NULL;
    }

    if (cmd_set_pwm(port_get_id(device->port), pwm_level) < 0)
        return NULL;

    Py_RETURN_NONE;
}


/* Handle the internals around a simple mode setting, since it
 * too now happens in more than one place.
 */
static int set_simple_mode(DeviceObject *device, int mode)
{
    mode_info_t *mode_info;

    if (cmd_set_mode(port_get_id(device->port), mode) < 0)
        return -1;
    device->current_mode = mode;

    /* Set up list for handling input values */
    mode_info = &device->modes[mode];
    if (mode_info->format.datasets != PyList_Size(device->values))
    {
        /* Need to resize the list */
        PyObject *new_list = PyList_New(mode_info->format.datasets);
        PyObject *old_list;
        int i;

        if (new_list == NULL)
            return -1;
        for (i = 0; i < mode_info->format.datasets; i++)
            PyList_SET_ITEM(new_list, i, Py_None);
        old_list = device->values;
        device->values = new_list;
        Py_XDECREF(old_list);
    }
    return 0;
}


/* Handle the internals around a combi-mode setting, since it is
 * quite involved and happens in more than one place.
 */
static int set_combi_mode(DeviceObject *device,
                          int combi_index,
                          uint8_t mode_and_dataset[MAX_DATASETS],
                          int num_entries)
{
    if (cmd_set_combi_mode(port_get_id(device->port),
                           combi_index,
                           mode_and_dataset,
                           num_entries) < 0)
        return -1;

    memcpy(device->combi_mode, mode_and_dataset, MAX_DATASETS);
    device->num_combi_modes = num_entries;
    device->combi_index = combi_index;
    device->current_mode = MODE_IS_COMBI;

    /* Set up the list for handling input values */
    {
        PyObject *new_list = PyList_New(num_entries);
        PyObject *old_list;
        int i;

        if (new_list == NULL)
            return -1;
        for (i = 0; i < num_entries; i++)
            PyList_SET_ITEM(new_list, i, Py_None);
        old_list = device->values;
        device->values = new_list;
        Py_XDECREF(old_list);
    }

    return 0;
}



static int
get_mode_info(DeviceObject *device, uint8_t port_id)
{
    port_modes_t mode;
    default_mode_t *default_mode;
    int i;

    if (cmd_get_port_modes(port_id, &mode) < 0)
    {
        return -1;
    }
    device->input_mode_mask = mode.input_mode_mask;
    device->output_mode_mask = mode.output_mode_mask;
    device->num_modes = mode.count;
    if ((mode.capabilities & CAP_MODE_COMBINABLE) != 0)
    {
        combi_mode_t combi_modes;

        device->flags |= DO_FLAGS_COMBINABLE;
        if (cmd_get_combi_modes(port_id, combi_modes) < 0)
            return -1;
        memcpy(device->combi_modes, combi_modes, sizeof(combi_mode_t));
    }

    for (i = 0; i < device->num_modes; i++)
    {
        mode_info_t *mode = &device->modes[i];

        if (cmd_get_mode_name(port_id, i, mode->name) < 0 ||
            cmd_get_mode_raw(port_id, i,
                             &mode->raw.min,
                             &mode->raw.max) < 0 ||
            cmd_get_mode_percent(port_id, i,
                                 &mode->percent.min,
                                 &mode->percent.max) < 0 ||
            cmd_get_mode_si(port_id, i,
                            &mode->si.min,
                            &mode->si.max) < 0 ||
            cmd_get_mode_symbol(port_id, i, mode->symbol) < 0 ||
            cmd_get_mode_mapping(port_id, i,
                                 &mode->input_mapping,
                                 &mode->output_mapping) < 0 ||
            cmd_get_mode_capability(port_id, i,
                                    mode->capability) < 0 ||
            cmd_get_mode_format(port_id, i, &mode->format) < 0)
        {
            return -1;
        }
    }

    device->flags |= DO_FLAGS_GOT_MODE_INFO;

    /* The Micropython runtime sets some devices to more user-friendly modes
     */
    if ((default_mode = get_default_mode(device->type_id)) != NULL)
    {
        int combi_index;

        for (combi_index = 0;
             combi_index < MAX_COMBI_MODES;
             combi_index++)
        {
            if (device->combi_modes[combi_index] == 0)
                break;  /* Run out of possible modes */
            if ((device->combi_modes[combi_index] &
                 default_mode->mode_mask) == default_mode->mode_mask)
            {
                /* This has all the modes we need */
                if (set_combi_mode(device, combi_index,
                                   default_mode->mode_list,
                                   default_mode->num_modes) < 0)
                    return -1;
                break;
            }
        }

        /* Assert that any formally recognised motor is set up to Get
         * the Speed, Position, Absolute Position and Power, and needs
         * to have its delta (preset) calculated.
         */
        if (motor_is_motor(device->type_id))
        {
            long position_from_zero;

            if (get_value(device) < 0)
                return -1;
            if (extract_value(device, 2, &position_from_zero) < 0)
                return -1;
            if (port_set_motor_preset(device->port, position_from_zero) < 0)
                return -1;
        }
    }

    return 0;
}


int
device_ensure_mode_info(PyObject *self)
{
    DeviceObject *device = (DeviceObject *)self;

    if ((device->flags & DO_FLAGS_GOT_MODE_INFO) == 0)
        return get_mode_info(device, port_get_id(device->port));

    return 0;
}


static PyObject *
Device_mode(PyObject *self, PyObject *args)
{
    DeviceObject *device = (DeviceObject *)self;
    PyObject *arg1 = NULL;
    PyObject *arg2 = NULL;

    if ((device->flags & DO_FLAGS_DETACHED) != 0)
    {
        PyErr_SetString(cmd_get_exception(), "Device is detached");
        return NULL;
    }
    if (!PyArg_ParseTuple(args, "|OO:mode", &arg1, &arg2))
        return NULL;
    if (device_ensure_mode_info(self) < 0)
        return NULL;

    if (arg1 == NULL)
    {
        PyObject *results;
        int i;

        if (device->current_mode != MODE_IS_COMBI)
        {
            /* Return the current mode and dataset(s) */
            mode_info_t *mode;

            mode = &device->modes[device->current_mode];

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

        /* Else this is a combi mode */
        if ((results = PyList_New(device->num_combi_modes)) == NULL)
            return NULL;
        for (i = 0; i < device->num_combi_modes; i++)
        {
            PyObject *entry = Py_BuildValue("(ii)",
                                            (device->combi_mode[i] >> 4) &0xf,
                                            device->combi_mode[i] & 0x0f);

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

        /* Check the type conversion worked */
        if (PyErr_Occurred() != NULL)
            return NULL;

        if (mode < 0 || mode >= device->num_modes)
        {
            PyErr_SetString(PyExc_ValueError, "Invalid mode number");
            return NULL;
        }

        if (set_simple_mode(device, mode) < 0)
            return NULL;

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
        PyErr_SetString(
            PyExc_TypeError,
            "Second arg to mode() must be a bytes or bytearray object");
        return NULL;
    }

    /* XXX: consider checking for an iterable rather than a list */
    else if (PyList_Check(arg1))
    {
        /* Else we have been given a combi mode.  This enters our
         * lives as a list of (mode, dataset) 2-tuples.  Validate all
         * the args before we even try changing modes.
         */
        uint8_t mode_and_dataset[MAX_DATASETS];
        int i;
        int num_entries;
        int combi_index;
        uint16_t mode_map = 0;

        if ((num_entries = PyList_Size(arg1)) > MAX_DATASETS)
        {
            PyErr_SetString(PyExc_ValueError,
                            "Too many items for a combination mode");
            return NULL;
        }
        memset(mode_and_dataset, 0, MAX_DATASETS);
        for (i = 0; i < num_entries; i++)
        {
            PyObject *entry = PyList_GET_ITEM(arg1, i);
            PyObject *mode_obj, *dataset_obj;
            long mode, dataset;

            if (!PyTuple_Check(entry) || PyTuple_Size(entry) != 2)
            {
                PyErr_SetString(
                    PyExc_TypeError,
                    "Invalid combination mode: must be a list of 2-tuples of ints");
                return NULL;
            }
            mode_obj = PyTuple_GET_ITEM(entry, 0);
            dataset_obj = PyTuple_GET_ITEM(entry, 1);
            if (!PyLong_Check(mode_obj) || !PyLong_Check(dataset_obj))
            {
                PyErr_SetString(
                    PyExc_TypeError,
                    "Invalid combination mode: must be a list of 2-tuples of ints");
                return NULL;
            }
            if ((mode = PyLong_AsLong(mode_obj)) == -1 &&
                PyErr_Occurred() != NULL)
                return NULL;
            if ((dataset = PyLong_AsLong(dataset_obj)) == -1 &&
                PyErr_Occurred() != NULL)
                return NULL;
            if (mode < 0 ||
                mode >= device->num_modes ||
                dataset < 0 ||
                dataset >= device->modes[mode].format.datasets)
            {
                PyErr_Format(PyExc_ValueError,
                             "Invalid mode/dataset combination (%d/%d)",
                             mode, dataset);
                return NULL;
            }

            /* This one is valid.  Hoorah. */
            mode_and_dataset[i] = (mode << 4) | dataset;
            mode_map |= (1 << mode);
        }

        /* Check if this combination is permitted */
        for (combi_index = 0; combi_index < MAX_COMBI_MODES; combi_index++)
        {
            if (device->combi_modes[combi_index] == 0)
            {
                /* We haven't found a match */
                PyErr_SetString(PyExc_ValueError, "Invalid mode combination");
                return NULL;
            }
            if ((device->combi_modes[combi_index] & mode_map) == mode_map)
                break; /* Yes, this combination is permitted */
        }
        if (combi_index == MAX_COMBI_MODES)
        {
            /* We haven't found a match */
            PyErr_SetString(PyExc_ValueError, "Invalid mode combination");
            return NULL;
        }

        if (set_combi_mode(device,
                           combi_index,
                           mode_and_dataset,
                           num_entries) < 0)
            return NULL;

        Py_RETURN_NONE;
    }

    PyErr_SetString(PyExc_TypeError, "Mode must be an int or list of 2-tuples");
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


static PyObject *convert_raw(PyObject *value, int format, mode_info_t *mode)
{
    if (PyLong_Check(value))
    {
        long long_value = PyLong_AsLong(value);

        if (long_value == -1 && PyErr_Occurred() != NULL)
            return NULL;

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
        return PyLong_FromLong(long_value);
    }
    else if (PyFloat_Check(value))
    {
        float fvalue = (float)PyFloat_AsDouble(value);

        if (fvalue == -1.0 && PyErr_Occurred() != NULL)
            return NULL;

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
        return PyFloat_FromDouble((double)fvalue);
    }
    else if (value == Py_None)
    {
        Py_RETURN_NONE;
    }

    /* Otherwise this is unexpected */
    PyErr_SetString(cmd_get_exception(), "Invalid value");
    return NULL;
}


static int get_value(DeviceObject *device)
{
    device->rx_error = DO_RXERR_NONE;
    if (cmd_get_port_value(port_get_id(device->port)) < 0)
    {
        PyObject *hub_protocol_exception = cmd_get_exception();

        if (PyErr_Occurred() != NULL &&
            PyErr_ExceptionMatches(hub_protocol_exception) &&
            device->rx_error != DO_RXERR_NONE)
        {
            /* Replace the error with something more informative */
            PyErr_Clear();
            switch (device->rx_error)
            {
                case DO_RXERR_NO_MODE_INFO:
                    PyErr_SetString(
                        hub_protocol_exception,
                        "Mode information not ready, try again");
                    break;

                case DO_RXERR_BAD_MODE:
                    PyErr_SetString(
                        hub_protocol_exception,
                        "Inconsistent mode information, please set mode");
                    break;

                case DO_RXERR_INTERNAL:
                    PyErr_SetString(hub_protocol_exception,
                                    "Internal error on reception");
                    break;

                case DO_RXERR_BAD_FORMAT:
                    PyErr_SetString(hub_protocol_exception,
                                    "Format error in received value");
                    break;

                default:
                    PyErr_Format(
                        hub_protocol_exception,
                        "Unknown error (%d) receiving value",
                        device->rx_error);
            }
        }
        return -1;
    }

    return 0;
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

    if ((device->flags & DO_FLAGS_DETACHED) != 0)
    {
        PyErr_SetString(cmd_get_exception(), "Device is detached");
        return NULL;
    }

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

    if (device_ensure_mode_info(self) < 0)
        return NULL;
    if (get_value(device) < 0)
        return NULL;

    /* "format" now contains the format code to use: Raw (0), Pct (1) or
     * SI (2).
     */

    if (device->current_mode != MODE_IS_COMBI)
    {
        /* Simple (single) mode */
        /* Get the current mode data */
        mode = &device->modes[device->current_mode];
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
            value = convert_raw(value, format, mode);
            if (value == NULL)
            {
                Py_DECREF(results);
                return NULL;
            }
            PyList_SET_ITEM(results, i, value);
        }
    }
    else
    {
        /* Combination mode */
        if ((results = PyList_New(device->num_combi_modes)) == NULL)
            return NULL;
        for (i = 0; i < device->num_combi_modes; i++)
        {
            PyObject *value;
            uint8_t mode_number = (device->combi_mode[i] >> 4) & 0x0f;

            mode = &device->modes[mode_number];
            value = PyList_GetItem(device->values, i);
            value = convert_raw(value, format, mode);
            if (value == NULL)
            {
                Py_DECREF(results);
                return NULL;
            }
            PyList_SET_ITEM(results, i, value);
        }
    }

    return results;
}

static PyObject *
Device_callback(PyObject *self, PyObject *args)
{
    DeviceObject *device = (DeviceObject *)self;
    PyObject *callable = NULL;

    if ((device->flags & DO_FLAGS_DETACHED) != 0)
    {
        PyErr_SetString(cmd_get_exception(), "Device is detached");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "|O:callback", &callable))
        return NULL;

    if (callable == NULL)
    {
        /* JUst wants the current callback returned */
        Py_INCREF(device->callback);
        return device->callback;
    }
    if (callable != Py_None && !PyCallable_Check(callable))
    {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    Py_INCREF(callable);
    Py_XDECREF(device->callback);
    device->callback = callable;

    Py_RETURN_NONE;
}


/* Called from callback thread */
int device_callback(PyObject *self, int event)
{
    DeviceObject *device = (DeviceObject *)self;
    PyGILState_STATE gstate = PyGILState_Ensure();
    int rv = 0;

    if (device->callback != Py_None)
    {
        PyObject *args = Py_BuildValue("(O)", device->values);

        rv = (PyObject_CallObject(device->callback, args) != NULL) ? 0 : -1;
        Py_XDECREF(args);
    }

    PyGILState_Release(gstate);

    return rv;
}

static PyMethodDef Device_methods[] = {
    { "pwm", Device_pwm, METH_VARARGS, "Set the PWM level for the port" },
    { "mode", Device_mode, METH_VARARGS, "Get or set the current mode" },
    { "get", Device_get, METH_VARARGS, "Get a set of readings from the device" },
    { "callback", Device_callback, METH_VARARGS, "Get or set the callback" },
    { NULL, NULL, 0, NULL }
};


static PyObject *
Device_get_constant(DeviceObject *self, void *closure)
{
    return PyLong_FromVoidPtr(closure);
}


static PyGetSetDef Device_getsetters[] =
{
    {
        "FORMAT_RAW",
        (getter)Device_get_constant,
        NULL,
        "Format giving raw values from the device",
        (void *)DEVICE_FORMAT_RAW
    },
    {
        "FORMAT_PCT",
        (getter)Device_get_constant,
        NULL,
        "Format giving percentage values from the device",
        (void *)DEVICE_FORMAT_PERCENT
    },
    {
        "FORMAT_SI",
        (getter)Device_get_constant,
        NULL,
        "Format giving SI unit values from the device",
        (void *)DEVICE_FORMAT_SI
    },
    { NULL }
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
    .tp_getset = Device_getsetters,
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


PyObject *device_new_device(PyObject *port,
                            uint16_t type_id,
                            uint8_t *hw_revision,
                            uint8_t *fw_revision)
{
    PyObject *self;
    DeviceObject *device;
    PyObject *version;

    self = PyObject_CallFunctionObjArgs((PyObject *)&DeviceType, port, NULL);
    if (self == NULL)
        return NULL;
    device = (DeviceObject *)self;
    device->type_id = type_id;

    version = cmd_version_as_unicode(hw_revision);
    Py_XDECREF(device->hw_revision);
    device->hw_revision = version;

    version = cmd_version_as_unicode(fw_revision);
    Py_XDECREF(device->fw_revision);
    device->fw_revision = version;

    return self;
}


PyObject *device_get_info(PyObject *self, uint8_t port_id)
{
    DeviceObject *device = (DeviceObject *)self;
    PyObject *results;
    PyObject *mode_list;
    int i;

    if ((device->flags & DO_FLAGS_GOT_MODE_INFO) == 0)
    {
        if (get_mode_info(device, port_id) < 0)
            return NULL;
    }

    /* XXX: missing the "speed" key */
    results = Py_BuildValue("{sisOsO}",
                            "type", device->type_id,
                            "fw_version", device->fw_revision,
                            "hw_version", device->hw_revision);
    if (results == NULL)
        return NULL;

    if ((mode_list = PyList_New(0)) == NULL)
    {
        Py_DECREF(results);
        return NULL;
    }

    for (i = 0; i < device->num_modes; i++)
    {
        PyObject *mode_entry;

        /* The "mode number" used for mode accesses is in fact the
         * index of the mode in the mode list.
         */
        mode_entry = Py_BuildValue(
            "{sss(ff)s(ff)s(ff)sssBsBsy#s{sBsBsBsB}}",
            "name",
            device->modes[i].name,
            "raw",
            device->modes[i].raw.min,
            device->modes[i].raw.max,
            "pct",
            device->modes[i].percent.min,
            device->modes[i].percent.max,
            "si",
            device->modes[i].si.min,
            device->modes[i].si.max,
            "symbol",
            device->modes[i].symbol,
            "map_out",
            device->modes[i].output_mapping,
            "map_in",
            device->modes[i].input_mapping,
            "capability",
            device->modes[i].capability, 6,
            "format",
            "datasets",
            device->modes[i].format.datasets,
            "figures",
            device->modes[i].format.figures,
            "decimals",
            device->modes[i].format.decimals,
            "type",
            device->modes[i].format.type);
        if (mode_entry == NULL)
        {
            Py_DECREF(mode_list);
            Py_DECREF(results);
            return NULL;
        }
        if (PyList_Append(mode_list, mode_entry) < 0)
        {
            Py_DECREF(mode_entry);
            Py_DECREF(mode_list);
            Py_DECREF(results);
            return NULL;
        }
    }
    if (PyDict_SetItemString(results, "modes", mode_list) < 0)
    {
        Py_DECREF(mode_list);
        Py_DECREF(results);
        return NULL;
    }

    if ((device->flags & DO_FLAGS_COMBINABLE) != 0)
    {
        /* First count the number of valid entries */
        int combi_count;

        for (i = 0; i < 8; i++)
            if (device->combi_modes[i] == 0)
                break;

        if (i == 0)
        {
            device->flags &= ~DO_FLAGS_COMBINABLE;
            if ((mode_list = PyTuple_New(0)) == NULL)
            {
                Py_DECREF(results);
                return NULL;
            }
        }
        else
        {
            combi_count = i;
            if ((mode_list = PyTuple_New(combi_count)) == NULL)
            {
                Py_DECREF(results);
                return NULL;
            }
            for (i = 0; i < combi_count; i++)
            {
                PyObject *combination =
                    PyLong_FromUnsignedLong(device->combi_modes[i]);

                if (combination == NULL)
                {
                    Py_DECREF(results);
                    Py_DECREF(mode_list);
                    return NULL;
                }
                if (PyTuple_SetItem(mode_list, i, combination) < 0)
                {
                    Py_DECREF(results);
                    Py_DECREF(mode_list);
                    Py_DECREF(combination);
                    return NULL;
                }
            }
        }
    }
    else
    {
        if ((mode_list = PyTuple_New(0)) == NULL)
        {
            Py_DECREF(results);
            return NULL;
        }
    }
    if (PyDict_SetItemString(results, "combi_modes", mode_list) < 0)
    {
        Py_DECREF(mode_list);
        Py_DECREF(results);
        return NULL;
    }


    return results;
}


static int read_value(uint8_t format_type,
                      uint8_t *buffer,
                      uint16_t nbytes,
                      PyObject **pvalue)
{
    *pvalue = NULL;
    switch (format_type)
    {
        case FORMAT_8BIT:
            if (nbytes < 1)
                return -1;
            if ((*pvalue = PyLong_FromLong((int8_t)buffer[0])) == NULL)
                return -1;
            return 1;

        case FORMAT_16BIT:
            if (nbytes < 2)
                return -1;
            *pvalue = PyLong_FromLong((int16_t)(buffer[0] |
                                                (buffer[1] << 8)));
            if (*pvalue == NULL)
                return -1;
            return 2;

        case FORMAT_32BIT:
            if (nbytes < 4)
                return -1;
            *pvalue = PyLong_FromLong((int32_t)(buffer[0] |
                                                (buffer[1] << 8) |
                                                (buffer[2] << 16) |
                                                (buffer[3] << 24)));
            if (*pvalue == NULL)
                return -1;
            return 4;

        case FORMAT_FLOAT:
        {
            uint32_t bytes;
            float fvalue;

            if (nbytes < 4)
                return -1;
            bytes = (buffer[0] |
                     (buffer[1] << 8) |
                     (buffer[2] << 16) |
                     (buffer[3] << 24));
            memcpy(&fvalue, &bytes, 4);
            if ((*pvalue = PyFloat_FromDouble((double)fvalue)) == NULL)
                return -1;
            return 4;
        }

        default:
            break;
    }
    return -1;
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
    //if ((device->flags & DO_FLAGS_GOT_MODE_INFO) == 0)
    //{
    //    /* The foreground has to be the one to send commands */
    //    device->rx_error = DO_RXERR_NO_MODE_INFO;
    //    return -1;
    //}

    if (device->current_mode == MODE_IS_COMBI)
    {
        /* This must be some sort of race condition */
        device->rx_error = DO_RXERR_BAD_MODE;
        return -1;
    }

    mode = &device->modes[device->current_mode];

    /* Construct the list to contain these results */
    if ((values = PyList_New(mode->format.datasets)) == NULL)
    {
        device->rx_error = DO_RXERR_INTERNAL;
        return -1;
    }

    for (i = 0; i < mode->format.datasets; i++)
    {
        int nread = read_value(mode->format.type, buffer, nbytes, &value);

        if (nread < 0)
        {
            Py_DECREF(values);
            device->rx_error = DO_RXERR_BAD_FORMAT;
            return -1;
        }
        buffer += nread;
        nbytes -= nread;
        bytes_consumed += nread;
        PyList_SET_ITEM(values, i, value);
    }

    /* Swap the new data into place */
    PyObject *old_values = device->values;
    device->values = values;
    Py_XDECREF(old_values);

    return bytes_consumed;
}


int device_new_combi_value(PyObject *self,
                           int entry,
                           uint8_t *buffer,
                           uint16_t nbytes)
{
    DeviceObject *device = (DeviceObject *)self;
    PyObject *values;
    PyObject *value;
    uint8_t mode_number;
    mode_info_t *mode;
    int bytes_consumed;
    int i;

    device->is_mode_busy = 0;

    //if ((device->flags & DO_FLAGS_GOT_MODE_INFO) == 0)
    //{
    //    /* The foreground has to be the one to send commands */
    //    device->rx_error = DO_RXERR_NO_MODE_INFO;
    //    return -1;
    //}

    if (device->current_mode != MODE_IS_COMBI)
    {
        /* A combined value in a simple mode is probably a race */
        device->rx_error = DO_RXERR_BAD_MODE;
        return -1;
    }

    mode_number = (device->combi_mode[entry] >> 4) & 0x0f;
    mode = &device->modes[mode_number];

    bytes_consumed = read_value(mode->format.type, buffer, nbytes, &value);
    if (bytes_consumed < 0)
    {
        device->rx_error = DO_RXERR_BAD_FORMAT;
        return -1;
    }

    /* This is not terribly efficient... */
    if ((values = PyList_New(device->num_combi_modes)) == NULL)
    {
        Py_DECREF(value);
        device->rx_error = DO_RXERR_INTERNAL;
        return -1;
    }

    for (i = 0; i < device->num_combi_modes; i++)
    {
        if (i == entry)
        {
            Py_INCREF(value); /* So we can do error recovery safely */
            PyList_SET_ITEM(values, i, value);
        }
        else
        {
            PyObject *old_value = PyList_GetItem(device->values, i);

            if (old_value == NULL)
            {
                Py_DECREF(values);
                Py_DECREF(value);
                device->rx_error = DO_RXERR_INTERNAL;
                return -1;
            }
            Py_INCREF(old_value);
            PyList_SET_ITEM(values, i, old_value);
        }
    }

    /* Swap the new data into place */
    PyObject *old_values = device->values;
    device->values = values;
    Py_XDECREF(old_values);

    return bytes_consumed;
}


int device_new_format(PyObject *self)
{
    DeviceObject *device = (DeviceObject *)self;

    /* A device is considered busy with its mode inbetween sending
     * the Port Input Format message confirming a mode/format change
     * and the next Port Value message.
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


void device_detach(PyObject *self)
{
    DeviceObject *device = (DeviceObject *)self;

    if (device != NULL && self != Py_None)
        device->flags |= DO_FLAGS_DETACHED;
}


int device_is_in_mode(PyObject *self, int mode)
{
    DeviceObject *device = (DeviceObject *)self;
    uint8_t i;

    if (device_ensure_mode_info(self) < 0)
        return -1;
    if (device->current_mode == mode)
        return 1;
    if (device->current_mode != MODE_IS_COMBI)
        return 0;

    /* Search the combi-mode list for the mode, don't care which dataset */
    for (i = 0; i < device->num_combi_modes; i++)
        if (((device->combi_mode[i] >> 4) & 0x0f) == mode)
            return 1;

    return 0;
}


static int extract_value(DeviceObject *device, uint8_t index, long *pvalue)
{
    PyObject *value = PyList_GetItem(device->values, index);

    if (value == NULL)
        return -1;
    if (!PyLong_Check(value))
    {
        PyErr_SetString(cmd_get_exception(), "Invalid value");
        return -1;
    }
    *pvalue = PyLong_AsLong(value);
    if (*pvalue == -1 && PyErr_Occurred() != NULL)
        return -1;

    return 0;
}

/* NB: only call if device_is_in_mode(mode) is true! */
int device_read_mode_value(PyObject *self, int mode, long *pvalue)
{
    DeviceObject *device = (DeviceObject *)self;

    if (get_value(device) < 0)
        return -1;

    if (device->current_mode == mode)
    {
        if (extract_value(device, 0, pvalue) < 0)
            return -1;
    }
    else if (device->current_mode == MODE_IS_COMBI)
    {
        /* Search the combi_list for the mode */
        int index;

        for (index = 0; index < device->num_combi_modes; index++)
            if (((device->combi_mode[index] >> 4) & 0x0f) == mode)
                break;
        if (index == device->num_combi_modes)
        {
            PyErr_SetString(cmd_get_exception(), "Mode not present");
            return -1;
        }

        if (extract_value(device, index, pvalue) < 0)
            return -1;
    }
    else
    {
        PyErr_SetString(cmd_get_exception(), "Mode not present");
        return -1;
    }

    return 0;
}


int device_push_mode(PyObject *self, int mode)
{
    DeviceObject *device = (DeviceObject *)self;

    if (device_ensure_mode_info(self) < 0)
        return -1;

    if (mode < 0 || mode >= device->num_modes)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid mode number");
        return -1;
    }

    /* First stash everything */
    device->saved_current_mode    = device->current_mode;
    device->saved_num_combi_modes = device->num_combi_modes;
    device->saved_combi_index     = device->combi_index;
    memcpy(device->saved_combi_mode, device->combi_mode, MAX_DATASETS);

    if (cmd_set_mode(port_get_id(device->port), mode) < 0)
        return -1;
    device->current_mode = mode;

    if (set_simple_mode(device, mode) < 0)
        return -1;

    return 0;
}


int device_pop_mode(PyObject *self)
{
    DeviceObject *device = (DeviceObject *)self;

    if (device_ensure_mode_info(self) < 0)
        return -1;

    if (device->saved_current_mode == MODE_IS_COMBI)
    {
        if (set_combi_mode(device,
                           device->saved_combi_index,
                           device->saved_combi_mode,
                           device->saved_num_combi_modes) < 0)
            return -1;
    }
    else
    {
        if (set_simple_mode(device, device->saved_current_mode) < 0)
            return -1;
    }

    return 0;
}
