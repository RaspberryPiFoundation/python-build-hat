/* firmware.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Functions to manage firmware upgrade
 */

#ifndef RPI_STRAWBERRY_FIRMWARE_H_INCLUDED
#define RPI_STRAWBERRY_FIRMWARE_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>


/* Initialise the firmware upgrade subsystem on the Pi.  HAT is unaffected.
 * Returns 0 for success, -1 for error.
 */
extern int firmware_modinit(void);

/* Finalise the firmware upgrade subsystem */
extern void firmware_demodinit(void);

/* Create the firmware object. */
extern PyObject *firmware_init(void);

/* Put the firmware object back in idle state.  Used to track
 * responses from the HAT, so that (for example) we don't attempt to
 * use the flash while it is being erased.
 *
 * XXX: need this to be a callback, and add a callback to the
 * Python-level API.
 */
extern int firmware_action_done(PyObject *firmware,
                                uint8_t reason,
                                uint8_t param);

/* Callback (from callback thread) for firmware action complete */
extern int firmware_handle_callback(uint8_t reason,
                                    uint8_t param,
                                    PyObject *firmware);


#endif /* RPI_STRAWBERRY_FIRMWARE_H_INCLUDED */
