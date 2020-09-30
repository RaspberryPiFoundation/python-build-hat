/* motor-settings.h
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
 * Common definitions used by motors and motor pairs
 */

#ifndef RPI_STRAWBERRY_MOTOR_SETTINGS_H_INCLUDED
#define RPI_STRAWBERRY_MOTOR_SETTINGS_H_INCLUDED


/* The invalid port ID, used to mark disconnected ports */
#define INVALID_ID (0xff)

/* Default values */
#define DEFAULT_ACCELERATION 100
#define DEFAULT_DECELERATION 150

/* Protocol flags for the acceleration/deceleration profiles */
#define USE_PROFILE_ACCELERATE 0x01
#define USE_PROFILE_DECELERATE 0x02

/* Values passed into Python methods */
#define MOTOR_STOP_FLOAT 0
#define MOTOR_STOP_BRAKE 1
#define MOTOR_STOP_HOLD  2
#define MOTOR_STOP_USE_DEFAULT 3

#define DIRECTION_CLOCKWISE     0
#define DIRECTION_ANTICLOCKWISE 1
#define DIRECTION_SHORTEST      2

/* Values passed to the cmd functions */
#define STOP_FLOAT 0
#define STOP_HOLD 126
#define STOP_BRAKE 127

/* Limits */
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


#endif /* RPI_STRAWBERRY_MOTOR_SETTINGS_H_INCLUDED */
