/* motor.c
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
 * Python class for handling a port's "motor" attribute
 * and attached motors.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>
#include <stdbool.h>

#include "motor.h"
#include "port.h"
#include "device.h"
#include "pair.h"
#include "motor-settings.h"
#include "callback.h"

/**

.. py:class:: Motor

    The ports on the Build HAT are able to autodetect the
    capabilites of the device that is plugged in.  When a motor device
    is detected, then an enhanced set of methods is available.

    The ``Motor`` object provides the API that is available to motors
    when they are plugged into a port.

    .. note::

        This class is not directly available to the user.  It is only
        used by the port objects, and its instances are provided
        already initialised.

    .. py:attribute:: BUSY_MODE

        :type: int
        :value: 0

        The value to pass to the :py:meth:`.busy()` method to check
        the busy status of the mode (i.e. device).

    .. py:attribute:: BUSY_MOTOR

        :type: int
        :value: 1

        The value to pass to the :py:meth:`.busy()` method to check
        the busy status of the motor

    .. py:attribute:: EVENT_COMPLETED

        :type: int
        :value: 0

        The value passed to :py:class:`Motor` and :py:class:`MotorPair`
        callback functions to indicate that the requested action has
        run to completion.

    .. py:attribute:: EVENT_INTERRUPTED

        :type: int
        :value: 1

        The value passed to :py:class:`Motor` and :py:class:`MotorPair`
        callback functions to indicate that the requested action was
        interrupted before it could run to completion.

    .. py:attribute:: EVENT_STALL

        :type: int
        :value: 2

        The value passed to :py:class:`Motor` and :py:class:`MotorPair`
        callback functions to indicate that stall detection was
        enabled and the motor stalled while the requested action was
        in progress.

    .. py:attribute:: FORMAT_RAW

        :type: int
        :value: 0

        As :py:const:`Device.FORMAT_RAW`.

    .. py:attribute:: FORMAT_PCT

        :type: int
        :value: 1

        As :py:const:`Device.FORMAT_PCT`

    .. py:attribute:: FORMAT_SI

        :type: int
        :value: 2

        As :py:const:`Device.FORMAT_SI`

    .. py:attribute:: PID_SPEED

        :type: int
        :value: 0

        Meaning uncertain.

    .. py:attribute:: PID_POSITION

        :type: int
        :value: 1

        Meaning uncertain.

    .. py:attribute:: STOP_FLOAT

        :type: int
        :value: 0

        Value to pass as a ``stop`` parameter to various
        :py:class:`Motor` and :py:class:`MotorPair` "run" methods.
        Floats the motor output on stopping.

    .. py:attribute:: STOP_BRAKE

        :type: int
        :value: 1

        Value to pass as a ``stop`` parameter to various
        :py:class:`Motor` and :py:class:`MotorPair` "run" methods.
        Brakes the motor output on stopping.

    .. py:attribute:: STOP_HOLD

        :type: int
        :value: 2

        Value to pass as a ``stop`` parameter to various
        :py:class:`Motor` and :py:class:`MotorPair` "run" methods.
        Uses the motor to actively hold position on stopping.

    .. py:attribute:: CLOCKWISE

        :type: int
        :value: 0

        Value to pass as a ``direction`` parameter to
        :py:meth:`Motor.run_to_position()` to cause the motor to
        rotate clockwise to achieve the desired position.

    .. py:attribute:: ANTICLOCKWISE

        :type: int
        :value: 0

        Value to pass as a ``direction`` parameter to
        :py:meth:`Motor.run_to_position()` to cause the motor to
        rotate anticlockwise to achieve the desired position.

    .. py:attribute:: SHORTEST

        :type: int
        :value: 0

        Value to pass as a ``direction`` parameter to
        :py:meth:`Motor.run_to_position()` to cause the motor to
        rotate for the shortest distance to achieve the desired
        position.

    .. py:method:: mode([mode[, mode_data]])

        As :py:meth:`Device.mode()`

    .. py:method:: get([format=2])

        As :py:meth:`Device.get()`

    .. py:method:: pwm(value)

        As :py:meth:`Port.pwm()`

    .. py:method:: float()

        Force the motor driver to floating state.  Equivalent to
        ``Motor.pwm(0)``.

    .. py:method:: brake()

        Force the motor driver to brake state.  Equivalent to
        ``Motor.pwm(127)``

    .. py:method:: hold([power])

        :param int power: Percentage of maximum power to use (0 - 100).

        Force the motor driver to hold position.  If ``power`` is
        specified and is negative or greater than 100, it is clipped
        back to the valid range rather than raising an exception.

        ``power`` is a position-only parameter.

    .. py:method:: busy(which)

        Checks if the device or motor are busy processing commands.

        :param int which: Which system's busy state to check.  Must be
            ``0`` (to check basic Device ("mode") access) or ``1`` (to
            check the Motor state machine).  :py:const:`BUSY_MODE` and
            :py:const:`BUSY_MOTOR` provided for convenience.
        :return: True if the relevant system is busy.
        :rtype: bool
        :raises ValueError: if ``which`` is neither zero nor one.

        ``which`` is a position-only parameter.

    .. py:method:: preset(position)

        "Presets" the motor's relative zero position.

        :param int position: the position the motor should consider
            itself to be at relative to the "zero" point.

        ``position`` is a position-only parameter.

    .. py:method:: default(/, speed=<current default_speed>, \
                           max_power=<current max power>, \
                           acceleration=<current default acceleration>, \
                           deceleration=<current default deceleration>, \
                           stall=<current default stall detection>, \
                           stop=<current default stop mode>, \
                           callback=<current callback function>, \
                           pid=<current pid>)

        Reads or sets default values for the motor methods.

        :param int speed: the default speed to use in Motor methods.
            There are currently no methods that allow a default speed
            to be used.
        :param int max_power: the percentage of maximum motor power
            (PWM) to use when not specified in a method (0 - 100).
            The range is not checked in this method call, but will be
            clipped if necessary when used.  The default is 100%.
        :param int acceleration: (0 - 10000) the time in milliseconds
            to reach 100% of motor design speed from 0%, when the
            acceleration time parameter is omitted from a method call.
            The range is not checked in this method call, but will be
            clipped if necessary when used.  The default is 100ms.
        :param int deceleration: (0 - 10000) the time in milliseconds
            to reach 0% of motor design speed from 100%, when the
            deceleration time parameter is omitted from a method call.
            The range is not checked in this method call, but will be
            clipped if necessary when used.  The default is 150ms.
        :param bool stall: whether stall detection is on or off by
            default, unless overridden in a method call.  The default
            is ``True`` (stall detection enabled).
        :param callback: The callback function called when a motor
            command completes.  If ``None``, the callback is
            disabled.  See :py:meth:`Motor.callback()` for details.
            The default is ``None``.  Note that unlike
            :py:meth:`Motor.callback()`, this method does not check
            that the callback is callable on ``None``.  Supplying
            invalid objects as callbacks will cause unpredictable
            problems.
        :type callback: Callable or none
        :param int stop: the state to put the motor in when the motor
            command completes, unless overridden in a method call.
            Must be one of ``0`` (float the output), ``1`` (brake the
            output), ``2`` (actively hold position) or ``3`` (the
            current default, i.e. leave this unchanged).  The
            attributes :py:const:`Motor.STOP_FLOAT`,
            :py:const:`Motor.STOP_BRAKE` and
            :py:const:`Motor.STOP_HOLD` are provided for convenience
            for the first three values.  The default value is ``1``
            (:py:const:`Motor.STOP_BRAKE`)
        :param pid: the default "position PID"
            (Proportional-Integral-Derivative) motor control tuple,
            unless overridden in a method call.  The default value is
            ``(0, 0, 0)``
        :type pid: (int, int, int)
        :return: a dictionary of the default values if no parameters
            are given, otherwise ``None``.
        :rtype: dict or None
        :raises ValueError: if ``stop`` is not a valid value.

    .. py:method:: callback([fn])

        Gets or sets the function to be called when a motor command
        completes.

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
            Methods of the :py:class:`Motor` class may invoke more
            than one motor command, depending on their exact
            parameters.

    .. py:method:: run_at_speed(speed[, max_power, acceleration, \
                                deceleration, stall])

        Sets the motor running at the given speed.

        :param int speed: the percentage of full speed at which to
            run the motor, from -100 to +100.  Out of range values are
            silently clipped to the correct range.  Negative values
            run the motor in reverse.
        :param int max_power: the maximum power of the motor to use
            when regulating speed, as a percentage of full power (0 -
            100).  Out of range values are silently clipped to the
            correct range.
        :param int acceleration: the time in milliseconds to achieve
            the given speed (0 - 10000).  Out of range values are
            silently clipped to the correct range.
        :param int deceleration: the time in milliseconds to stop from
            full speed (0 - 10000).  Out of range values are silently
            clipped to the correct range.
        :param bool stall: enables or disables stall detection.

        If any of the optional parameters are omitted, default values
        are used as specified by :py:meth:`Motor.default()`.

    .. py:method:: run_for_degrees(degrees, speed[, max_power, stop, \
                                   acceleration, deceleration, stall, \
                                   blocking])

        Runs the motor through the given angle.

        :param int degrees: the angle in degrees to move the motor
            through from the current position.
        :param int speed: the percentage of full speed at which to
            run the motor, from -100 to +100.  Out of range values are
            silently clipped to the correct range.  Negative values
            run the motor in reverse.
        :param int max_power: the maximum power of the motor to use
            when regulating speed, as a percentage of full power (0 -
            100).  Out of range values are silently clipped to the
            correct range.
        :param int stop: the stop state of the motor.  Must be one of
            ``0`` (floating), ``1`` (brake), ``2`` (hold position) or
            ``3`` (current default setting).  The attributes
            :py:const:`Motor.STOP_FLOAT`, :py:const:`Motor.STOP_BRAKE`
            and :py:const:`Motor.STOP_HOLD` are provided as symbolic
            constants for the first three of those values.
        :param int acceleration: the time in milliseconds to achieve
            the given speed (0 - 10000).  Out of range values are
            silently clipped to the correct range.
        :param int deceleration: the time in milliseconds to stop from
            full speed (0 - 10000).  Out of range values are silently
            clipped to the correct range.
        :param bool stall: enables or disables stall detection.
        :param bool blocking: waits for the motor to stop if true
            (the default), otherwise returns early to allow other
            commands to be executed.
        :raises ValueError: if ``stop`` is not a valid value.

        If any of the optional parameters are omitted, default values
        are used as specified by :py:meth:`Motor.default()`.

    .. py:method:: run_to_position(position, speed[, max_power, stop, \
                                   acceleration, deceleration, stall, \
                                   direction, blocking])

        Runs the motor to the given absolute position.

        :param int position: the angle from the "zero position" preset
            to move the motor to.
        :param int speed: the percentage of full speed at which to
            run the motor, from -100 to +100.  Out of range values are
            silently clipped to the correct range.  Negative values
            run the motor in reverse.
        :param int max_power: the maximum power of the motor to use
            when regulating speed, as a percentage of full power (0 -
            100).  Out of range values are silently clipped to the
            correct range.
        :param int stop: the stop state of the motor.  Must be one of
            ``0`` (floating), ``1`` (brake), ``2`` (hold position) or
            ``3`` (current default setting).  The attributes
            :py:const:`Motor.STOP_FLOAT`, :py:const:`Motor.STOP_BRAKE`
            and :py:const:`Motor.STOP_HOLD` are provided as symbolic
            constants for the first three of those values.
        :param int acceleration: the time in milliseconds to achieve
            the given speed (0 - 10000).  Out of range values are
            silently clipped to the correct range.
        :param int deceleration: the time in milliseconds to stop from
            full speed (0 - 10000).  Out of range values are silently
            clipped to the correct range.
        :param bool stall: enables or disables stall detection.
        :param int direction: the direction the motor will turn to
            reach its destination.  Must be one of ``0`` (clockwise),
            ``1`` (anticlockwise) or ``2`` (shortest distance, the
            default).  The attributes :py:const:`Motor.CLOCKWISE`,
            :py:const:`Motor.ANTICLOCKWISE` and
            :py:const:`Motor.SHORTEST` are provided as symbolic
            constants for these values.
        :param bool blocking: waits for the motor to stop if true
            (the default), otherwise returns early to allow other
            commands to be executed.
        :raises ValueError: if ``stop`` or ``direction`` is not a
            valid value.

        If any of the optional parameters are omitted, default values
        are used as specified by :py:meth:`Motor.default()`.

    .. py:method:: run_for_time(msec, speed[, max_power, stop, \
                                acceleration, deceleration, stall \
                                blocking])

        Runs the motor for the given period.

        :param int msec: the time in milliseconds for which to run the
            motor.
        :param int speed: the percentage of full speed at which to
            run the motor, from -100 to +100.  Out of range values are
            silently clipped to the correct range.  Negative values
            run the motor in reverse.
        :param int max_power: the maximum power of the motor to use
            when regulating speed, as a percentage of full power (0 -
            100).  Out of range values are silently clipped to the
            correct range.
        :param int stop: the stop state of the motor.  Must be one of
            ``0`` (floating), ``1`` (brake), ``2`` (hold position) or
            ``3`` (current default setting).  The attributes
            :py:const:`Motor.STOP_FLOAT`, :py:const:`Motor.STOP_BRAKE`
            and :py:const:`Motor.STOP_HOLD` are provided as symbolic
            constants for the first three of those values.
        :param int acceleration: the time in milliseconds to achieve
            the given speed (0 - 10000).  Out of range values are
            silently clipped to the correct range.
        :param int deceleration: the time in milliseconds to stop from
            full speed (0 - 10000).  Out of range values are silently
            clipped to the correct range.
        :param bool stall: enables or disables stall detection.
        :param bool blocking: waits for the motor to stop if true
            (the default), otherwise returns early to allow other
            commands to be executed.
        :raises ValueError: if ``stop`` is not a valid value.

        If any of the optional parameters are omitted, default values
        are used as specified by :py:meth:`Motor.default()`.

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

    .. py:method:: pair(motor)

        Pairs two motors together so they are controlled through a
        single object interface.

        :param motor: the motor to pair, which cannot be the motor
            this method is called on.
        :type motor: :py:class:`Motor`
        :return: a :py:class:`MotorPair` if the motors are
            successfully paired, or ``False`` if the pairing attempted
            timed out.
        :raises TypeError: if ``motor`` is not a :py:class:`Motor`.

        .. note::

            The module regards ``A.pair(B)`` and ``B.pair(A)`` as
            different motor pairs, and will accept multiple pairings
            of a single motor (i.e. ``A.pair(B)`` followed by
            ``A.pair(C)`` is allowed).  Care should be taken not to
            set up conflicting defaults; the results may be
            surprising.

*/


/* The actual Motor type */
typedef struct
{
    PyObject_HEAD
    PyObject *port;
    PyObject *device;
    int is_detached;
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
    Py_VISIT(self->callback);
    return 0;
}


static int
Motor_clear(MotorObject *self)
{
    Py_CLEAR(self->port);
    Py_CLEAR(self->device);
    Py_CLEAR(self->callback);
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

    self->is_detached = 0;

    tmp = self->port;
    Py_INCREF(port);
    self->port = port;
    Py_XDECREF(tmp);

    tmp = self->device;
    Py_INCREF(device);
    self->device = device;
    Py_XDECREF(tmp);

    self->default_speed = 0;
    self->default_max_power = 100;
    self->default_acceleration = DEFAULT_ACCELERATION;
    self->default_deceleration = DEFAULT_DECELERATION;
    self->default_stall = 1;
    self->default_stop = STOP_BRAKE;
    self->default_position_pid[0] = 0;
    self->default_position_pid[1] = 0;
    self->default_position_pid[2] = 0;

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
    PyObject *get_fn;
    PyObject *result;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    if ((get_fn = PyObject_GetAttrString(motor->device, "get")) == NULL)
        return NULL;
    result = PyObject_CallObject(get_fn, args);
    Py_DECREF(get_fn);
    return result;
}


static PyObject *
Motor_mode(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *mode_fn;
    PyObject *result;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    if ((mode_fn = PyObject_GetAttrString(motor->device, "mode")) == NULL)
        return NULL;
    result = PyObject_CallObject(mode_fn, args);
    Py_DECREF(mode_fn);
    return result;
}


static PyObject *
Motor_pwm(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *pwm_fn;
    PyObject *result;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    if ((pwm_fn = PyObject_GetAttrString(motor->port, "pwm")) == NULL)
        return NULL;
    result = PyObject_CallObject(pwm_fn, args);
    Py_DECREF(pwm_fn);
    return result;
}


static PyObject *
Motor_float(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    /* float() is equivalent to pwm(0) */

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    return PyObject_CallMethod(motor->port, "pwm", "i", 0);
}


static PyObject *
Motor_brake(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

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

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

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

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "i", &type))
        return NULL;

    return device_is_busy(motor->device, type);
}


static PyObject *
Motor_preset(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    int32_t position;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "i", &position))
        return NULL;

    if (cmd_preset_encoder(port_get_id(motor->port), position) < 0)
        return NULL;

    Py_RETURN_NONE;
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

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    /* If we have no parameters, return a dictionary of defaults.
     * To determine that, we need to inspect the tuple `args` and
     * the dictionary `kwds` to see if they contain anything.
     */
    if (PyTuple_Size(args) == 0 &&
        (kwds == NULL || PyDict_Size(kwds) == 0))
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
Motor_callback(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *callable = NULL;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "|O:callback", &callable))
        return NULL;

    if (callable == NULL)
    {
        /* JUst wants the current callback returned */
        Py_INCREF(motor->callback);
        return motor->callback;
    }
    if (callable != Py_None && !PyCallable_Check(callable))
    {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    Py_INCREF(callable);
    Py_XDECREF(motor->callback);
    motor->callback = callable;

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

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

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
        "blocking", NULL
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
    int blocking = 1;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds,
                                    "ii|IIIIpp:run_for_degrees", kwlist,
                                    &degrees, &speed, &power, &stop,
                                    &accel, &decel, &stall, &blocking) == 0)
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
                                    (uint8_t)parsed_stop, use_profile,
                                    blocking) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static int32_t clockwise(int32_t target, long current)
{
    int32_t delta;

    if (target < current)
        delta = 360 - current + target;
    else
        delta = target - current;
    return delta % 360;
}


static int32_t anticlockwise(int32_t target, long current)
{
    int32_t delta;

    if (target < current)
        delta = current - target;
    else
        delta = 360 + current - target;

    return -(delta % 360);
}


static PyObject *
Motor_run_to_position(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorObject *motor = (MotorObject *)self;
    static char *kwlist[] = {
        "position", "speed", "max_power", "stop",
        "acceleration", "deceleration", "stall",
        "direction", "blocking", NULL
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
    int blocking = 1;
    uint32_t direction = DIRECTION_SHORTEST;
    long current_position;
    int32_t position_delta;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds,
                                    "ii|IIIIpIp:run_to_position", kwlist,
                                    &position, &speed, &power, &stop,
                                    &accel, &decel, &stall,
                                    &direction, &blocking) == 0)
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
    if (direction > 2)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid direction");
        return NULL;
    }

    /* Get the motor's absolute position.  If that's part of the
     * current mode set, we can just call get(), otherwise we need to
     * set the mode, get the info and reset the mode.  Urgh!
     */
    if (device_is_in_mode(motor->device, 3))
    {
        /* We are already in Absolute Position mode, just read it */
        if (device_read_mode_value(motor->device, 3, &current_position) < 0)
            return NULL;
    }
    else
    {
        /* First switch modes */
        if (device_push_mode(motor->device, 3) < 0 ||
            device_read_mode_value(motor->device, 3, &current_position) < 0 ||
            device_pop_mode(motor->device) < 0)
        {
            return NULL;
        }
    }

    /* Calculate the actual change */
    position = position % 360;
    switch (direction)
    {
        case DIRECTION_CLOCKWISE:
            position_delta = clockwise(position, current_position);
            break;

        case DIRECTION_ANTICLOCKWISE:
            position_delta = anticlockwise(position, current_position);
            break;

        default: /* DIRECTION_SHORTEST */
        {
            int32_t clk = clockwise(position, current_position);
            int32_t aclk = anticlockwise(position, current_position);

            if (abs(clk) < abs(aclk))
                position_delta = clk;
            else
                position_delta = aclk;
        }
    }

    if (set_acceleration(motor, accel, &use_profile) < 0 ||
        set_deceleration(motor, decel, &use_profile) < 0 ||
        set_stall(motor, stall) < 0)
        return NULL;

    if (cmd_goto_abs_position(port_get_id(motor->port),
                              position_delta, speed, power,
                              (uint8_t)parsed_stop, use_profile,
                              blocking) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
Motor_run_for_time(PyObject *self, PyObject *args, PyObject *kwds)
{
    MotorObject *motor = (MotorObject *)self;
    static char *kwlist[] = {
        "msec", "speed", "max_power", "stop",
        "acceleration", "deceleration", "stall",
        "blocking", NULL
    };
    uint32_t time;
    int32_t speed;
    uint32_t power = motor->default_max_power;
    uint32_t accel = motor->default_acceleration;
    uint32_t decel = motor->default_deceleration;
    int stall = motor->default_stall;
    uint32_t stop = MOTOR_STOP_USE_DEFAULT;
    uint8_t use_profile = 0;
    int parsed_stop;
    int blocking = 1;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds,
                                    "Ii|IIIIpp:run_for_time", kwlist,
                                    &time, &speed, &power, &stop,
                                    &accel, &decel, &stall, &blocking) == 0)
        return NULL;

    time = CLIP(time, RUN_TIME_MIN, RUN_TIME_MAX);
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

    if (cmd_start_speed_for_time(port_get_id(motor->port),
                                 time, speed, power,
                                 (uint8_t)parsed_stop, use_profile,
                                 blocking) < 0)
        return NULL;

    Py_RETURN_NONE;
}


static PyObject *
Motor_pid(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;

    if (motor->is_detached)
    {
        PyErr_SetString(cmd_get_exception(), "Motor is detached");
        return NULL;
    }

    /* If we have no parameters, return a tuple of the values */
    if (PyTuple_Size(args) == 0)
    {
        return Py_BuildValue("III",
                             motor->default_position_pid[0],
                             motor->default_position_pid[1],
                             motor->default_position_pid[2]);
    }

    /* Otherwise we should have all three values, and set them */
    if (PyArg_ParseTuple(args, "III:pid",
                         &motor->default_position_pid[0],
                         &motor->default_position_pid[1],
                         &motor->default_position_pid[2]) == 0)
        return NULL;

    if (cmd_set_pid(port_get_id(motor->port), motor->default_position_pid) < 0)
        return NULL;

    Py_RETURN_NONE;
}


/* Forward declaration; Motor_pair needs access to MotorType */
static PyObject *
Motor_pair(PyObject *self, PyObject *args);


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
    { "preset", Motor_preset, METH_VARARGS, "Set the encoder position" },
    {
        "default", (PyCFunction)Motor_default,
        METH_VARARGS | METH_KEYWORDS,
        "View or set the default values used in motor functions"
    },
    { "callback", Motor_callback, METH_VARARGS, "Get or set the callback" },
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
    {
        "run_for_time", (PyCFunction)Motor_run_for_time,
        METH_VARARGS | METH_KEYWORDS,
        "Run the motor for the given duration (in ms)"
    },
    {
        "pid", Motor_pid, METH_VARARGS,
        "Read or set the P, I and D values"
    },
    {
        "pair", Motor_pair, METH_VARARGS,
        "Pair two motors to control them as one"
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
        (void *)CALLBACK_COMPLETE
    },
    {
        "EVENT_INTERRUPTED",
        (getter)Motor_get_constant,
        NULL,
        "Callback reason code: event was interrupted",
        (void *)CALLBACK_INTERRUPTED
    },
    {
        "EVENT_STALL",
        (getter)Motor_get_constant,
        NULL,
        "Callback reason code: event has stalled",
        (void *)CALLBACK_STALLED
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
    {
        "CLOCKWISE",
        (getter)Motor_get_constant,
        NULL,
        "Value to pass as a 'direction' parameter",
        (void *)0
    },
    {
        "ANTICLOCKWISE",
        (getter)Motor_get_constant,
        NULL,
        "Value to pass as a 'direciton' parameter",
        (void *)1
    },
    {
        "SHORTEST",
        (getter)Motor_get_constant,
        NULL,
        "Value to pass as a 'direction' parameter",
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


static PyObject *
Motor_pair(PyObject *self, PyObject *args)
{
    MotorObject *motor = (MotorObject *)self;
    PyObject *other;
    PyObject *pair;
    clock_t start;

    if (PyArg_ParseTuple(args, "O:pair", &other) == 0)
        return NULL;
    if (!PyObject_IsInstance(other, (PyObject *)&MotorType))
    {
        PyErr_SetString(PyExc_TypeError,
                        "Argument to pair() must be a Motor");
        return NULL;
    }

    if ((pair = pair_get_pair(motor->port,
                              ((MotorObject *)other)->port)) == NULL)
        return NULL;

    /* Wait for ID to become valid or timeout */
    start = clock();
    while (!pair_is_ready(pair))
    {
        if (clock() - start > CLOCKS_PER_SEC)
        {
            /* Timeout */
            if (pair_unpair(pair) < 0)
                return NULL;
            Py_RETURN_FALSE;
        }
    }

    return pair;
}


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


/* Called from callback thread */
int motor_callback(PyObject *self, int event)
{
    MotorObject *motor = (MotorObject *)self;
    PyGILState_STATE gstate = PyGILState_Ensure();
    int rv = 0;

    if (motor->callback != Py_None)
    {
        PyObject *args = Py_BuildValue("(i)", event);

        rv = (PyObject_CallObject(motor->callback, args) != NULL) ? 0 : -1;
        Py_XDECREF(args);
    }

    PyGILState_Release(gstate);

    return rv;
}


void motor_detach(PyObject *self)
{
    MotorObject *motor = (MotorObject *)self;

    if (motor != NULL && self != Py_None)
        motor->is_detached = 1;
}
