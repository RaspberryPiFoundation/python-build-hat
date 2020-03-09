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

#include "cmd.h"

/* Support types for ports */
typedef struct min_max_s
{
    float min;
    float max;
} min_max_t;

typedef struct mode_info_s
{
    char name[12];
    char symbol[5];
    uint8_t capability[6];
    uint8_t input_mapping;
    uint8_t output_mapping;
    min_max_t raw;
    min_max_t percent;
    min_max_t si;
    value_format_t format;
} mode_info_t;


/* Total number of physical ports connected to the hat */
#define NUM_HUB_PORTS 6

extern int port_modinit(void);
extern void port_demodinit(void);

extern PyObject *port_init(void);

/* The following are called only from the background comms thread */
extern int port_attach_port(uint8_t port_id,
                            uint16_t type_id,
                            uint8_t *hw_revision,
                            uint8_t *fw_revision);
extern int port_detach_port(uint8_t port_id);
extern int port_new_value(uint8_t port_id, uint8_t *buffer, uint16_t nbytes);
extern int port_new_format(uint8_t port_id);
extern int port_feedback_status(uint8_t port_id, uint8_t status);

extern int port_get_id(PyObject *port);
extern PyObject *port_get_device(PyObject *port);

extern int port_ensure_mode_info(PyObject *port);
extern mode_info_t *port_get_mode(PyObject *port, int mode);
extern int port_check_mode(PyObject *port, int mode);


#endif /* RPI_STRAWBERRY_PORT_H_INCLUDED */
