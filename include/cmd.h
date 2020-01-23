/* cmd.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Functions implementing "wireless protocol" commands
 */

#ifndef RPI_STRAWBERRY_CMD_H_INCLUDED
#define RPI_STRAWBERRY_CMD_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>

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


/* Initialises the command subsystem, creating the exception used for
 * protocol errors.  Returns 0 for success, -1 for error.
 */
extern int cmd_init(PyObject *hub);


/* Finalise the command subsystem */
extern void cmd_deinit(void);


/* Convert version data encoded in a buffer into a Python string */
extern PyObject *cmd_version_as_unicode(uint8_t *buffer);

/* Sends a Hub Property Request Update command for the HW Version
 * property and waits for the Hub Property Update in response.  The
 * version is converted into a Python string of the form "M.m.BB.bbbb"
 */
extern PyObject *cmd_get_hardware_version(void);

/* Sends a Hub Property Request Update command for the FW Version
 * property and waits for the Hub Property Update in response.  The
 * version is converted into a Python string of the form "M.m.BB.bbbb"
 */
extern PyObject *cmd_get_firmware_version(void);

/* Sends a Hub Port Info Request command for the mode info of the
 * given port.  Returns a port_modes_t structure containing the
 * returned information, or NULL on error (when an exception will
 * already be raised).
 */
extern port_modes_t *cmd_get_port_modes(uint8_t port_id);


#endif /* RPI_STRAWBERRY_CMD_H_INCLUDED */
