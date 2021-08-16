/* cmd.h
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
 * Functions implementing "wireless protocol" commands
 */

#ifndef RPI_STRAWBERRY_CMD_H_INCLUDED
#define RPI_STRAWBERRY_CMD_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>
#include <stdbool.h>

/* Return type for cmd_get_port_modes() */
typedef struct port_modes_s
{
    uint8_t capabilities;  /* CAP_ flags (see below) */
    uint8_t count;         /* Total number of port modes */
    uint16_t input_mode_mask;
    uint16_t output_mode_mask;
} port_modes_t;

#define CAP_MODE_HAS_OUTPUT     0x01
#define CAP_MODE_HAS_INPUT      0x02
#define CAP_MODE_COMBINABLE     0x04
#define CAP_MODE_SYNCHRONIZABLE 0x08

#define MAX_COMBI_MODES 8
typedef uint16_t combi_mode_t[MAX_COMBI_MODES];

typedef struct value_format_s
{
    uint8_t datasets;
    uint8_t type;
    uint8_t figures;
    uint8_t decimals;
} value_format_t;

#define FORMAT_8BIT  0x00
#define FORMAT_16BIT 0x01
#define FORMAT_32BIT 0x02
#define FORMAT_FLOAT 0x03

#define FW_CHECKSUM_STORED 0x00
#define FW_CHECKSUM_CALC   0x01


/* Initialises the command subsystem, creating the exception used for
 * protocol errors.  Returns 0 for success, -1 for error.
 */
extern int cmd_modinit(PyObject *hub);


/* Finalise the command subsystem */
extern void cmd_demodinit(void);


/* Return the hub_protocol_error exception object, as an alternative
 * to having a global running around.
 */
extern PyObject *cmd_get_exception(void);


/* Convert version data encoded in a buffer into a Python string */
extern PyObject *cmd_version_as_unicode(uint8_t *buffer);

/* Sends a Port Information Request command for the value and waits
 * for the Port Value (Single or Combination) response.  The values
 * will be inserted into the device structure.  Returns 0 on success
 * (and the device->values entry will be updated) or -1 on error (when
 * an exception will already be raised).
 */
extern int cmd_get_port_value(uint8_t port_id, uint8_t selindex);


/* Sends a Port Mode Information Request command for the format of the
 * data presented in this mode.  Returns the value in the `format`
 * parameter.  Returns 0 on success, -1 on error (when a Python
 * exception will have been raised).
 */
extern int cmd_get_mode_format(uint8_t port_id,
                               uint8_t mode_id,
                               value_format_t *format);

/* Sends a Port Output command to set the start power of the given
 * port.  Returns 0 on success, -1 on error (when a Python exception
 * will have been raised).
 */
extern int cmd_set_pwm(uint8_t port_id, int8_t pwm);

/* Sends a Port Output command to set start or hold a motor.  Returns
 * 0 on success, -1 on error (when a Python exception will have been
 * raised).
 */
extern int cmd_start_speed(uint8_t port_id,
                           int8_t speed,
                           uint8_t max_power,
                           uint8_t use_profile);

/* Sends a Port Output command to run a motor for a given number of
 * milliseconds.  Returns 0 on success, -1 on error (when a Python
 * exception will have been raised).
 */
extern int cmd_start_speed_for_time(uint8_t port_id,
                                    uint16_t time,
                                    int8_t speed,
                                    uint8_t max_power,
                                    uint8_t stop,
                                    uint8_t use_profile,
                                    bool blocking);

/* Sends a Port Output command to run a motor through a given angle.
 * Returns 0 on success, -1 on error (when a Python exception will
 * have been raised).
 */
extern int cmd_start_speed_for_degrees(uint8_t port_id,
                                       double newpos,
                                       double curpos,
                                       int8_t speed,
                                       uint8_t max_power,
                                       uint8_t stop,
                                       uint8_t use_profile,
                                       bool blocking);

/* Sends a Port Output command to run a motor to a specified
 * position.  Returns 0 on success, -1 on error (when a Python
 * exception will have been raised).
 *
 * Sadly it seems the underlying "Goto Absolute Position" command
 * actually goes to the _relative_ position.
 */
extern int cmd_goto_abs_position(uint8_t port_id,
                                 int32_t position,
                                 int8_t speed,
                                 uint8_t max_power,
                                 uint8_t stop,
                                 uint8_t use_profile,
                                 bool blocking);

/* Sends a Port Input Format Setup command (possibly more, depending)
 * to set the mode used on the given port.  Returns 0 on success, -1
 * on error (when a Python exception will have been raised already).
 */
extern int cmd_set_mode(uint8_t port_id, uint8_t mode, uint8_t notifications);

/* Sends the required set of Port Input Format Setup Combined commands
 * to set the combi mode used on the given port.  Returns 0 on
 * success, -1 on error (when a Python exception will have been raised
 * already.)  On error, the function tries to leave the device in a
 * reset state, but this cannot be guaranteed.
 */
extern int cmd_set_combi_mode(uint8_t port,
                              int combi_index,
                              uint8_t *modes,
                              int num_modes,
                              uint8_t notifications);

#endif /* RPI_STRAWBERRY_CMD_H_INCLUDED */
