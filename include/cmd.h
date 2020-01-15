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


/* Initialises the command subsystem, creating the exception used for
 * protocol errors.  Returns 0 for success, -1 for error.
 */
extern int cmd_init(PyObject *hub);


/* Finalise the command subsystem */
extern void cmd_deinit(void);


/* Sends a Hub Property Request Update command for the HW Version
 * property and waits for the Hub Property Update in response.  The
 * version is converted into a Python string of the form "M.m.BB.bbbb"
 */
extern PyObject *cmd_get_hardware_version(void);

#endif /* RPI_STRAWBERRY_CMD_H_INCLUDED */
