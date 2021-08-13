/* port.h
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
 * Constants and types for device ports of a hat
 */

#ifndef RPI_STRAWBERRY_PORT_H_INCLUDED
#define RPI_STRAWBERRY_PORT_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "cmd.h"

enum d_type { FLOAT, INTEGER } ;

typedef struct data_s
{
    float f_data;
    int   i_data;
    enum d_type t;
} data_t;

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
#ifdef BUILD_FOR_HW_VER_1
#define NUM_HUB_PORTS 6
#else
#define NUM_HUB_PORTS 4
#endif

extern int port_modinit(void);
extern void port_demodinit(void);

extern PyObject *port_init(void);

/* The following are called only from the background comms thread */
extern int port_attach_port(uint8_t port_id,
                            uint16_t type_id,
                            uint8_t *hw_revision,
                            uint8_t *fw_revision);
extern int port_detach_port(uint8_t port_id);
extern int port_new_any_value(uint8_t port_id,
                                int entry,
                                data_t *buffer);
extern int port_new_format(uint8_t port_id);
extern int port_feedback_status(uint8_t port_id, uint8_t status);

extern int port_get_id(PyObject *port);
extern PyObject *port_get_device(PyObject *port);
extern PyObject *ports_get_value_dict(PyObject *portset);
extern PyObject *port_get_motor(PyObject *port);  /* Returns new reference */
extern int port_is_motor(PyObject *port); /* True if a motor is connected */
extern int port_set_motor_preset(PyObject *port, long position);

/* The following are called only from the callback thread */
extern int port_handle_callback(uint8_t port_id, uint8_t event);
extern int port_handle_motor_callback(uint8_t port_id, uint8_t event);
extern int ports_handle_callback(uint8_t overpower_state);
extern int port_handle_device_callback(uint8_t port_id, uint8_t event);

extern int port_set_device_format(uint8_t port_id, uint8_t mode, uint8_t type);
#endif /* RPI_STRAWBERRY_PORT_H_INCLUDED */
