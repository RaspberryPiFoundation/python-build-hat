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
typedef struct min_max_s
{
    float min;
    float max;
} min_max_t;

typedef struct mode_info_s
{
    char name[12];
    char symbol[5];
    uint8_t capability[6];
    uint8_t input_mapping;
    uint8_t output_mapping;
    min_max_t raw;
    min_max_t percent;
    min_max_t si;
    value_format_t format;
} mode_info_t;

typedef struct
{
    PyObject_HEAD
    PyObject *device;
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
        self->callback_fn = Py_None;
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


static PyObject *
Port_callback(PyObject *self, PyObject *args)
{
    PortObject *port = (PortObject *)self;
    PyObject *result = NULL;
    PyObject *temp;

    if (PyArg_ParseTuple(args, "O:callback", &temp))
    {
        if (!PyCallable_Check(temp))
        {
            PyErr_SetString(PyExc_TypeError, "callback must be callable");
            return NULL;
        }
        Py_XINCREF(temp);
        Py_XDECREF(port->callback_fn);
        port->callback_fn = temp;
        /* Now return None */
        Py_INCREF(Py_None);
        result = Py_None;
    }

    return result;
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
        port_modes_t mode;

        if (cmd_get_port_modes(port->port_id, &mode) < 0)
            return NULL;
        port->input_mode_mask = mode.input_mode_mask;
        port->output_mode_mask = mode.output_mode_mask;
        port->num_modes = mode.count;
        if ((mode.capabilities & CAP_MODE_COMBINABLE) != 0)
        {
            combi_mode_t combi_modes;

            port->flags |= PO_FLAGS_COMBINABLE;
            if (cmd_get_combi_modes(port->port_id, combi_modes) < 0)
                return NULL;
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
                                     &mode->output_mapping) ||
                cmd_get_mode_capability(port->port_id, i,
                                        mode->capability) ||
                cmd_get_mode_format(port->port_id, i, &mode->format))
            {
                return NULL;
            }
        }
        port->flags |= PO_FLAGS_GOT_MODE_INFO;
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
         * is either input or output but not both.  XXX: ask Lego
         * the truth of this matter.
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
    .tp_itemsize= 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = Port_new,
    .tp_dealloc = (destructor)Port_dealloc,
    .tp_traverse = (traverseproc)Port_traverse,
    .tp_clear = (inquiry)Port_clear,
    .tp_members = Port_members,
    .tp_methods = Port_methods
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

    port->type_id = 0;
    port->flags = 0; /* Got nothing anymore */
    Py_XDECREF(port->hw_revision);
    port->hw_revision = NULL;
    Py_XDECREF(port->fw_revision);
    port->fw_revision = NULL;

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

