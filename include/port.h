/* port.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Constants and types for device ports of a hat
 */

#ifndef RPI_STRAWBERRY_PORT_H_INCLUDED
#define RPI_STRAWBERRY_PORT_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>


/* Total number of physical ports connected to the hat */
#define NUM_HUB_PORTS 6

extern int port_init(PyObject *hub);
extern void port_deinit(void);

/* The following are called only from the background comms thread */
extern int port_attach_port(uint8_t port_id,
                            uint16_t type_id,
                            uint8_t *hw_revision,
                            uint8_t *fw_revision);
extern int port_detach_port(uint8_t port_id);

extern int port_get_id(PyObject *port);


#endif /* RPI_STRAWBERRY_PORT_H_INCLUDED */
