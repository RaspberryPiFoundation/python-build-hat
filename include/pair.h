/* pair.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Motor Pair operations
 */

#ifndef RPI_STRAWBERRY_PAIR_H_INCLUDED
#define RPI_STRAWBERRY_PAIR_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

extern int pair_modinit(void);
extern void pair_demodinit(void);

/* Creates a new MotorPair class object, pairing the ports */
extern PyObject *pair_get_pair(PyObject *primary, PyObject *secondary);

/* Returns TRUE if the motor pair object is registered as attached */
extern int pair_is_ready(PyObject *self);

/* Signals that the given pair's attachment message has arrived */
extern int pair_attach_port(uint8_t id,
                            uint8_t primary_id,
                            uint8_t secondary_id,
                            uint16_t device_type);

/* Signals that the given pair's detachment message has arrived */
extern int pair_detach_port(uint8_t id);

/* Signals that a port detachment message message has arrived that
 * might be one of the ports in a pair.
 */
extern int pair_detach_subport(uint8_t id);

/* Detaches the virtual port */
extern int pair_unpair(PyObject *self);

/* Signals an output command status */
extern int pair_feedback_status(uint8_t port_id, uint8_t status);


#endif /* RPI_STRAWBERRY_PAIR_H_INCLUDED */
