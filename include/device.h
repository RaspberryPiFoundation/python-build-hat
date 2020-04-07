/* device.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Basic device operations on a port
 */

#ifndef RPI_STRAWBERRY_DEVICE_H_INCLUDED
#define RPI_STRAWBERRY_DEVICE_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

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

/* Parse an input buffer and update the stored values.  Returns the number
 * of bytes in the buffer consumed, or -1 on failure.
 */
extern int device_new_value(PyObject *self, uint8_t *buffer, uint16_t nbytes);

/* Parse an input buffer and update the stored values for a single
 * mode/dataset combination from a Combi-mode value update.  Returns
 * the number of bytes in the buffer consumed, or -1 on failure.
 */
extern int device_new_combi_value(PyObject *self,
                                  int entry,
                                  uint8_t *buffer,
                                  uint16_t nbytes);

/* Update internal state from the format information */
extern int device_new_format(PyObject *self);

/* Update the internal "motor busy" flag */
extern int device_set_port_busy(PyObject *self, uint8_t is_busy);

/* Check if the device is busy as a motor or with mode changes */
extern PyObject *device_is_busy(PyObject *self, int type);


#endif /* RPI_STRAWBERRY_DEVICE_H_INCLUDED */
