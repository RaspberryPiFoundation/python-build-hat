/* pair.c
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
 * Python class for handling a pair of motors
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>

#include "pair.h"
#include "port.h"
#include "cmd.h"
#include "motor-settings.h"
#include "callback.h"
#include "motor.h"

/**

.. py:class:: MotorPair

    The ``MotorPair`` object provides the API that is available to
    paired motors operated as one.

    The methods of this class will generally return ``False`` if the
    motors are no longer paired, either explicitly or because one of
    the motors has been unplugged.

    .. note::

        This class is not directly available to the user.  Instances
        are created by the :py:meth:`Motor.pair()` method.

    .. py:method:: primary()

        Returns the :py:class:`Motor` object representing the motor
        that created this pair.

    .. py:method:: secondary()

        Returns the :py:class:`Motor` object representing the motor
        that was paired with the :py:meth:`.primary()` to create this
        pair.

    .. py:method:: id()

        Returns the integer ID assigned to the port pair.

    .. py:method:: callback([fn])

        Gets or sets the function to be called when a motor command on
        the pair completes.

        :param fn: The function to call.  This is a positional
            parameter only.
        :type fn: Callable or None
        :raises TypeError: if ``fn`` is present, not ``None`` and not
            callable.
        :return: the current callback function if ``fn`` is omitted,
            otherwise ``None``.
        :rtype: Callable or None

        ``fn``, if present, should be a function taking one parameter
        that indicates whether the motor command completed
        successfully.  The possible values are:

        * :py:const:`Motor.EVENT_COMPLETED` : indicates that the
          command completed successfully.
        * :py:const:`Motor.EVENT_INTERRUPTED` : indicates that the
          command was interrupted before it could complete.
        * :py:const:`Motor.EVENT_STALL` : indicates that the motor
          stalled, preventing the command from completing.

        The callback function is called from a background context.  It
        is sometimes best to set a flag variable and deal with the
        event from the foreground.

        ``fn`` is a position-only parameter.

        .. note::

            The callback is not as useful as one might hope, since it
            does not indicate which motor command is being reported.
            Methods of the :py:class:`MotorPair` class may invoke more
            than one motor command, depending on their exact
            parameters.

    .. py:method:: unpair()

        Removes the pairing between the motors in this pair.  After
        calling this method, this :py:class:`MotorPair` object will be
        invalid.

        Returns ``True`` if the unpair was successful, or ``False`` if
        the attempt timed out.

    .. py:method:: pid([p, i, d])

        With no parameters, returns a tuple with the current used
        P(roportional), I(ntegral) and D(erivative) motor controller
        values if the values have been set using this method or
        :py:meth:`Motor.default()`.  If the values have not been set,
        a tuple of zeroes is returned and is invalid.  Otherwise all
        three parameters must be given, and the default P, I and D
        values are set from them.

       .. note::

           At the moment it is not possible to readout the default PID
           values used in the low-level drivers. To do this it is
           required to implement additional sub-commands in the LPF2
           protocol.

    .. py:method:: float()

        Force the motor driver to floating state.  Equivalent to
        ``MotorPair.pwm(0)``.

    .. py:method:: brake()

        Force the motor driver to brake state.  Equivalent to
        ``MotorPair.pwm(127)``

    .. py:method:: hold([power])

        :param int power: Percentage of maximum power to use (0 - 100).

        Force the motor drivers to hold position.  If ``power`` is
        specified and is negative or greater than 100, it is clipped
        back to the valid range rather than raising an exception.

        ``power`` is a position-only parameter.

    .. py:method:: pwm(value)

        As :py:meth:`Port.pwm()`

    .. py:method:: preset(position0, position1)

        "Presets" the motors' relative zero positions.

        :param int position0: the position the primary motor should
            consider itself to be at relative to the "zero" point.

        :param int position1: the position the secondary motor should
            consider itself to be at relative to the "zero" point.

        ``position0`` and ``position1`` are position-only parameters.

    .. py:method:: run_at_speed(speed0, speed1[, max_power, \
                                acceleration, deceleration])

        Sets the motors running at the given speeds.

        :param int speed0: the percentage of full speed at which to
            run the primary motor, from -100 to +100.  Out of range
            values are silently clipped to the correct range.
            Negative values run the motor in reverse.
        :param int speed1: the percentage of full speed at which to
            run the secondary motor, from -100 to +100.  Out of range
            values are silently clipped to the correct range.
            Negative values run the motor in reverse.
        :param int max_power: the maximum power of the motors to use
            when regulating speed, as a percentage of full power (0 -
            100).  Out of range values are silently clipped to the
            correct range.  If omitted, 100% power is assumed.
        :param int acceleration: the time in milliseconds to achieve
            the given speed (0 - 10000).  Out of range values are
            silently clipped to the correct range.  If omitted, the
            default is 100ms.
        :param int deceleration: the time in milliseconds to stop from
            full speed (0 - 10000).  Out of range values are silently
            clipped to the correct range.  If omitted, the default is
            150ms.

    .. py:method:: run_for_time(msec, speed0, speed1[, max_power, \
                                stop, acceleration, deceleration, \
                                blocking])

        Runs the motors for the given period.

        :param int msec: the time in milliseconds for which to run the
            motors.
        :param int speed0: the percentage of full speed at which to
            run the primary motor, from -100 to +100.  Out of range
            values are silently clipped to the correct range.
            Negative values run the motor in reverse.
        :param int speed1: the percentage of full speed at which to
            run the secondary motor, from -100 to +100.  Out of range
            values are silently clipped to the correct range.
            Negative values run the motor in reverse.
        :param int max_power: the maximum power of the motors to use
            when regulating speed, as a percentage of full power (0 -
            100).  Out of range values are silently clipped to the
            correct range.  If omitted, 100% power is assumed.
        :param int stop: the stop state of the motor.  Must be one of
            ``0`` (floating), ``1`` (brake), ``2`` (hold position) or
            ``3`` (current default setting).  If omitted, ``1``
            (break) is used by default.
        :param int acceleration: the time in milliseconds to achieve
            the given speed (0 - 10000).  Out of range values are
            silently clipped to the correct range.  If omitted, the
            default is 100ms.
        :param int deceleration: the time in milliseconds to stop from
            full speed (0 - 10000).  Out of range values are silently
            clipped to the correct range.  If omitted, the default is
            150ms.
        :param bool blocking: waits for the motors to stop if true
            (the default), otherwise returns early to allow other
            commands to be executed.

    .. py:method:: run_for_degrees(degrees, speed0, speed1[, max_power, \
                                stop, acceleration, deceleration, \
                                blocking])

        Runs the motor through the given angle.

        :param int degrees: the angle in degrees to move the motors
            through from their current positions.
        :param int speed0: the percentage of full speed at which to
            run the primary motor, from -100 to +100.  Out of range
            values are silently clipped to the correct range.
            Negative values run the motor in reverse.
        :param int speed1: the percentage of full speed at which to
            run the secondary motor, from -100 to +100.  Out of range
            values are silently clipped to the correct range.
            Negative values run the motor in reverse.
        :param int max_power: the maximum power of the motors to use
            when regulating speed, as a percentage of full power (0 -
            100).  Out of range values are silently clipped to the
            correct range.  If omitted, 100% power is assumed.
        :param int stop: the stop state of the motor.  Must be one of
            ``0`` (floating), ``1`` (brake), ``2`` (hold position) or
            ``3`` (current default setting).  If omitted, ``1``
            (break) is used by default.
        :param int acceleration: the time in milliseconds to achieve
            the given speed (0 - 10000).  Out of range values are
            silently clipped to the correct range.  If omitted, the
            default is 100ms.
        :param int deceleration: the time in milliseconds to stop from
            full speed (0 - 10000).  Out of range values are silently
            clipped to the correct range.  If omitted, the default is
            150ms.
        :param bool blocking: waits for the motor to stop if true
            (the default), otherwise returns early to allow other
            commands to be executed.

    .. py:method:: run_to_position(position0, position1, speed[, max_power, \
                                   stop, acceleration, deceleration, \
                                   blocking])

        Runs the motors to the given absolute positions.

        :param int position0: the angle from the "zero position" preset
            to move the primary motor to.
        :param int position1: the angle from the "zero position" preset
            to move the secondary motor to.
        :param int speed: the percentage of full speed at which to run
            the motors, from -100 to +100.  Out of range values are
            silently clipped to the correct range.  Negative values
            run the motor in reverse.
        :param int max_power: the maximum power of the motors to use
            when regulating speed, as a percentage of full power (0 -
            100).  Out of range values are silently clipped to the
            correct range.  If omitted, 100% power is assumed.
        :param int stop: the stop state of the motor.  Must be one of
            ``0`` (floating), ``1`` (brake), ``2`` (hold position) or
            ``3`` (current default setting).  If omitted, ``1``
            (break) is used by default.
        :param int acceleration: the time in milliseconds to achieve
            the given speed (0 - 10000).  Out of range values are
            silently clipped to the correct range.  If omitted, the
            default is 100ms.
        :param int deceleration: the time in milliseconds to stop from
            full speed (0 - 10000).  Out of range values are silently
            clipped to the correct range.  If omitted, the default is
            150ms.
        :param bool blocking: waits for the motor to stop if true
            (the default), otherwise returns early to allow other
            commands to be executed.

        .. warning::

            If you have not kept the motors' positions in sync, the
            resulting movement can be somewhat surprising.  Think
            carefully before using this method.

 */


/* The actual motor pair type */
typedef struct
{
    PyObject_HEAD
    PyObject *primary;   /* The Port object, not the Motor! */
    PyObject *secondary; /* Also the Port object */
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

    if (motor_ensure_mode_info(primary) < 0 ||
        motor_ensure_mode_info(secondary) < 0)
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

    return port_get_motor(pair->primary);
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

    return port_get_motor(pair->secondary);
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
    long from_preset0, from_preset1;

    if (!PyArg_ParseTuple(args, "ii", &position0, &position1))
        return NULL;

    /* If the object is invalid, return False */
    if (pair->id == INVALID_ID)
        Py_RETURN_FALSE;

    if (motor_get_position(pair->primary, &from_preset0) < 0 ||
        motor_get_position(pair->secondary, &from_preset1) < 0)
        return NULL;

    if (cmd_preset_encoder_pair(pair->id, position0, position1) < 0)
        return NULL;
    motor_update_preset(pair->primary, position0 - from_preset0);
    motor_update_preset(pair->secondary, position1 - from_preset1);

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
        "acceleration", "deceleration", "blocking",
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
    int blocking = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "Iii|IIIIp:run_for_time", kwlist,
                                     &time, &speed0, &speed1,
                                     &power, &stop,
                                     &accel, &decel, &blocking))
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
                                      (uint8_t)parsed_stop, use_profile,
                                      blocking) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_run_for_degrees(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    static char *kwlist[] = {
        "degrees", "speed0", "speed1", "max_power", "stop",
        "acceleration", "deceleration", "blocking",
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
    int blocking = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "iii|IIIIp:run_for_degrees", kwlist,
                                     &degrees, &speed0, &speed1,
                                     &power, &stop,
                                     &accel, &decel, &blocking))
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
                                         use_profile, blocking) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
MotorPair_run_to_position(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorPairObject *pair = (MotorPairObject *)self;
    static char *kwlist[] = {
        "position0", "position1", "speed", "max_power", "stop",
        "acceleration", "deceleration", "blocking",
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
    int blocking = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "iii|IIIIp:run_to_position", kwlist,
                                     &position0, &position1, &speed,
                                     &power, &stop,
                                     &accel, &decel,
                                     &blocking))
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

    position0 -= motor_get_position_offset(pair->primary);
    position1 -= motor_get_position_offset(pair->secondary);

    if (set_acceleration(pair, accel, &use_profile) < 0 ||
        set_deceleration(pair, decel, &use_profile) < 0)
        return NULL;

    if (cmd_goto_abs_position_pair(pair->id,
                                   position0, position1,
                                   speed, power,
                                   (uint8_t)parsed_stop,
                                   use_profile, blocking) < 0)
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
            pairs[i]->primary_id == primary_id &&
            pairs[i]->secondary_id == secondary_id)
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
            cmd_disconnect_virtual_port(pairs[i]->id);
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
        if (cmd_disconnect_virtual_port(pair->id) < 0)
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


/* See port.c:port_feedback_status() for a discussion of the status
 * field bits.  Called from the background rx context.
 */
int pair_feedback_status(uint8_t port_id, uint8_t status)
{
    MotorPairObject *pair = find_pair(port_id);

    if (pair == NULL)
        return -1;

    if ((status & 0x02) != 0)
        callback_queue(CALLBACK_PAIR, port_id, CALLBACK_COMPLETE, NULL);
    if ((status & 0x04) != 0)
        callback_queue(CALLBACK_PAIR, port_id,
                       ((status & 0x20) != 0) ? CALLBACK_STALLED :
                       CALLBACK_INTERRUPTED,
                       NULL);

    /* Turns out we don't track busy here */

    return 0;
}


/* Called from the callback thread */
int pair_handle_callback(uint8_t port_id, uint8_t event)
{
    MotorPairObject *pair = find_pair(port_id);
    PyGILState_STATE gstate;
    PyObject *arg_list;
    int rv;

    if (pair == NULL)
        return -1;

    gstate = PyGILState_Ensure();

    if (pair->callback_fn == Py_None)
    {
        PyGILState_Release(gstate);
        return 0;
    }

    arg_list = Py_BuildValue("(i)", event);
    rv = (PyObject_CallObject(pair->callback_fn, arg_list) != NULL) ? 0 : -1;
    Py_XDECREF(arg_list);
    PyGILState_Release(gstate);

    return rv;
}
