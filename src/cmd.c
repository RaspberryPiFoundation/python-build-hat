/* cmd.c
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
 * The command interface to the Hat
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>
#include <stdbool.h>

#include "queue.h"
#include "cmd.h"
#include "protocol.h"

#define MOTOR_BIAS "bias .2"
#define MOTOR_PLIMIT "plimit .5"

/* Macro to split a uint32_t into bytes for an argument list,
 * intended for making make_request() calls a little easier
 * to write.  Use with care.
 */
#define U32_TO_BYTE_ARG(n) (n) & 0xff, \
        ((n) >> 8) & 0xff,             \
        ((n) >> 16) & 0xff,            \
        ((n) >> 24) & 0xff

/* Macro to split a uint16_t into bytes for an argument list,
 * intended for making make_request() calls a little easier
 * to write.  Use with care.
 */
#define U16_TO_BYTE_ARG(n) (n) & 0xff, \
        ((n) >> 8) & 0xff


PyObject *hub_protocol_error;


const char *error_message[] = {
    "Error: ACK",
    "Error: MACK",
    "Buffer overflow",
    "Timeout",
    "Command not recognised",
    "Invalid use",
    "Overcurrent",
    "Internal error"
};


static inline int bcd_byte(uint8_t byte)
{
    return ((byte >> 4) * 10) + (byte & 0x0f);
}


static inline int bcd_2byte(uint8_t hi, uint8_t lo)
{
    return ((hi >> 4) * 1000) +
        ((hi & 0x0f) * 100) +
        ((lo >> 4) * 10) +
        (lo & 0x0f);
}


static void handle_generic_error(uint8_t expected_type, uint8_t *buffer)
{
    /* XXX: Arguably ACK/MACK are not errors */
    if (buffer[0] != 5 || buffer[3] != expected_type)
    {
        PyErr_SetString(hub_protocol_error,
                        "Unexpected error: wrong type in error");
        return;
    }

    if (buffer[4] == 0x00 || buffer[4] > 0x08)
    {
        PyErr_SetString(hub_protocol_error, "Unknown error number");
        return;
    }

    PyErr_SetString(hub_protocol_error, error_message[buffer[4]-1]);
}


int cmd_modinit(PyObject *hub)
{
    hub_protocol_error = PyErr_NewException("build_hat.HubProtocolError",
                                            NULL, NULL);
    Py_XINCREF(hub_protocol_error);
    if (PyModule_AddObject(hub, "HubProtocolError", hub_protocol_error) < 0)
    {
        Py_XDECREF(hub_protocol_error);
        Py_CLEAR(hub_protocol_error);
        return -1;
    }

    return 0;
}


void cmd_demodinit(void)
{
    if (hub_protocol_error == NULL)
        return;
    Py_DECREF(hub_protocol_error);
    Py_CLEAR(hub_protocol_error);
    hub_protocol_error = NULL;
}


PyObject *cmd_get_exception(void)
{
    return hub_protocol_error;
}


PyObject *cmd_version_as_unicode(uint8_t *buffer)
{
    return PyUnicode_FromFormat("%d.%d.%d.%d",
                                (buffer[3] >> 4) & 7,
                                buffer[3] & 0x0f,
                                bcd_byte(buffer[2]),
                                bcd_2byte(buffer[1], buffer[0]));
}


static uint8_t *get_response(uint8_t type, bool return_feedback, int8_t portid)
{
    uint8_t *response;
    //uint8_t MAX_SKIPS=20;
    //uint8_t *skip[MAX_SKIPS];

    while (1)
    {

        if (queue_get(&response) != 0)
        {
            PyErr_SetFromErrno(hub_protocol_error);
            response = NULL;
            break;
        }
        
        if (response == NULL)
        {
            PyErr_SetString(hub_protocol_error, "Tx timeout");
            response = NULL;
            break;
        }

        /* `response` is dynamically allocated and now our responsibility */
        /*if (response[1] != 0x00)
        {
            PyErr_Format(hub_protocol_error, "Bad hub ID 0x%02x", response[1]);
            free(response);
            response = NULL;
            break;
        }*/

        /* Check for an error return */
        if (response[2] == TYPE_GENERIC_ERROR)
        {
            handle_generic_error(type, response);
            free(response);
            response = NULL;
            break;
        }

        /* Check if response is for correct port
         *
         * If response isn't for our port, skip it 
         * for now and look for next response.
         */

        /*if(portid != -1 && response[3] != portid)
        {
            if(count > MAX_SKIPS)
            {
                queue_return_buffer(response);
                PyErr_SetString(hub_protocol_error, "Max skips exceeded");
                response = NULL;
                break;
            }
            skip[count] = response;
            count++;
            continue;
        }*/

        /* Ignore Feedback messages unless explicitly asked for them */
        if (return_feedback || response[2] != TYPE_PORT_OUTPUT_FEEDBACK)
        {
            break;
        }

        free(response);
    }

    /*if(count != 0)
    {
        for(int i=0;i<count;i++)
            queue_return_buffer(skip[i]);
    }*/

    return response;
}


static uint8_t *make_request_uart(bool return_feedback, uint8_t type, uint8_t port_id, char *cmd)
{
    uint8_t *buffer = malloc(strlen(cmd)+1);
    memcpy(buffer, cmd, strlen(cmd));
    buffer[strlen(cmd)] = 0;

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    if (queue_clear_responses() != 0 || queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return NULL;
    }

    /* `buffer` now belongs to the comms thread, which will free it when
     * it is done with it.
     */
    //return get_response(type, return_feedback, port_id);
    return NULL;
}


PyObject *cmd_get_firmware_version(void)
{
    PyObject *version;
    return version;
}


int cmd_get_port_value(uint8_t port_id, uint8_t selindex)
{
    char buf[100];
    sprintf(buf, "port %d ; selonce %d\r", port_id, selindex);
    make_request_uart(false, TYPE_PORT_OUTPUT, port_id, buf);
    return 0;
}


static int wait_for_complete_feedback(uint8_t port_id, uint8_t *response)
{
    int rv = 0;
    uint8_t *buffer = response;

    while (rv == 0)
    {
        if (buffer != NULL)
        {
            if (buffer[1] != 0x00)
            {
                PyErr_Format(hub_protocol_error,
                             "Bad hub ID 0x%02x", buffer[1]);
                free(buffer);
                return -1;
            }
            if (buffer[2] == TYPE_GENERIC_ERROR)
            {
                handle_generic_error(TYPE_PORT_OUTPUT, buffer);
                free(buffer);
                return -1;
            }

            if (/*buffer[0] == 5 &&
                buffer[2] == TYPE_PORT_OUTPUT_FEEDBACK &&*/
                buffer[3] == port_id)
            {
                /*if ((buffer[4] & 0x20) != 0)
                {
                    // The motor has stalled!
                    free(buffer);
                    PyErr_SetString(hub_protocol_error, "Motor stalled");
                    return -1;
                }
                if ((buffer[4] & 0x04) != 0)
                {
                    // "Current Command(s) Discarded" bit set
                    free(buffer);
                    PyErr_SetString(hub_protocol_error, "Port busy");
                    return -1;
                }
                if ((buffer[4] & 0x02) != 0)
                {*/
                    // "Current command(s) Complete" bit set
                    free(buffer);
                    return 0;
               // }
            }
            free(buffer);
        }

        /* Try again */
        rv = queue_get(&buffer);
    }

    /* Something went wrong, check with errno */
    PyErr_SetFromErrno(hub_protocol_error);
    return -1;
}


int cmd_set_pwm(uint8_t port_id, int8_t pwm)
{
    char buf[100];
    sprintf(buf, "port %u ; pwm ; set %f\r", port_id, ((float)pwm) / 100.0);
    make_request_uart(true, TYPE_PORT_OUTPUT, port_id, buf);
    return 0;
}


int cmd_start_speed(uint8_t port_id,
                    int8_t speed,
                    uint8_t max_power,
                    uint8_t use_profile)
{
    char buf[1000];
    sprintf(buf, "port %u ; combi 0 1 0 2 0 3 0 ; select 0 ; " MOTOR_PLIMIT " ; " MOTOR_BIAS " ; pid 0 0 0 s1 1 0 0.003 0.01 0 100; set %d\r", port_id, speed);
    make_request_uart(true, TYPE_PORT_OUTPUT, port_id, buf);
    return 0;
}


int cmd_start_speed_for_time(uint8_t port_id,
                             uint16_t time,
                             int8_t speed,
                             uint8_t max_power,
                             uint8_t stop,
                             uint8_t use_profile,
                             bool blocking)
{
    char buf[1000];
    // Have to use pulse during parameter to represent speed (it is a PWM value)
    sprintf(buf, "port %u ; pwm ; " MOTOR_PLIMIT " ; " MOTOR_BIAS " ; set pulse %f 0.0 %f 0\r", port_id, ((float)speed) / 100.0, (double)time / 1000.0);
    make_request_uart(true, TYPE_PORT_OUTPUT, port_id, buf);
    if (blocking){
        return wait_for_complete_feedback(port_id, NULL);
    }
    return 0;
}


int cmd_start_speed_for_degrees(uint8_t port_id,
                                double newpos,
                                double curpos,
                                int8_t speed,
                                uint8_t max_power,
                                uint8_t stop,
                                uint8_t use_profile,
                                bool blocking)
{
    char buf[1000];
    sprintf(buf, "port %u ; combi 0 1 0 2 0 3 0 ; select 0 ; " MOTOR_PLIMIT " ; " MOTOR_BIAS " ; pid 0 0 1 s4 0.0027777778 0 5 0 .1 3 ; set ramp %f %f %f 0\r", port_id, curpos, newpos, (newpos - curpos) / (double)speed);
    make_request_uart(true, TYPE_PORT_OUTPUT, port_id, buf);
    if (blocking){
        return wait_for_complete_feedback(port_id, NULL);
    }
    return 0;
}


int cmd_goto_abs_position(uint8_t port_id,
                          int32_t position,
                          int8_t speed,
                          uint8_t max_power,
                          uint8_t stop,
                          uint8_t use_profile,
                          bool blocking)
{

    char buf[1000];
    // The position PID doesn't actually support speed, so try to make use of plimit somewhat?
    sprintf(buf, "port %d ; combi 0 1 0 2 0 3 0 ; select 0 ; plimit %f ; " MOTOR_BIAS " ;  pid 0 0 5 s2 0.0027777778 1 5 0 .1 3 ; set %f ;\r", port_id, (float)speed/100.0, (float)position/360.0);
    make_request_uart(false, TYPE_PORT_OUTPUT, port_id, buf);
    /*if (blocking){
        return wait_for_complete_feedback(port_id, NULL);
    }*/
    return 0;
}


int cmd_set_mode(uint8_t port_id, uint8_t mode, uint8_t notifications)
{
    char buf[200];
    if(notifications)
        sprintf(buf, "port %u ; combi %d; select %d\r", port_id, mode, mode);
    else
        sprintf(buf, "port %u ; combi %d; selonce %d\r", port_id, mode, mode);
    make_request_uart(false, TYPE_PORT_OUTPUT, port_id, buf);
    return 0;
}


int cmd_set_combi_mode(uint8_t port_id,
                       int combi_index,
                       uint8_t *modes,
                       int num_modes,
                       uint8_t notifications)
{
    int i;
    char buf[200];
    char modestr[20];
    char mod[20];

    memset(modestr, 0, sizeof modestr);
    for(i = 0; i < num_modes; i++){
        memset(mod, 0, sizeof mod);
        sprintf(mod, "%d %d ", modes[i] >> 4, modes[i] & 0xf);
        strcat(modestr, mod);
    }

    if(notifications)
        sprintf(buf, "port %u ; combi %d %s ; select %d\r", port_id, combi_index, modestr, combi_index);
    else
        sprintf(buf, "port %u ; combi %d %s\r", port_id, combi_index, modestr);

    make_request_uart(false, TYPE_PORT_OUTPUT, port_id, buf);
    return 0;
}
