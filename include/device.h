/* device.h
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
 * Basic device operations on a port
 */

#ifndef RPI_STRAWBERRY_DEVICE_H_INCLUDED
#define RPI_STRAWBERRY_DEVICE_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "port.h"

/* Device ID manifest constants */
#define ID_MOTOR_MEDIUM            0x30
#define ID_MOTOR_LARGE             0x31
#define ID_COLOUR                  0x3d
#define ID_DISTANCE                0x3e
#define ID_FORCE                   0x3f
#define ID_MOTOR_SMALL             0x41
#define ID_STONE_GREY_MOTOR_MEDIUM 0x4b
#define ID_STONE_GREY_MOTOR_LARGE  0x4c

#define NUM_PORTS 4
extern pthread_mutex_t mtxgotdata[NUM_PORTS];

extern int device_modinit(void);
extern void device_demodinit(void);

/* Create a new Device class object for attaching to a port */
extern PyObject *device_new_device(PyObject *port,
                                   uint16_t type_id,
                                   uint8_t *hw_revision,
                                   uint8_t *fw_revision);

/* Returns the dictionary of device information, or NULL on failure
 * with a Python exception already raised.
 */
extern PyObject *device_get_info(PyObject *self, uint8_t port_id);

/* Parse an input buffer and update the stored values for a single
 * mode/dataset combination from a Combi-mode or simple mode value update.  Returns
 * the number of bytes in the buffer consumed, or -1 on failure.
 */
extern int device_new_any_value(PyObject *self,
                                  int entry,
                                  data_t *buffer);

/* Update internal state from the format information */
extern int device_new_format(PyObject *self);

/* Update the internal "motor busy" flag */
extern int device_set_port_busy(PyObject *self, uint8_t is_busy);

/* Check if the device is busy as a motor or with mode changes */
extern PyObject *device_is_busy(PyObject *self, int type);

/* Mark the device as detached from the port */
extern void device_detach(PyObject *self);

/* Check if the device is in the given mode as a simple mode, or that
 * the device is in a combination mode and the mode together with any
 * dataset is in that combination.  This is only intended to support
 * motors looking for Absolute Position mode, so may be of limited
 * other utility.
 */
extern int device_is_in_mode(PyObject *self, int mode);

/* Read the value associated with the first dataset found for this
 * mode, assuming the mode is present (i.e. device_is_in_mode() has
 * returned True).  Return 0 on success with the value written to the
 * long passed in (which means that the value must not be floating
 * point), -1 on failure.  This is only intended to support motors
 * looking for Absolute Position mode information, so may be of
 * limited other utility.
 */
extern int device_read_mode_value(PyObject *self, int mode, long *pvalue);

/* Save the current mode (simple or combi) and set the given simple
 * mode.  Returns 0 on success, -1 on failure.
 */
extern int device_push_mode(PyObject *self, int mode);

/* Restore the previously pushed mode */
extern int device_pop_mode(PyObject *self);

/* Ensures that the initialisation that cannot be done in the background
 * has in fact been done.
 */
extern int device_ensure_mode_info(PyObject *self);

/* Handles device callbacks */
extern int device_callback(PyObject *self, int event);

int device_set_device_format(PyObject *device, uint8_t modei, uint8_t type);

#endif /* RPI_STRAWBERRY_DEVICE_H_INCLUDED */
