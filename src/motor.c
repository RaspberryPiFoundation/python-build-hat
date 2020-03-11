/* motor.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Python class for handling a port's "motor" attribute
 * and attached motors.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>

#include "motor.h"
#include "port.h"
#include "device.h"


/* The actual Motor type */
typedef struct
{
    PyObject_HEAD
    PyObject *port;
    PyObject *device;
    uint32_t default_speed;
    uint32_t default_max_power;
    uint32_t default_acceleration;
    uint32_t default_deceleration;
    int default_stall;
    uint32_t default_stop;
    uint32_t default_position_pid[3];
    int want_default_acceleration_set;
    int want_default_deceleration_set;
    int want_default_stall_set;
    PyObject *callback;
} MotorObject;

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

/* Limits for various motor settings */
#define SPEED_MIN -100
#define SPEED_MAX 100
#define POWER_MIN 0
#define POWER_MAX 100
#define ACCEL_MIN 0
#define ACCEL_MAX 10000
#define DECEL_MIN 0
#define DECEL_MAX 10000

#define CLIP(value,min,max) (((value) > (max)) ? (max) :                \
                             (((value) < (min)) ? (min) : (value)))


/* Utility functions for dealing with common parameters */
static int parse_stop(MotorObject *motor, uint32_t stop)
{
    switch (stop)
    {
        case MOTOR_STOP_FLOAT:
            return STOP_FLOAT;

        case MOTOR_STOP_BRAKE:
            return STOP_BRAKE;

        case MOTOR_STOP_HOLD:
            return STOP_HOLD;

        case MOTOR_STOP_USE_DEFAULT:
            return motor->default_stop;

        default:
            break;
    }
    return -1;
}


static int set_acceleration(MotorObject *motor,
                            uint32_t accel,
                            uint8_t *p_use_profile)
{
    if (accel != motor->default_acceleration)
    {
        motor->want_default_acceleration_set = 1;
        *p_use_profile |= USE_PROFILE_ACCELERATE;
        return cmd_set_acceleration(port_get_id(motor->port), accel);
    }
    if (motor->want_default_acceleration_set)
    {
        if (cmd_set_acceleration(port_get_id(motor->port),
                                 motor->default_acceleration) < 0)
            return -1;
        motor->want_default_acceleration_set = 0;
    }
    /* Else the right acceleration value has already been set */
    return 0;
}


static int set_deceleration(MotorObject *motor,
                            uint32_t decel,
                            uint8_t *p_use_profile)
{
    if (decel != motor->default_deceleration)
    {
        motor->want_default_deceleration_set = 1;
        *p_use_profile |= USE_PROFILE_DECELERATE;
        return cmd_set_deceleration(port_get_id(motor->port), decel);
    }
    if (motor->want_default_deceleration_set)
    {
        if (cmd_set_deceleration(port_get_id(motor->port),
                                 motor->default_deceleration) < 0)
            return -1;
        motor->want_default_deceleration_set = 0;
    }
    /* Else the right deceleration value has already been set */
    return 0;
}


static int set_stall(MotorObject *motor, int stall)
{
    stall = stall ? 1 : 0;
    if (stall != motor->default_stall)
    {
        motor->want_default_stall_set = 1;
        return cmd_set_stall(port_get_id(motor->port), stall);
    }
    if (motor->want_default_stall_set)
    {
        if (cmd_set_stall(port_get_id(motor->port),
                          motor->default_stall) < 0)
            return -1;
        motor->want_default_stall_set = 0;
    }
    /* Else the right stall detection is already set */
    return 0;
}



static int
Motor_traverse(MotorObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->port);
    Py_VISIT(self->device);
    return 0;
}


static int
Motor_clear(MotorObject *self)
{
    Py_CLEAR(self->port);
    Py_CLEAR(self->device);
    return 0;
}


static void
Motor_dealloc(MotorObject *self)
{
    PyObject_GC_UnTrack(self);
    Motor_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *
Motor_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MotorObject *self = (MotorObject *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->port = Py_None;
        Py_INCREF(Py_None);
        self->device = Py_None;
        Py_INCREF(Py_None);
        self->callback = Py_None;
        Py_INCREF(Py_None);
    }
    return (PyObject *)self;
}


static int
Motor_init(MotorObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "port", "device", NULL };
    PyObject *port = NULL;
    PyObject *device = NULL;
    PyObject *tmp;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist, &port, &device))
        return -1;

    tmp = self->port;
    Py_INCREF(port);
    self->port = port;
    Py_XDECREF(tmp);

    tmp = self->device;
    Py_INCREF(device);
    self->device = device;
    Py_XDECREF(tmp);

    self->default_speed = 0;
    self->default_max_power = 0;
    self->default_acceleration = DEFAULT_ACCELERATION;
    self->default_deceleration = DEFAULT_DECELERATION;
    self->default_stall = 1;
    self->default_stop = STOP_BRAKE;
    self->default_position_pid[0] = 0;
    self->default_position_pid[0] = 1;
    self->default_position_pid[0] = 2;

    self->want_default_acceleration_set = 1;
    self->want_default_deceleration_set = 1;

    return 0;
}


static PyObject *
Motor_repr(PyObject *self)
{
    MotorObject *motor = (MotorObject *)self;
    int port_id = port_get_id(motor->port);

    return PyUnicode_FromFormat("Motor(%c)", 'A' + port_id);
}


static PyObject *
Motor_get(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *get_fn = PyObject_GetAttrString(motor->device, "get");
    PyObject *result;

    if (get_fn == NULL)
        return NULL;
    result = PyObject_CallObject(get_fn, args);
    Py_DECREF(get_fn);
    return result;
}


static PyObject *
Motor_mode(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *mode_fn = PyObject_GetAttrString(motor->device, "mode");
    PyObject *result;

    if (mode_fn == NULL)
        return NULL;
    result = PyObject_CallObject(mode_fn, args);
    Py_DECREF(mode_fn);
    return result;
}


static PyObject *
Motor_pwm(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *pwm_fn = PyObject_GetAttrString(motor->port, "pwm");
    PyObject *result;

    if (pwm_fn == NULL)
        return NULL;
    result = PyObject_CallObject(pwm_fn, args);
    Py_DECREF(pwm_fn);
    return result;
}


static PyObject *
Motor_float(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;

    /* float() is equivalent to pwm(0) */

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    return PyObject_CallMethod(motor->port, "pwm", "i", 0);
}


static PyObject *
Motor_brake(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;

    /* brake() is equivalent to pwm(127) */

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    return PyObject_CallMethod(motor->port, "pwm", "i", 127);
}


static PyObject *
Motor_hold(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    int max_power = 100;
    uint8_t use_profile = 0;

    if (!PyArg_ParseTuple(args, "|i", &max_power))
        return NULL;

    if (max_power > 100 || max_power < 0)
    {
        PyErr_Format(PyExc_ValueError,
                     "Max power %d out of range",
                     max_power);
        return NULL;
    }
    if (motor->default_acceleration != 0)
        use_profile |= USE_PROFILE_ACCELERATE;
    if (motor->default_deceleration != 0)
        use_profile |= USE_PROFILE_DECELERATE;

    if (cmd_start_speed(port_get_id(motor->port), 0,
                        max_power, use_profile) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
Motor_busy(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    int type;

    if (!PyArg_ParseTuple(args, "i", &type))
        return NULL;

    return device_is_busy(motor->device, type);
}


static PyObject *
Motor_default(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorObject *motor = (MotorObject *)self;
    static char *kwlist[] = {
        "speed", "max_power", "acceleration", "deceleration",
        "stop", "pid", "stall", "callback",
        NULL
    };
    uint32_t speed = motor->default_speed; /* Use the right defaults */
    uint32_t power = motor->default_max_power;
    uint32_t acceleration = motor->default_acceleration;
    uint32_t deceleration = motor->default_deceleration;
    uint32_t stop = MOTOR_STOP_USE_DEFAULT;
    int parsed_stop;
    uint32_t pid[3];
    int stall = motor->default_stall;
    PyObject *callback = NULL;

    /* If we have no parameters, return a dictionary of defaults.
     * To determine that, we need to inspect the tuple `args` and
     * the dictionary `kwds` to see if they contain anything.
     */
    if (PyTuple_Size(args) != 0)
    {
        PyErr_SetString(PyExc_TypeError,
                        "Function takes at most 0 positional arguments");
        return NULL;
    }
    if (kwds == NULL || PyDict_Size(kwds) == 0)
    {
        /* No args, return the dictionary */
        return Py_BuildValue("{sI sI sI sI sN sI sO s(III)}",
                             "speed", motor->default_speed,
                             "max_power", motor->default_max_power,
                             "acceleration", motor->default_acceleration,
                             "deceleration", motor->default_deceleration,
                             "stall", PyBool_FromLong(motor->default_stall),
                             "stop", motor->default_stop,
                             "callback", motor->callback,
                             "pid",
                             motor->default_position_pid[0],
                             motor->default_position_pid[1],
                             motor->default_position_pid[2]);
    }

    memcpy(pid, motor->default_position_pid, sizeof(pid));
    if (PyArg_ParseTupleAndKeywords(args, kwds,
                                    "|$IIIII(III)pO:default", kwlist,
                                    &speed, &power,
                                    &acceleration,
                                    &deceleration,
                                    &stop,
                                    &pid[0], &pid[1], &pid[2],
                                    &stall,
                                    &callback) == 0)
        return NULL;

    motor->default_speed = speed;
    motor->default_max_power = power;

    if (acceleration != motor->default_acceleration)
    {
        motor->default_acceleration = acceleration;
        if (cmd_set_acceleration(port_get_id(motor->port), acceleration) < 0)
        {
            motor->want_default_acceleration_set = 1;
            return NULL;
        }
        motor->want_default_acceleration_set = 0;
    }

    if (deceleration != motor->default_deceleration)
    {
        motor->default_deceleration = deceleration;
        if (cmd_set_deceleration(port_get_id(motor->port), deceleration) < 0)
        {
            motor->want_default_deceleration_set = 1;
            return NULL;
        }
        motor->want_default_deceleration_set = 0;
    }

    if ((parsed_stop = parse_stop(motor, stop)) < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid stop mode setting\n");
        return NULL;
    }
    motor->default_stop = (uint32_t)parsed_stop;

    if (pid[0] != motor->default_position_pid[0] ||
        pid[1] != motor->default_position_pid[1] ||
        pid[2] != motor->default_position_pid[2])
    {
        if (cmd_set_pid(port_get_id(motor->port), pid) < 0)
            return NULL;
        memcpy(motor->default_position_pid, pid, sizeof(pid));
    }

    if (stall != motor->default_stall)
    {
        motor->default_stall = stall;
        if (cmd_set_stall(port_get_id(motor->port), stall) < 0)
        {
            motor->want_default_stall_set = 1;
            return NULL;
        }
        motor->want_default_stall_set = 0;
    }

    if (callback != NULL)
    {
        PyObject *cb = motor->callback;

        Py_INCREF(callback);
        motor->callback = callback;
        Py_DECREF(cb);
    }

    Py_RETURN_NONE;
}


static PyObject *
Motor_run_at_speed(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorObject *motor = (MotorObject *)self;
    static char *kwlist[] = {
        "speed", "max_power", "acceleration", "deceleration", "stall",
        NULL
    };
    int32_t speed;
    uint32_t power = motor->default_max_power;
    uint32_t accel = motor->default_acceleration;
    uint32_t decel = motor->default_deceleration;
    int stall = motor->default_stall;
    uint8_t use_profile = 0;

    if (PyArg_ParseTupleAndKeywords(args, kwds,
                                    "i|IIIp:run_at_speed", kwlist,
                                    &speed, &power,
                                    &accel, &decel, &stall) == 0)
        return NULL;

    speed = CLIP(speed, SPEED_MIN, SPEED_MAX);
    power = CLIP(power, POWER_MIN, POWER_MAX);
    accel = CLIP(accel, ACCEL_MIN, ACCEL_MAX);
    decel = CLIP(decel, DECEL_MIN, DECEL_MAX);

    if (set_acceleration(motor, accel, &use_profile) < 0 ||
        set_deceleration(motor, decel, &use_profile) < 0 ||
        set_stall(motor, stall) < 0)
        return NULL;

    if (cmd_start_speed(port_get_id(motor->port),
                        speed, power, use_profile) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
Motor_run_for_degrees(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorObject *motor = (MotorObject *)self;
    static char *kwlist[] = {
        "degrees", "speed", "max_power", "stop",
        "acceleration", "deceleration", "stall",
        NULL
    };
    int32_t degrees;
    int32_t speed;
    uint32_t power = motor->default_max_power;
    uint32_t accel = motor->default_acceleration;
    uint32_t decel = motor->default_deceleration;
    int stall = motor->default_stall;
    uint32_t stop = MOTOR_STOP_USE_DEFAULT;
    uint8_t use_profile = 0;
    int parsed_stop;

    if (PyArg_ParseTupleAndKeywords(args, kwds,
                                    "ii|IIIIp:run_for_degrees", kwlist,
                                    &degrees, &speed, &power, &stop,
                                    &accel, &decel, &stall) == 0)
        return NULL;

    speed = CLIP(speed, SPEED_MIN, SPEED_MAX);
    power = CLIP(power, POWER_MIN, POWER_MAX);
    accel = CLIP(accel, ACCEL_MIN, ACCEL_MAX);
    decel = CLIP(decel, DECEL_MIN, DECEL_MAX);
    if ((parsed_stop = parse_stop(motor, stop)) < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid stop state");
        return NULL;
    }

    if (set_acceleration(motor, accel, &use_profile) < 0 ||
        set_deceleration(motor, decel, &use_profile) < 0 ||
        set_stall(motor, stall) < 0)
        return NULL;

    if (cmd_start_speed_for_degrees(port_get_id(motor->port),
                                    degrees, speed, power,
                                    (uint8_t)parsed_stop, use_profile) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
Motor_run_to_position(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorObject *motor = (MotorObject *)self;
    static char *kwlist[] = {
        "position", "speed", "max_power", "stop",
        "acceleration", "deceleration", "stall",
        NULL
    };
    int32_t position;
    int32_t speed;
    uint32_t power = motor->default_max_power;
    uint32_t accel = motor->default_acceleration;
    uint32_t decel = motor->default_deceleration;
    int stall = motor->default_stall;
    uint32_t stop = MOTOR_STOP_USE_DEFAULT;
    uint8_t use_profile = 0;
    int parsed_stop;

    if (PyArg_ParseTupleAndKeywords(args, kwds,
                                    "ii|IIIIp:run_to_position", kwlist,
                                    &position, &speed, &power, &stop,
                                    &accel, &decel, &stall) == 0)
        return NULL;

    speed = CLIP(speed, SPEED_MIN, SPEED_MAX);
    power = CLIP(power, POWER_MIN, POWER_MAX);
    accel = CLIP(accel, ACCEL_MIN, ACCEL_MAX);
    decel = CLIP(decel, DECEL_MIN, DECEL_MAX);
    if ((parsed_stop = parse_stop(motor, stop)) < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid stop state");
        return NULL;
    }

    if (set_acceleration(motor, accel, &use_profile) < 0 ||
        set_deceleration(motor, decel, &use_profile) < 0 ||
        set_stall(motor, stall) < 0)
        return NULL;

    if (cmd_goto_abs_position(port_get_id(motor->port),
                              position, speed, power,
                              (uint8_t)parsed_stop, use_profile) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyMethodDef Motor_methods[] = {
    { "mode", Motor_mode, METH_VARARGS, "Get or set the current mode" },
    { "get", Motor_get, METH_VARARGS, "Get a set of readings from the motor" },
    { "pwm", Motor_pwm, METH_VARARGS, "Set the PWM level for the motor" },
    {
        "float", Motor_float, METH_VARARGS,
        "Force the motor driver to floating state"
    },
    {
        "brake", Motor_brake, METH_VARARGS,
        "Force the motor driver to brake state"
    },
    {
        "hold", Motor_hold, METH_VARARGS,
        "Force the motor driver to hold position"
    },
    { "busy", Motor_busy, METH_VARARGS, "Check if the motor is busy" },
    {
        "default", (PyCFunction)Motor_default,
        METH_VARARGS | METH_KEYWORDS,
        "View or set the default values used in motor functions"
    },
    {
        "run_at_speed", (PyCFunction)Motor_run_at_speed,
        METH_VARARGS | METH_KEYWORDS,
        "Run the motor at the given speed"
    },
    {
        "run_for_degrees", (PyCFunction)Motor_run_for_degrees,
        METH_VARARGS | METH_KEYWORDS,
        "Run the motor for the given angle"
    },
    {
        "run_to_position", (PyCFunction)Motor_run_to_position,
        METH_VARARGS | METH_KEYWORDS,
        "Run the motor to the given position"
    },
    { NULL, NULL, 0, NULL }
};


static PyObject *
Motor_get_constant(MotorObject *motor, void *closure)
{
    return PyLong_FromVoidPtr(closure);
}


static PyGetSetDef Motor_getsetters[] =
{
    {
        "BUSY_MODE",
        (getter)Motor_get_constant,
        NULL,
        "Parameter to Motor.busy() to check mode status",
        (void *)0
    },
    {
        "BUSY_MOTOR",
        (getter)Motor_get_constant,
        NULL,
        "Parameter to Motor.busy() to check motor status",
        (void *)1
    },
    {
        "EVENT_COMPLETED",
        (getter)Motor_get_constant,
        NULL,
        "Callback reason code: event completed normally",
        (void *)0
    },
    {
        "EVENT_INTERRUPTED",
        (getter)Motor_get_constant,
        NULL,
        "Callback reason code: event was interrupted",
        (void *)1
    },
    {
        "EVENT_STALL",
        (getter)Motor_get_constant,
        NULL,
        "Callback reason code: event has stalled",
        (void *)2
    },
    {
        "FORMAT_RAW",
        (getter)Motor_get_constant,
        NULL,
        "Format giving raw values from the device",
        (void *)0
    },
    {
        "FORMAT_PCT",
        (getter)Motor_get_constant,
        NULL,
        "Format giving percentage values from the device",
        (void *)1
    },
    {
        "FORMAT_SI",
        (getter)Motor_get_constant,
        NULL,
        "Format giving SI unit values from the device",
        (void *)2
    },
    {
        "PID_SPEED",
        (getter)Motor_get_constant,
        NULL,
        "???",
        (void *)0
    },
    {
        "PID_POSITION",
        (getter)Motor_get_constant,
        NULL,
        "???",
        (void *)1
    },
    {
        "STOP_FLOAT",
        (getter)Motor_get_constant,
        NULL,
        "Stop mode: float the motor output on stopping",
        (void *)0
    },
    {
        "STOP_BRAKE",
        (getter)Motor_get_constant,
        NULL,
        "Stop mode: brake the motor on stopping",
        (void *)1
    },
    {
        "STOP_HOLD",
        (getter)Motor_get_constant,
        NULL,
        "Stop mode: actively hold position on stopping",
        (void *)2
    },
    { NULL }
};


static PyTypeObject MotorType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Motor",
    .tp_doc = "An attached motor",
    .tp_basicsize = sizeof(MotorObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = Motor_new,
    .tp_init = (initproc)Motor_init,
    .tp_dealloc = (destructor)Motor_dealloc,
    .tp_traverse = (traverseproc)Motor_traverse,
    .tp_clear = (inquiry)Motor_clear,
    .tp_methods = Motor_methods,
    .tp_getset = Motor_getsetters,
    .tp_repr = Motor_repr
};


int motor_modinit(void)
{
    if (PyType_Ready(&MotorType) < 0)
        return -1;
    Py_INCREF(&MotorType);
    return 0;
}


void motor_demodinit(void)
{
    Py_DECREF(&MotorType);
}


PyObject *motor_new_motor(PyObject *port, PyObject *device)
{
    return PyObject_CallFunctionObjArgs((PyObject *)&MotorType,
                                        port, device, NULL);
}
