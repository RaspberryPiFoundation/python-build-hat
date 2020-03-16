/* pair.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Python class for handling a pair of motors
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>

#include "pair.h"
#include "port.h"
#include "cmd.h"


#define INVALID_ID (0xff)


/* The actual motor pair type */
typedef struct
{
    PyObject_HEAD
    PyObject *primary;
    PyObject *secondary;
    PyObject *callback_fn;
    uint32_t default_acceleration;
    uint32_t default_deceleration;
    uint32_t default_position_pid[3];
    int want_default_acceleration_set;
    int want_default_deceleration_set;
    uint8_t id;
    uint8_t primary_id;
    uint8_t secondary_id;
    uint16_t device_type;
} MotorPairObject;


#define DEFAULT_ACCELERATION 100
#define DEFAULT_DECELERATION 150

#define USE_PROFILE_ACCELERATE 0x01
#define USE_PROFILE_DECELERATE 0x02

/* Values passed into Python methods */
#define MOTOR_STOP_FLOAT 0
#define MOTOR_STOP_BRAKE 1
#define MOTOR_STOP_HOLD  2
#define MOTOR_STOP_USE_DEFAULT 3

/* Values passed to the cmd functions */
#define STOP_FLOAT 0
#define STOP_HOLD 126
#define STOP_BRAKE 127

#define SPEED_MIN -100
#define SPEED_MAX 100
#define POWER_MIN 0
#define POWER_MAX 100
#define ACCEL_MIN 0
#define ACCEL_MAX 10000
#define DECEL_MIN 0
#define DECEL_MAX 10000
#define RUN_TIME_MIN 0
#define RUN_TIME_MAX 65535

#define CLIP(value,min,max) (((value) > (max)) ? (max) :                \
                             (((value) < (min)) ? (min) : (value)))

#define PAIR_COUNT 6

MotorPairObject *pairs[PAIR_COUNT] = { NULL };


/* Utility functions for dealing with common parameters */
static int parse_stop(uint32_t stop)
{
    switch (stop)
    {
        case MOTOR_STOP_FLOAT:
            return STOP_FLOAT;

        case MOTOR_STOP_BRAKE:
        case MOTOR_STOP_USE_DEFAULT:
            return STOP_BRAKE;

        case MOTOR_STOP_HOLD:
            return STOP_HOLD;

        default:
            break;
    }
    return -1;
}


static int set_acceleration(MotorPairObject *pair,
                            uint32_t accel,
                            uint8_t *p_use_profile)
{
    if (accel != pair->default_acceleration)
    {
        pair->want_default_acceleration_set = 1;
        *p_use_profile |= USE_PROFILE_ACCELERATE;
        return cmd_set_acceleration(pair->id, accel);
    }
    if (pair->want_default_acceleration_set)
    {
        if (cmd_set_acceleration(pair->id, pair->default_acceleration) < 0)
            return -1;
        pair->want_default_acceleration_set = 0;
    }
    /* Else the right acceleration value has already been set */
    return 0;
}


static int set_deceleration(MotorPairObject *pair,
                            uint32_t decel,
                            uint8_t *p_use_profile)
{
    if (decel != pair->default_deceleration)
    {
        pair->want_default_deceleration_set = 1;
        *p_use_profile |= USE_PROFILE_DECELERATE;
        return cmd_set_deceleration(pair->id, decel);
    }
    if (pair->want_default_deceleration_set)
    {
        if (cmd_set_deceleration(pair->id, pair->default_deceleration) < 0)
            return -1;
        pair->want_default_deceleration_set = 0;
    }
    /* Else the right deceleration value has already been set */
    return 0;
}


static int
MotorPair_traverse(MotorPairObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->primary);
    Py_VISIT(self->secondary);
    return 0;
}


static int
MotorPair_clear(MotorPairObject *self)
{
    Py_CLEAR(self->primary);
    Py_CLEAR(self->secondary);
    return 0;
}


static void
MotorPair_dealloc(MotorPairObject *self)
{
    PyObject_GC_UnTrack(self);
    MotorPair_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *
MotorPair_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MotorPairObject *self;
    int i;

    /* Find an empty slot */
    for (i = 0; i < PAIR_COUNT; i++)
        if (pairs[i] == NULL)
            break;
    if (i == PAIR_COUNT)
    {
        /* No available slots.  Abort */
        PyErr_SetString(cmd_get_exception(), "Too many motor pairs");
        return NULL;
    }

    if ((self = (MotorPairObject *)type->tp_alloc(type, 0)) != NULL)
    {
        Py_INCREF(self);
        pairs[i] = self;
        self->primary = Py_None;
        Py_INCREF(Py_None);
        self->secondary = Py_None;
        Py_INCREF(Py_None);
        self->callback_fn = Py_None;
        Py_INCREF(Py_None);
        self->id =
            self->primary_id =
            self->secondary_id = INVALID_ID;
        self->default_position_pid[0] =
            self->default_position_pid[1] =
            self->default_position_pid[2] = 0;
        self->default_acceleration = DEFAULT_ACCELERATION;
        self->default_deceleration = DEFAULT_DECELERATION;
        self->want_default_acceleration_set =
            self->want_default_deceleration_set = 0;
    }
    return (PyObject *)self;
}


static int
MotorPair_init(MotorPairObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "primary", "secondary", NULL };
    PyObject *primary = NULL;
    PyObject *secondary = NULL;
    PyObject *tmp;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist,
                                     &primary, &secondary))
        return -1;

    tmp = self->primary;
    Py_INCREF(primary);
    self->primary = primary;
    Py_XDECREF(tmp);

    tmp = self->secondary;
    Py_INCREF(secondary);
    self->secondary = secondary;
    Py_XDECREF(tmp);

    self->primary_id = port_get_id(primary);
    self->secondary_id = port_get_id(secondary);

    if (cmd_connect_virtual_port(self->primary_id, self->secondary_id) < 0)
        return -1;

    return 0;
}


static PyObject *
MotorPair_repr(PyObject *self)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    return PyUnicode_FromFormat("MotorPair(%d:%c%c)",
                                pair->id,
                                'A' + pair->primary_id,
                                'A' + pair->secondary_id);
}


static PyObject *
MotorPair_primary(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    if (pair->id == INVALID_ID)
    {
        PyErr_SetString(cmd_get_exception(),
                        "Motor pair no longer connected");
        return NULL;
    }

    Py_XINCREF(pair->primary);
    return pair->primary;
}


static PyObject *
MotorPair_secondary(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    if (pair->id == INVALID_ID)
    {
        PyErr_SetString(cmd_get_exception(),
                        "Motor pair no longer connected");
        return NULL;
    }

    Py_XINCREF(pair->secondary);
    return pair->secondary;
}


static PyObject *
MotorPair_id(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    return PyLong_FromUnsignedLong(pair->id);
}


static PyObject *
MotorPair_callback(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    PyObject *callable = NULL;

    if (!PyArg_ParseTuple(args, "|O:callback", &callable))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    if (callable == NULL)
    {
        /* Just wants the current callback returned */
        Py_INCREF(pair->callback_fn);
        return pair->callback_fn;
    }
    if (callable != Py_None && !PyCallable_Check(callable))
    {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    Py_XINCREF(callable);
    Py_XDECREF(pair->callback_fn);
    pair->callback_fn = callable;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_unpair(PyObject *self, PyObject *args)
{
    int rv;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    if ((rv = pair_unpair(self)) < 0)
        return NULL;
    else if (rv > 0)
        Py_RETURN_FALSE;

    Py_RETURN_TRUE;
}


static PyObject *
MotorPair_pid(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    /* If we have no parameters, return a tuple of the values */
    if (PyTuple_Size(args) == 0)
    {
        return Py_BuildValue("III",
                             pair->default_position_pid[0],
                             pair->default_position_pid[1],
                             pair->default_position_pid[2]);
    }

    /* Otherwise we should have all three values, and set them */
    if (!PyArg_ParseTuple(args, "III:pid",
                          &pair->default_position_pid[0],
                          &pair->default_position_pid[1],
                          &pair->default_position_pid[2]))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    if (cmd_set_pid(pair->id, pair->default_position_pid) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_float(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    /* float() is equivalent to pwm(0) */

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    if (cmd_set_pwm_pair(pair->id, 0, 0) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_brake(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    /* brake() is equivalent to pwm(127) */

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    if (cmd_set_pwm_pair(pair->id, 127, 127) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_hold(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    int max_power = 100;
    uint8_t use_profile = 0;

    if (!PyArg_ParseTuple(args, "|i", &max_power))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    max_power = CLIP(max_power, POWER_MIN, POWER_MAX);

    if (pair->default_acceleration != 0)
        use_profile |= USE_PROFILE_ACCELERATE;
    if (pair->default_deceleration != 0)
        use_profile |= USE_PROFILE_DECELERATE;

    if (cmd_start_speed_pair(pair->id, 0, 0, max_power, use_profile) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_pwm(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    int pwm0, pwm1;

    if (!PyArg_ParseTuple(args, "ii", &pwm0, &pwm1))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    if (cmd_set_pwm_pair(pair->id, pwm0, pwm1) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_preset(PyObject *self, PyObject *args)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    int32_t position0, position1;

    if (!PyArg_ParseTuple(args, "ii", &position0, &position1))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    if (cmd_preset_encoder_pair(pair->id, position0, position1) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_run_at_speed(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    static char *kwlist[] = {
        "speed0", "speed1", "max_power",
        "acceleration", "deceleration", NULL
    };
    int32_t speed0, speed1;
    uint32_t power = 100;
    uint32_t accel = pair->default_acceleration;
    uint32_t decel = pair->default_deceleration;
    uint8_t use_profile = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "ii|III:run_at_speed", kwlist,
                                     &speed0, &speed1, &power,
                                     &accel, &decel))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    speed0 = CLIP(speed0, SPEED_MIN, SPEED_MAX);
    speed1 = CLIP(speed1, SPEED_MIN, SPEED_MAX);
    power = CLIP(power, POWER_MIN, POWER_MAX);
    accel = CLIP(accel, ACCEL_MIN, ACCEL_MAX);
    decel = CLIP(decel, DECEL_MIN, DECEL_MAX);

    if (set_acceleration(pair, accel, &use_profile) < 0 ||
        set_deceleration(pair, decel, &use_profile) < 0)
        return NULL;

    if (cmd_start_speed_pair(pair->id, speed0, speed1, power, use_profile) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_run_for_time(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    static char *kwlist[] = {
        "msec", "speed0", "speed1", "max_power", "stop",
        "acceleration", "deceleration",
        NULL
    };
    uint32_t time;
    int32_t speed0, speed1;
    uint32_t power = 100;
    uint32_t accel = pair->default_acceleration;
    uint32_t decel = pair->default_deceleration;
    uint32_t stop = MOTOR_STOP_USE_DEFAULT;
    uint8_t use_profile = 0;
    int parsed_stop;

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "Iii|IIII:run_for_time", kwlist,
                                     &time, &speed0, &speed1,
                                     &power, &stop,
                                     &accel, &decel))
        return NULL;

    time = CLIP(time, RUN_TIME_MIN, RUN_TIME_MAX);
    speed0 = CLIP(speed0, SPEED_MIN, SPEED_MAX);
    speed1 = CLIP(speed1, SPEED_MIN, SPEED_MAX);
    power = CLIP(power, POWER_MIN, POWER_MAX);
    accel = CLIP(accel, ACCEL_MIN, ACCEL_MAX);
    decel = CLIP(decel, DECEL_MIN, DECEL_MAX);
    if ((parsed_stop = parse_stop(stop)) < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid stop state");
        return NULL;
    }

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    if (set_acceleration(pair, accel, &use_profile) < 0 ||
        set_deceleration(pair, decel, &use_profile) < 0)
        return NULL;

    if (cmd_start_speed_for_time_pair(pair->id, time,
                                      speed0, speed1, power,
                                      (uint8_t)parsed_stop, use_profile) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_run_for_degrees(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    static char *kwlist[] = {
        "degrees", "speed0", "speed1", "max_power", "stop",
        "acceleration", "deceleration",
        NULL
    };
    int32_t degrees;
    int32_t speed0, speed1;
    uint32_t power = 100;
    uint32_t accel = pair->default_acceleration;
    uint32_t decel = pair->default_deceleration;
    uint32_t stop = MOTOR_STOP_USE_DEFAULT;
    uint8_t use_profile = 0;
    int parsed_stop;

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "iii|IIII:run_for_degrees", kwlist,
                                     &degrees, &speed0, &speed1,
                                     &power, &stop,
                                     &accel, &decel))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    speed0 = CLIP(speed0, SPEED_MIN, SPEED_MAX);
    speed1 = CLIP(speed1, SPEED_MIN, SPEED_MAX);
    power = CLIP(power, POWER_MIN, POWER_MAX);
    accel = CLIP(accel, ACCEL_MIN, ACCEL_MAX);
    decel = CLIP(decel, DECEL_MIN, DECEL_MAX);
    if ((parsed_stop = parse_stop(stop)) < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid stop state");
        return NULL;
    }

    if (set_acceleration(pair, accel, &use_profile) < 0 ||
        set_deceleration(pair, decel, &use_profile) < 0)
        return NULL;

    if (cmd_start_speed_for_degrees_pair(pair->id, degrees,
                                         speed0, speed1, power,
                                         (uint8_t)parsed_stop,
                                         use_profile) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_run_to_position(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    static char *kwlist[] = {
        "position0", "position1", "speed", "max_power",
        "acceleration", "deceleration", "stop",
        NULL
    };
    int32_t position0, position1;
    int32_t speed;
    uint32_t power = 100;
    uint32_t accel = pair->default_acceleration;
    uint32_t decel = pair->default_deceleration;
    uint32_t stop = MOTOR_STOP_USE_DEFAULT;
    uint8_t use_profile = 0;
    int parsed_stop;

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "iii|IIII:run_to_position", kwlist,
                                     &position0, &position1, &speed,
                                     &power, &stop,
                                     &accel, &decel))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    speed = CLIP(speed, SPEED_MIN, SPEED_MAX);
    power = CLIP(power, POWER_MIN, POWER_MAX);
    accel = CLIP(accel, ACCEL_MIN, ACCEL_MAX);
    decel = CLIP(decel, DECEL_MIN, DECEL_MAX);
    if ((parsed_stop = parse_stop(stop)) < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid stop state");
        return NULL;
    }

    if (set_acceleration(pair, accel, &use_profile) < 0 ||
        set_deceleration(pair, decel, &use_profile) < 0)
        return NULL;

    if (cmd_goto_abs_position_pair(pair->id,
                                   position0, position1,
                                   speed, power,
                                   (uint8_t)parsed_stop,
                                   use_profile) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyMethodDef MotorPair_methods[] = {
    {
        "primary", MotorPair_primary, METH_VARARGS,
        "Returns the primary port"
    },
    {
        "secondary", MotorPair_secondary, METH_VARARGS,
        "Returns the secondary port"
    },
    {
        "id", MotorPair_id, METH_VARARGS,
        "Returns the ID of the port pair"
    },
    {
        "callback", MotorPair_callback, METH_VARARGS,
        "Sets or returns the current callback function"
    },
    {
        "unpair", MotorPair_unpair, METH_VARARGS,
        "Unpair the motors.  The object is no longer valid"
    },
    {
        "pid", MotorPair_pid, METH_VARARGS,
        "Set the default P, I, D values"
    },
    {
        "float", MotorPair_float, METH_VARARGS,
        "Force the motor drivers to floating state"
    },
    {
        "brake", MotorPair_brake, METH_VARARGS,
        "Force the motor drivers to break state"
    },
    {
        "hold", MotorPair_hold, METH_VARARGS,
        "Force the motor drivers to hold position"
    },
    {
        "pwm", MotorPair_pwm, METH_VARARGS,
        "Set the PWM level for the motors"
    },
    {
        "preset", MotorPair_preset, METH_VARARGS,
        "Set the 'zero' positions for the motor pair"
    },
    {
        "run_at_speed", (PyCFunction)MotorPair_run_at_speed,
        METH_VARARGS | METH_KEYWORDS,
        "Run the motor pair at specified speeds"
    },
    {
        "run_for_time", (PyCFunction)MotorPair_run_for_time,
        METH_VARARGS | METH_KEYWORDS,
        "Run the motor pair for a given length of time"
    },
    {
        "run_for_degrees", (PyCFunction)MotorPair_run_for_degrees,
        METH_VARARGS | METH_KEYWORDS,
        "Run the motor pair for the given angle"
    },
    {
        "run_to_position", (PyCFunction)MotorPair_run_to_position,
        METH_VARARGS | METH_KEYWORDS,
        "Run the motor pair to the given positions"
    },
    { NULL, NULL, 0, NULL }
};


static PyTypeObject MotorPairType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "MotorPair",
    .tp_doc = "A pair of attached motors",
    .tp_basicsize = sizeof(MotorPairObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = MotorPair_new,
    .tp_init = (initproc)MotorPair_init,
    .tp_dealloc = (destructor)MotorPair_dealloc,
    .tp_traverse = (traverseproc)MotorPair_traverse,
    .tp_clear = (inquiry)MotorPair_clear,
    .tp_methods = MotorPair_methods,
    .tp_repr = MotorPair_repr
};


int pair_modinit(void)
{
    if (PyType_Ready(&MotorPairType) < 0)
        return -1;
    Py_INCREF(&MotorPairType);
    return 0;
}


void pair_demodinit(void)
{
    Py_DECREF(&MotorPairType);
}


PyObject *pair_get_pair(PyObject *primary, PyObject *secondary)
{
    int i;
    uint8_t primary_id = port_get_id(primary);
    uint8_t secondary_id = port_get_id(secondary);

    /* First attempt to find an existing pair with these ports */
    for (i = 0; i < PAIR_COUNT; i++)
    {
        if (pairs[i] != NULL &&
            ((pairs[i]->primary_id == primary_id &&
              pairs[i]->secondary_id == secondary_id) ||
             (pairs[i]->primary_id == secondary_id &&
              pairs[i]->secondary_id == primary_id)))
        {
            return (PyObject *)pairs[i];
        }
    }
    return PyObject_CallFunctionObjArgs((PyObject *)&MotorPairType,
                                        primary, secondary, NULL);
}


int pair_is_ready(PyObject *self)
{
    MotorPairObject *pair = (MotorPairObject *)self;

    return (pair->id != INVALID_ID);
}


int pair_attach_port(uint8_t id,
                     uint8_t primary_id,
                     uint8_t secondary_id,
                     uint16_t device_type)
{
    int i;

    for (i = 0; i < PAIR_COUNT; i++)
    {
        if (pairs[i] != NULL &&
            pairs[i]->primary_id == primary_id &&
            pairs[i]->secondary_id == secondary_id)
        {
            pairs[i]->id = id;
            pairs[i]->device_type = device_type;
            return 0;
        }
    }

    /* Somehow this is not a pair we know about */
    return -1;
}


static MotorPairObject *find_pair(uint8_t id)
{
    int i;

    for (i = 0; i < PAIR_COUNT; i++)
        if (pairs[i] != NULL && pairs[i]->id == id)
            return pairs[i];

    return NULL;
}


int pair_detach_port(uint8_t id)
{
    MotorPairObject *pair = find_pair(id);

    if (pair != NULL)
        pair->id = INVALID_ID;

    /* Most likely any error is caused by timing out an unpair command */
    return 0;
}


int pair_detach_subport(uint8_t id)
{
    int i;

    for (i = 0; i < PAIR_COUNT; i++)
    {
        if (pairs[i] != NULL &&
            (pairs[i]->primary_id == id ||
             pairs[i]->secondary_id == id))
        {
            /* Ignore errors for now */
            cmd_disconnect_virtual_port(pairs[i]->id, 1);
        }
    }
    return 0;
}


int pair_unpair(PyObject *self)
{
    int i;
    MotorPairObject *pair = (MotorPairObject *)self;
    clock_t start;

    for (i = 0; i < PAIR_COUNT; i++)
    {
        if (pairs[i] == pair)
        {
            break;
        }
    }
    if (i == PAIR_COUNT)
        return -1;

    if (pair->id != INVALID_ID)
        if (cmd_disconnect_virtual_port(pair->id, 0) < 0)
            return -1;

    /* Wait for ID to become invalid */
    start = clock();
    while (pair->id != INVALID_ID)
    {
        if (clock() - start > CLOCKS_PER_SEC)
        {
            /* Timeout */
            pairs[i] = NULL;
            return 1;
        }
    }

    pairs[i] = NULL;
    Py_DECREF(pair);
    return 0;
}


int pair_feedback_status(uint8_t port_id, uint8_t status)
{
    MotorPairObject *pair = find_pair(port_id);

    if (pair == NULL)
        return -1;

    /* Turns out we don't track busy here */
    return 0;
}
