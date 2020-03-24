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
#include "device.h"
#include "motor.h"
#include "pair.h"

/**

.. py:class:: PortSet

    Represents the collection of ports attached to the Hub.

    .. note::

        This class is not actually available to the user.  It is only
        used by the Hub object itself, and comes ready initialized.

    .. py:attribute:: A

        :type: Port

        A ``Port`` instance (see below) representing port A.

    .. py:attribute:: B

        :type: Port

        Represents port B.

    .. py:attribute:: C

        :type: Port

        Represents port C.

    .. py:attribute:: D

        :type: Port

        Represents port D.

    .. py:attribute:: E

        :type: Port

        Represents port E.

    .. py:attribute:: F

        :type: Port

        Represents port F.

    .. py:attribute:: ATTACHED

        :type: int
        :value: 1

        The value passed to the ``Port`` instance callback function
        when a device is attached to the port.

    .. py:attribute:: DETACHED

        :type: int
        :value: 0

        The value passed to the ``Port`` instance callback function
        when a device is detached from the port.

    .. note::

        The original doesn't explicitly acknowledge that this
        intermediate collection object exists.  To avoid confusion, it
        seems better to document it as it really is.  There is also no
        documentation of the ATTACHED and DETACHED attributes.

.. py:class:: Port

    Represents a port on the Hat, which may or may not have a device
    attached to it.  The methods of this class are always available.

    The ports on the hub are able to autodetect the capabilities of
    the device that is plugged in.  When a motor or device is
    detected, then an enhanced set of methods is available through the
    Port.motor or Port.device attribute.

    .. note::

        This class is not actually available to the user.  It is only
        used by the Hub object itself, and comes ready initialized.

    .. py:attribute:: device

        A ``Device`` instance for the device attached to the port, or
        ``None`` if no device is currently attached.

    .. py:attribute:: motor

        A ``Motor`` instance for the motor attached to the port, or
        ``None`` if there is no recognised motor currently attached.

    .. py:method:: info() -> dict

        Returns a dictionary describing the capabilities of the device
        connected to the port.  If the port has nothing plugged in,
        then the result is a dictinoary with only a ``type`` key with
        a value of ``None``.

        A port with a powered-up compatible device plugged in returns
        a dictionary with the following keys:

        * ``type`` : Device type as an integer.
        * ``fw_version`` : Firmware version as a string in the form
          ``MAJOR.MINOR.BUGFIX.BUILD``
        * ``hw_version`` : Hardware version as a string in the form
          ``MAJOR.MINOR.BUGFIX.BUILD``
        * ``combi_modes`` : A tuple of legal mode combinations as
          16-bit unsigned integers.  In each integer, bit ``N``
          corresponds to mode number ``N`` (see below), and the set
          bits indicate modes that can be combined together.
        * ``modes`` : A list of dictionaries representing available
          modes.  Where _mode numbers_ are called for in this
          documentation, they are the index of the mode in this list.

        .. note::

            The original Port.info() dictionary also contains a
            ``speed`` key, containing the maximum baud rate of the
            device connected.  There doesn't appear to be any means
            for this library to acquire that information.

            The original ``fw_version`` and ``hw_version`` values are
            unsigned 32-bit integers rather than human-readable
            strings.  The encoding used is not entirely obvious, and
            it was thought decoding the version into a string might be
            more convenient.

            The original documentation mistakenly claims that the
            ``combi-modes`` value is a list rather than a tuple.  At
            present, combination modes are not (very) implemented.

        Each ``modes`` list item dictionary has the following keys:

        * ``name`` : The name of the mode as a string.
        * ``capability`` : The 48-bit capability as a length 6 byte
          string.
        * ``symbol`` : The SI symbol name to use for the data returned
          by the device in SI format.
        * ``raw`` : The range of the raw data (as returned in RAW
          format) expressed as a 2 element tuple ``(minimum,
          maximum)``.
        * ``pct`` : The range of the percentage data (as returned in
          PCT format) expressed as a 2 element tuple ``(minimum,
          maximum)``.
        * ``si`` : The range of the SI data (as returned in SI format)
          expressed as a 2 element tuple ``(minimum, maximum)``.
        * ``map_out`` : The output mapping bits as an 8 bit value.
        * ``map_in`` : The input mapping bits as an 8 bit value.
        * ``format`` : A dictionary representing the format data for
          this mode.

        The input and output mapping bits are defined as follows:

        ===  =======
        Bit  Meaning
        ===  =======
        7    Supports NULL value
        6    Supports Functional Mapping 2.0+
        5    N/A
        4    ABS (Absolute [min..max])
        3    REL (Relative [-1..1])
        2    DIS (Discrete [0, 1, 2, 3])
        1    N/A
        0    N/A
        ===  =======

        .. note::

            This information was taken from the LEGO Wireless Protocol
            3.0.00 documentation available online.  No further
            documentation of what these fields actually mean is
            currently provided.

        Each ``format`` dictionary has the following keys:

        * ``datasets`` : The number of data values that this mode
          returns.
        * ``figures`` : The number digits in the data value.
        * ``decimals`` : the number of digits after the implied
          decimal point.
        * ``type`` : The type of the returned data, encoded as an
          integer.

        The ``type`` values are defined as follows:

        =====  =======
        Value  Meaning
        =====  =======
        0      8-bit signed integer
        1      16-bit signed integer
        2      32-bit signed integer
        3      IEEE 32-bit floating point
        =====  =======

        .. note::

            There is no available documentation of the Capability
            bits.

    .. py:method:: pwm(value: int) -> None

        Sets the PWM level generated at the port, if output is
        permitted.

        :param int value: The PWM level generated, ranging from -100
            to +100.  The polarity of the PWM signal matches the sign
            of the value.

        :raises ValueError: if the input is greater than 100 or less
            than -100.

        Calling ``pwm(0)`` stops the PWM signal and leaves the port
        driver in a floating state.

        Note that ``value`` is not a keyword argument: calling
        ``pwm(value=42)`` will raise a ``TypeError``.

        .. note::

            Arguably this should only be implemented as a method of
            the Device class, in the same way that
            :py:meth:`Port.mode()` is.

    .. py:method:: callback([fn])

        Sets the function to be called when a device is plugged into
        or removed from this port.

        :param fn: The function to call.  This is a positional
            parameter only.
        :type fn: Callable or None
        :raises TypeError: if ``fn`` is present, not ``None`` and not
            callable.

        If ``fn`` is omitted, the current callback function will be
        returned.  Otherwise ``None`` is returned.

        If ``fn`` is ``None``, the callback will be disabled.

        Otherwise ``fn`` should be a function taking one parameter
        that indicates why the callback was made.  The possible values
        are:

        * :py:const:`PortSet.ATTACHED` (available as
          ``hub.port.ATTACHED``) : indicates that something has been
          plugged into the port.
        * :py:const:`PortSet.DETACHED` : (available as
          ``hub.port.DETACHED``) : indicates that the device
          previously plugged into the port has been removed.

        The callback function is called from a background context,
        which limits what it can do.  In particular, calling any of
        the functions in the hub library is likely to cause problems.
        It is usually best to set a flag variable and deal with the
        event from the foreground.

        Example::

            from hub import hub

            def greet(event):
                if event == hub.port.ATTACHED:
                    print("Hello, new thing on port A")
                else:
                    print("Goodbye, old thing on port A")

            hub.port.A.callback(greet)

    .. py:method:: mode(mode)

        Sets the mode of the device on this port.

        .. caution::

            ``Port.mode()`` is not implemented.  Use
            :py:meth:`Device.mode()` instead.
*/

/* The actual Port type */
typedef struct
{
    PyObject_HEAD
    PyObject *device;
    PyObject *motor;
    PyObject *callback_fn;
    PyObject *hw_revision;
    PyObject *fw_revision;
    uint8_t  port_id;
    uint8_t  flags;
    uint8_t  num_modes;
    uint16_t type_id;
    uint16_t input_mode_mask;
    uint16_t output_mode_mask;
    combi_mode_t combi_modes;
    mode_info_t modes[16];
} PortObject;

#define PO_FLAGS_GOT_MODE_INFO 0x01
#define PO_FLAGS_COMBINABLE    0x02

static int
Port_traverse(PortObject *self, visitproc visit, void *arg)
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
        self->motor = Py_None;
        Py_INCREF(Py_None);
        self->callback_fn = Py_None;
        Py_INCREF(Py_None);
    }
    return (PyObject *)self;
}


/* Make Port.device read-only from Python */
static PyObject *
Port_get_device(PortObject *self, void *closure)
{
    Py_INCREF(self->device);
    return self->device;
}

/* Ditto Port.motor */
static PyObject *
Port_get_motor(PortObject *self, void *closure)
{
    Py_INCREF(self->motor);
    return self->motor;
}


static PyGetSetDef Port_getsetters[] =
{
    {
        "device", (getter)Port_get_device, NULL,
        "Generic device handler", NULL
    },
    {
        "motor", (getter)Port_get_motor, NULL,
        "Handler for motor devices", NULL
    },
    { NULL }
};


static PyObject *
Port_callback(PyObject *self, PyObject *args)
{
    PortObject *port = (PortObject *)self;
    PyObject *temp = NULL;

    if (!PyArg_ParseTuple(args, "O:callback", &temp))
        return NULL;

    if (temp == NULL)
    {
        Py_INCREF(port->callback_fn);
        return port->callback_fn;
    }

    if (temp != Py_None && !PyCallable_Check(temp))
    {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    Py_XINCREF(temp);
    Py_XDECREF(port->callback_fn);
    port->callback_fn = temp;

    Py_RETURN_NONE;
}


static int get_mode_info(PortObject *port)
{
    port_modes_t mode;
    int i;

    if (cmd_get_port_modes(port->port_id, &mode) < 0)
        return -1;
    port->input_mode_mask = mode.input_mode_mask;
    port->output_mode_mask = mode.output_mode_mask;
    port->num_modes = mode.count;
    if ((mode.capabilities & CAP_MODE_COMBINABLE) != 0)
    {
        combi_mode_t combi_modes;

        port->flags |= PO_FLAGS_COMBINABLE;
        if (cmd_get_combi_modes(port->port_id, combi_modes) < 0)
            return -1;
        memcpy(port->combi_modes, combi_modes, sizeof(combi_mode_t));
    }

    for (i = 0; i < port->num_modes; i++)
    {
        mode_info_t *mode = &port->modes[i];

        if (cmd_get_mode_name(port->port_id, i, mode->name) < 0 ||
            cmd_get_mode_raw(port->port_id, i,
                             &mode->raw.min,
                             &mode->raw.max) < 0 ||
            cmd_get_mode_percent(port->port_id, i,
                                 &mode->percent.min,
                                 &mode->percent.max) < 0 ||
            cmd_get_mode_si(port->port_id, i,
                            &mode->si.min,
                            &mode->si.max) < 0 ||
            cmd_get_mode_symbol(port->port_id, i, mode->symbol) < 0 ||
            cmd_get_mode_mapping(port->port_id, i,
                                 &mode->input_mapping,
                                 &mode->output_mapping) < 0 ||
            cmd_get_mode_capability(port->port_id, i,
                                    mode->capability) < 0 ||
            cmd_get_mode_format(port->port_id, i, &mode->format) < 0)
        {
            return -1;
        }
    }

    port->flags |= PO_FLAGS_GOT_MODE_INFO;
    return 0;
}


static PyObject *
Port_info(PyObject *self, PyObject *args)
{
    PortObject *port = (PortObject *)self;
    PyObject *results;
    PyObject *mode_list;
    int i;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    /* Is the port attached to anything */
    if (port->type_id == 0)
        return Py_BuildValue("{sO}", "type", Py_None);

    /* It is attached, build the desired dictionary */
    if ((port->flags & PO_FLAGS_GOT_MODE_INFO) == 0)
    {
        if (get_mode_info(port) < 0)
            return NULL;
    }

    /* XXX: missing the "speed" key */
    results = Py_BuildValue("{sisOsO}",
                            "type", port->type_id,
                            "fw_version", port->fw_revision,
                            "hw_version", port->hw_revision);
    if (results == NULL)
        return NULL;

    if ((mode_list = PyList_New(0)) == NULL)
    {
        Py_DECREF(results);
        return NULL;
    }

    for (i = 0; i < port->num_modes; i++)
    {
        PyObject *mode_entry;

        /* Taking a wild guess here, but the only thing I can see
         * the mode number corresponding to is the position in the
         * mode list.  Assume that that is the case, and each mode
         * is either input or output but not both.
         */
        mode_entry = Py_BuildValue(
            "{sss(ff)s(ff)s(ff)sssBsBsy#s{sBsBsBsB}}",
            "name",
            port->modes[i].name,
            "raw",
            port->modes[i].raw.min,
            port->modes[i].raw.max,
            "pct",
            port->modes[i].percent.min,
            port->modes[i].percent.max,
            "si",
            port->modes[i].si.min,
            port->modes[i].si.max,
            "symbol",
            port->modes[i].symbol,
            "map_out",
            port->modes[i].output_mapping,
            "map_in",
            port->modes[i].input_mapping,
            "capability",
            port->modes[i].capability, 6,
            "format",
            "datasets",
            port->modes[i].format.datasets,
            "figures",
            port->modes[i].format.figures,
            "decimals",
            port->modes[i].format.decimals,
            "type",
            port->modes[i].format.type);
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

    if ((port->flags & PO_FLAGS_COMBINABLE) != 0)
    {
        /* First count the number of valid entries */
        int combi_count;

        for (i = 0; i < 8; i++)
            if (port->combi_modes[i] == 0)
                break;

        if (i == 0)
        {
            port->flags &= ~PO_FLAGS_COMBINABLE;
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
                    PyLong_FromUnsignedLong(port->combi_modes[i]);

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


static PyObject *
Port_mode(PyObject *self, PyObject *args)
{
    PyErr_SetString(PyExc_NotImplementedError,
                    "Port.mode() not implemented: see Port.device.mode()");
    return NULL;
}


static PyObject *
Port_pwm(PyObject *self, PyObject *args)
{
    PortObject *port = (PortObject *)self;
    int pwm_level;

    if (!PyArg_ParseTuple(args, "i:pwm", &pwm_level))
        return NULL;

    /* Check the pwm value is within range */
    if (pwm_level < -100 || (pwm_level > 100 && pwm_level != 127))
    {
        PyErr_Format(PyExc_ValueError,
                     "PWM value %d out of range",
                     pwm_level);
        return NULL;
    }

    if (cmd_set_pwm(port->port_id, pwm_level) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
Port_repr(PyObject *self)
{
    PortObject *port = (PortObject *)self;

    return PyUnicode_FromFormat("Port(%c)", 'A' + port->port_id);
}


static PyMethodDef Port_methods[] = {
    {
        "callback", Port_callback, METH_VARARGS,
        "Set a callback for plugging or unplugging devices in this port"
    },
    { "info", Port_info, METH_VARARGS, "Information on the attached device" },
    { "mode", Port_mode, METH_VARARGS, "Set the mode for the port" },
    { "pwm", Port_pwm, METH_VARARGS, "Set the PWM level for the port" },
    { NULL, NULL, 0, NULL }
};


static PyTypeObject PortType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Port",
    .tp_doc = "An individual port",
    .tp_basicsize = sizeof(PortObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = Port_new,
    .tp_dealloc = (destructor)Port_dealloc,
    .tp_traverse = (traverseproc)Port_traverse,
    .tp_clear = (inquiry)Port_clear,
    .tp_getset = Port_getsetters,
    .tp_methods = Port_methods,
    .tp_repr = Port_repr
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
            /* TODO: pass the port ID to the constructor */
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
            ((PortObject *)self->ports[i])->port_id = i;
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


static PyObject *
PortSet_get_constant(PortSetObject *self, void *closure)
{
    return PyLong_FromVoidPtr(closure);
}


static PyGetSetDef PortSet_getsetters[] =
{
    { "A", (getter)PortSet_get_port, NULL, "Port A", (void *)0 },
    { "B", (getter)PortSet_get_port, NULL, "Port B", (void *)1 },
    { "C", (getter)PortSet_get_port, NULL, "Port C", (void *)2 },
    { "D", (getter)PortSet_get_port, NULL, "Port D", (void *)3 },
    { "E", (getter)PortSet_get_port, NULL, "Port E", (void *)4 },
    { "F", (getter)PortSet_get_port, NULL, "Port F", (void *)5 },
    {
        "ATTACHED",
        (getter)PortSet_get_constant,
        NULL,
        "Value passed to callback when port is attached",
        (void *)1
    },
    {
        "DETACHED",
        (getter)PortSet_get_constant,
        NULL,
        "Value passed to callback when port is detached",
        (void *)0
    },
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

int port_modinit(void)
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
    return 0;
}


void port_demodinit(void)
{
    Py_DECREF(&PortType);
    Py_DECREF(&PortSetType);
}


PyObject *port_init(void)
{
    port_set =
        (PortSetObject *)PyObject_CallObject((PyObject *)&PortSetType, NULL);
    Py_INCREF(port_set);
    return (PyObject *)port_set;
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
    PyObject *device = device_new_device((PyObject *)port);
    int rv = 0;

    if (device == NULL)
        return -1;
    if (motor_is_motor(type_id))
    {
        PyObject *motor = motor_new_motor((PyObject *)port, device);

        if (motor == NULL)
        {
            Py_DECREF(device);
            return -1;
        }

        Py_DECREF(port->motor);
        port->motor = motor;
    }
    else
    {
        Py_XDECREF(port->motor);
        port->motor = Py_None;
        Py_INCREF(Py_None);
    }

    Py_DECREF(port->device);
    port->device = device;
    port->type_id = type_id;
    port->flags = 0; /* Got nothing useful yet */
    version = cmd_version_as_unicode(hw_revision);
    Py_XDECREF(port->hw_revision);
    port->hw_revision = version;
    version = cmd_version_as_unicode(fw_revision);
    Py_XDECREF(port->fw_revision);
    port->fw_revision = version;

    if (port->callback_fn != Py_None)
    {
        arg_list = Py_BuildValue("(i)", 1); /* ATTACHED */
        rv = (PyObject_CallObject(port->callback_fn,
                                  arg_list) != NULL) ? 0 : -1;
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

    pair_detach_subport(port_id);

    port->type_id = 0;
    port->flags = 0; /* Got nothing anymore */
    Py_XDECREF(port->hw_revision);
    port->hw_revision = NULL;
    Py_XDECREF(port->fw_revision);
    port->fw_revision = NULL;
    Py_XDECREF(port->device);
    port->device = Py_None;
    Py_INCREF(Py_None);
    Py_XDECREF(port->motor);
    port->motor = Py_None;
    Py_INCREF(Py_None);

    if (port->callback_fn != Py_None)
    {
        arg_list = Py_BuildValue("(i)", 0); /* DETATCHED */
        rv = (PyObject_CallObject(port->callback_fn,
                                  arg_list) != NULL) ? 0 : 1;
    }

    /* Release the penguins^WGIL */
    PyGILState_Release(gstate);

    return rv;
}


/* Called from the background context */
int port_new_value(uint8_t port_id, uint8_t *buffer, uint16_t nbytes)
{
    /* Some or all of the buffer will be the values we crave */
    PyGILState_STATE gstate = PyGILState_Ensure();
    PortObject *port = (PortObject *)port_set->ports[port_id];
    int rv;

    if (port->device == Py_None)
    {
        /* We don't think we have anything attached.  Unfortunately
         * this means we cannot tell how much of the buffer was the
         * value for this port.  Options are:
         *
         * a) Assume this was a simple case, consume the whole buffer
         * and return success, since this is most likely something
         * happening in an unhelpful order, or
         *
         * b) Report an error and get the lower levels to abandon
         * processing.
         *
         * We'll go with the error for safety.
         */
        rv = -1;
    }
    else
    {
        rv = device_new_value(port->device, buffer, nbytes);
    }

    PyGILState_Release(gstate);
    return rv;
}


/* Called from the background context */
int port_new_format(uint8_t port_id)
{
    /* Don't need to call any Python functions off this */
    PortObject *port = (PortObject *)port_set->ports[port_id];

    if (port->device == Py_None)
    {
        /* We don't think anything is attached.  See port_new_value()
         * for why this is more of a problem than you would hope.
         */
        return -1;
    }
    return device_new_format(port->device);
}


/* Called from the background context */
int port_feedback_status(uint8_t port_id, uint8_t status)
{
    PortObject *port = (PortObject *)port_set->ports[port_id];

    if (port->device == Py_None)
    {
        /* We don't think anything is attached.  How odd. */
        return -1;
    }
    /* The documentation of the status byte is somewhat confusing.
     * Judging from the Flipper code, the bits actually mean the
     * following:
     *
     * Bit 0: BUSY: the device is working on a command
     * Bit 1: COMPLETE: a command completed
     * Bit 2: DISCARDED: a command was discarded
     * Bit 3: IDLE: unclear
     * Bit 4: BUSY/FULL: unclear
     * Bit 5: STALL: the device has stalled
     */
    if (port->motor != Py_None)
    {
        if ((status & 0x02) != 0)
            motor_callback(port->motor, CALLBACK_COMPLETE);
        if ((status & 0x04) != 0)
            motor_callback(port->motor,
                           ((status & 0x20) != 0) ? CALLBACK_STALLED :
                           CALLBACK_INTERRUPTED);
    }
    return device_set_port_busy(port->device, status & 0x01);
}


int
port_get_id(PyObject *port)
{
    return ((PortObject *)port)->port_id;
}


PyObject *
port_get_device(PyObject *port)
{
    PortObject *self = (PortObject *)port;

    Py_INCREF(self->device);
    return self->device;
}


int
port_ensure_mode_info(PyObject *port)
{
    PortObject *self = (PortObject *)port;

    if (self->type_id == 0)
    {
        PyErr_SetString(cmd_get_exception(), "Unexpectedly detached");
        return -1;
    }

    if ((self->flags & PO_FLAGS_GOT_MODE_INFO) == 0)
        if (get_mode_info(self) < 0)
            return -1;

    return 0;
}


mode_info_t *
port_get_mode(PyObject *port, int mode)
{
    PortObject *self = (PortObject *)port;

    if (port_ensure_mode_info(port) < 0)
        return NULL;

    if (mode < 0 || mode >= self->num_modes)
    {
        PyErr_Format(PyExc_ValueError, "Bad mode number %d", mode);
        return NULL;
    }

    return &self->modes[mode];
}


int
port_check_mode(PyObject *port, int mode)
{
    PortObject *self = (PortObject *)port;

    if (port_ensure_mode_info(port) < 0)
        return -1;

    return (mode >= 0) && (mode < self->num_modes);
}


static PyObject *
port_get_value_list(PyObject *port)
{
    PortObject *self = (PortObject *)port;

    if (self->device == Py_None)
        return PyList_New(0);
    return PyObject_CallMethod(self->device, "get", NULL);
}


PyObject *
ports_get_value_dict(PyObject *port_set)
{
    PortSetObject *self = (PortSetObject *)port_set;
    int i;
    PyObject *dict = PyDict_New();
    char name[2];

    if (dict == NULL)
        return NULL;

    /* 'name' will be used to generate the port letter string */
    name[1] = '\0';

    for (i = 0; i < NUM_HUB_PORTS; i++)
    {
        PyObject *value = port_get_value_list(self->ports[i]);

        if (value == NULL)
        {
            Py_DECREF(dict);
            return NULL;
        }
        name[0] = 'A' + i;
        if (PyDict_SetItemString(dict, name, value) < 0)
        {
            Py_DECREF(value);
            Py_DECREF(dict);
            return NULL;
        }
    }

    return dict;
}


PyObject *
port_get_motor(PyObject *port)
{
    return Port_get_motor((PortObject *)port, NULL);
}
