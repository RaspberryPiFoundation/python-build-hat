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

static uint8_t *make_request(bool return_feedback,
                             uint8_t nbytes,
                             uint8_t type, ...)
{
    va_list args;
    uint8_t *buffer = malloc(nbytes);
    int i;
    int portid = -1;

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    buffer[0] = nbytes;
    buffer[1] = 0x00; /* Hub ID, must be zero */
    buffer[2] = type;

    va_start(args, type);
    for (i = 3; i < nbytes; i++)
    {
        buffer[i] = va_arg(args, int);
        if(i == 3)
            portid = buffer[i];
    }
    va_end(args);

    if (queue_clear_responses() != 0 || queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return NULL;
    }

    /* `buffer` now belongs to the comms thread, which will free it when
     * it is done with it.
     */

    return get_response(type, return_feedback, portid);
}


PyObject *cmd_get_hardware_version(void)
{
    PyObject *version;
    uint8_t *response = make_request(false, 5, TYPE_HUB_PROPERTY,
                                     PROP_HW_VERSION,
                                     PROP_OP_REQUEST);

    if (response == NULL)
        return NULL;

    if (response[0] != 9 ||
        response[2] != TYPE_HUB_PROPERTY ||
        response[3] != PROP_HW_VERSION ||
        response[4] != PROP_OP_UPDATE)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to H/W Version Request");
        return NULL;
    }

    version = cmd_version_as_unicode(response + 5);
    free(response);

    return version;
}


PyObject *cmd_get_firmware_version(void)
{
    PyObject *version;
    uint8_t *response = make_request(false, 5, TYPE_HUB_PROPERTY,
                                     PROP_FW_VERSION,
                                     PROP_OP_REQUEST);

    if (response == NULL)
        return NULL;

    if (response[0] != 9 ||
        response[2] != TYPE_HUB_PROPERTY ||
        response[3] != PROP_FW_VERSION ||
        response[4] != PROP_OP_UPDATE)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to F/W Version Request");
        return NULL;
    }

    version = cmd_version_as_unicode(response + 5);
    free(response);

    return version;
}


int cmd_get_port_value(uint8_t port_id, uint8_t selindex)
{
    char buf[100];
    sprintf(buf, "port %d ; selonce %d\r", port_id, selindex);
    make_request_uart(false, TYPE_PORT_OUTPUT, port_id, buf);
    return 0;
}


int cmd_get_port_modes(uint8_t port_id, port_modes_t *results)
{
    uint8_t *response = make_request(false, 5, TYPE_PORT_INFO_REQ,
                                     port_id,
                                     PORT_INFO_MODE);

    if (response == NULL)
        return -1;

    if (response[0] != 11 ||
        response[2] != TYPE_PORT_INFO ||
        response[3] != port_id ||
        response[4] != PORT_INFO_MODE)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port Information Request");
        return -1;
    }

    results->capabilities = response[5];
    results->count = response[6];
    results->input_mode_mask = response[7] | (response[8] << 8);
    results->output_mode_mask = response[9] | (response[10] << 8);
    free(response);

    return 0;
}


int cmd_get_combi_modes(uint8_t port_id, combi_mode_t combi)
{
    int i;
    uint8_t *response = make_request(false, 5, TYPE_PORT_INFO_REQ,
                                     port_id,
                                     PORT_INFO_MODE_COMBINATIONS);

    if (response == NULL)
        return -1;

    /* The length of this packet is variable, but should be between
     * 7 and 21, and an odd number.
     */
    if (response[0] < 7 ||
        response[0] > 21 ||
        ((response[0] & 1) != 1) ||
        response[2] != TYPE_PORT_INFO ||
        response[3] != port_id ||
        response[4] != PORT_INFO_MODE_COMBINATIONS)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port Information Request");
        return -1;
    }

    memset(combi, 0, sizeof(combi_mode_t));
    for (i = 0; 2*i+5 < response[0]; i++)
        combi[i] = response[2*i+5] | (response[2*i+6] << 8);

    return 0;
}


int cmd_get_mode_name(uint8_t port_id, uint8_t mode_id, char *name)
{
    uint8_t *response = make_request(false, 6, TYPE_PORT_MODE_REQ,
                                     port_id,
                                     mode_id,
                                     MODE_INFO_NAME);

    if (response == NULL)
        return -1;

    /* The length of this packet is variable, but should be between
     * 7 and 17 bytes.
     */
    if (response[0] < 6 || response[0] > 17 ||
        response[2] != TYPE_PORT_MODE ||
        response[3] != port_id ||
        response[4] != mode_id ||
        response[5] != MODE_INFO_NAME)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Mode Name Request");
        return -1;
    }

    if (response[0] == 6)
    {
        name[0] = '\0';
    }
    else
    {
        memcpy(name, response+6, response[0]-6);
        name[response[0]-6] = '\0';
    }

    return 0;
}


static int get_mode_min_max(uint8_t port_id,
                            uint8_t mode_id,
                            float *pmin,
                            float *pmax,
                            uint8_t info_type,
                            const char *info_name)
{
    uint8_t *response = make_request(false, 6, TYPE_PORT_MODE_REQ,
                                     port_id,
                                     mode_id,
                                     info_type);
    uint32_t temp;

    if (response == NULL)
        return -1;

    if (response[0] != 14 ||
        response[2] != TYPE_PORT_MODE ||
        response[3] != port_id ||
        response[4] != mode_id ||
        response[5] != info_type)
    {
        free(response);
        PyErr_Format(hub_protocol_error,
                     "Unexpected reply to Mode %s Request",
                     info_name);
        return -1;
    }

    /* Bytes 6-9 and 10-13 should be the bit patterns of floats */
    /* Make sure our values have the right endianness first */
    temp = response[6] |
        (response[7] << 8) |
        (response[8] << 16) |
        (response[9] << 24);
    memcpy(pmin, &temp, 4);

    temp = response[10] |
        (response[11] << 8) |
        (response[12] << 16) |
        (response[13] << 24);
    memcpy(pmax, &temp, 4);

    return 0;
}


int cmd_get_mode_raw(uint8_t port_id,
                     uint8_t mode_id,
                     float *pmin,
                     float *pmax)
{
    return get_mode_min_max(port_id, mode_id, pmin, pmax,
                            MODE_INFO_RAW, "Raw");
}

int cmd_get_mode_percent(uint8_t port_id,
                         uint8_t mode_id,
                         float *pmin,
                         float *pmax)
{
    return get_mode_min_max(port_id, mode_id, pmin, pmax,
                            MODE_INFO_PCT, "Percent");
}


int cmd_get_mode_si(uint8_t port_id,
                    uint8_t mode_id,
                    float *pmin,
                    float *pmax)
{
    return get_mode_min_max(port_id, mode_id, pmin, pmax,
                            MODE_INFO_SI, "SI");
}


int cmd_get_mode_symbol(uint8_t port_id, uint8_t mode_id, char *symbol)
{
    uint8_t *response = make_request(false, 6, TYPE_PORT_MODE_REQ,
                                     port_id,
                                     mode_id,
                                     MODE_INFO_SYMBOL);

    if (response == NULL)
        return -1;

    /* The length of this packet is variable, but should be between
     * 7 and 11 bytes.
     */
    if (response[0] < 6 || response[0] > 11 ||
        response[2] != TYPE_PORT_MODE ||
        response[3] != port_id ||
        response[4] != mode_id ||
        response[5] != MODE_INFO_SYMBOL)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Mode Symbol Request");
        return -1;
    }

    if (response[0] == 6)
    {
        symbol[0] = '\0';
    }
    else
    {
        memcpy(symbol, response+6, response[0]-6);
        symbol[response[0]-6] = '\0';
    }

    return 0;
}


int cmd_get_mode_mapping(uint8_t port_id,
                         uint8_t mode_id,
                         uint8_t *pinput_mapping,
                         uint8_t *poutput_mapping)
{
    uint8_t *response = make_request(false, 6, TYPE_PORT_MODE_REQ,
                                     port_id,
                                     mode_id,
                                     MODE_INFO_MAPPING);
    if (response == NULL)
        return -1;

    if (response[0] != 8 ||
        response[2] != TYPE_PORT_MODE ||
        response[3] != port_id ||
        response[4] != mode_id ||
        response[5] != MODE_INFO_MAPPING)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Mode Mapping Request");
        return -1;
    }

    *poutput_mapping = response[6];
    *pinput_mapping = response[7];

    return 0;
}


int cmd_get_mode_capability(uint8_t port_id,
                            uint8_t mode_id,
                            uint8_t capability[6])
{
    uint8_t *response = make_request(false, 6, TYPE_PORT_MODE_REQ,
                                     port_id,
                                     mode_id,
                                     MODE_INFO_CAPABILITY);
    if (response == NULL)
        return -1;

    if (response[0] != 12 ||
        response[2] != TYPE_PORT_MODE ||
        response[3] != port_id ||
        response[4] != mode_id ||
        response[5] != MODE_INFO_CAPABILITY)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Mode Capability Request");
        return -1;
    }

    memcpy(capability, response+6, 6);

    return 0;
}


int cmd_get_mode_format(uint8_t port_id,
                        uint8_t mode_id,
                        value_format_t *format)
{
    uint8_t *response = make_request(false, 6, TYPE_PORT_MODE_REQ,
                                     port_id,
                                     mode_id,
                                     MODE_INFO_FORMAT);

    if (response == NULL)
        return -1;

    if (response[0] != 10 ||
        response[2] != TYPE_PORT_MODE ||
        response[3] != port_id ||
        response[4] != mode_id ||
        response[5] != MODE_INFO_FORMAT)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Mode Format Request");
        return -1;
    }

    format->datasets = response[6];
    format->type = response[7];
    format->figures = response[8];
    format->decimals = response[9];

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


int cmd_set_acceleration(uint8_t port_id, uint32_t accel)
{
    uint8_t *response = make_request(true, 9, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_SET_ACC_TIME,
                                     accel & 0xff,
                                     (accel >> 8) & 0xff,
                                     0);
    if (response == NULL)
        return -1;

    return wait_for_complete_feedback(port_id, response);
}


int cmd_set_deceleration(uint8_t port_id, uint32_t decel)
{
    uint8_t *response = make_request(true, 9, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_SET_DEC_TIME,
                                     decel & 0xff,
                                     (decel >> 8) & 0xff,
                                     0);
    if (response == NULL)
        return -1;

    return wait_for_complete_feedback(port_id, response);
}


int cmd_set_pid(uint8_t port_id, uint32_t pid[3])
{
    uint8_t *response = make_request(true, 22, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_WRITE_PID,
                                     U32_TO_BYTE_ARG(pid[0]),
                                     U32_TO_BYTE_ARG(pid[1]),
                                     U32_TO_BYTE_ARG(pid[2]),
                                     U32_TO_BYTE_ARG(10000));
    if (response == NULL)
        return -1;

    return wait_for_complete_feedback(port_id, response);
}


int cmd_set_stall(uint8_t port_id, int stall)
{
    uint8_t *response = make_request(true, 7, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_STALL_CONTROL,
                                     stall ? 1 : 0);
    if (response == NULL)
        return -1;

    return wait_for_complete_feedback(port_id, response);
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
    sprintf(buf, "port %u ; combi 0 1 0 2 0 3 0 ; select 0 ; " MOTOR_PLIMIT " ; " MOTOR_BIAS " ; pid 0 0 5 s2 0.0027777778 1 5 0 .1 3 ; set ramp %f %f %f 0\r", port_id, curpos, newpos, (newpos - curpos) / (double)speed);
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


int cmd_preset_encoder(uint8_t port_id, int32_t position)
{
    uint8_t *response = make_request(true, 10, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_PRESET_ENCODER,
                                     U32_TO_BYTE_ARG((uint32_t)position));
    if (response == NULL)
        return -1;
    return wait_for_complete_feedback(port_id, response);
}


int cmd_write_mode_data(uint8_t port_id,
                        uint8_t mode,
                        ssize_t nbytes,
                        const char *bytes)
{
    ssize_t len = 7 + nbytes;
    uint8_t *buffer;
    uint8_t *response;
    int offset = 0;

    if (len > 0x7f)
        offset = 1;

    if ((buffer = malloc(len + offset)) == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    if (offset)
    {
        buffer[0] = ((len+1) & 0x7f) | 0x80;
        buffer[1] = (len+1) >> 7;
    }
    else
    {
        buffer[0] = len;
    }
    buffer[1+offset] = 0x00; /* Hub ID */
    buffer[2+offset] = TYPE_PORT_OUTPUT;
    buffer[3+offset] = port_id;
    buffer[4+offset] = OUTPUT_STARTUP_IMMEDIATE | OUTPUT_COMPLETE_STATUS;
    buffer[5+offset] = OUTPUT_CMD_WRITE_DIRECT_MODE_DATA;
    buffer[6+offset] = mode;
    memcpy(buffer + 7 + offset, bytes, nbytes);

    if (queue_clear_responses() != 0 || queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return -1;
    }

    /* 'buffer' is now the responsibility of the comms thread */

    if ((response = get_response(TYPE_PORT_OUTPUT, true, port_id)) == NULL)
        return -1;
    if (response[0] != 5 ||
        response[2] != TYPE_PORT_OUTPUT_FEEDBACK ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Output Write Direct Mode Data");
        return -1;
    }
    if ((response[4] & 0x04) != 0)
    {
        /* "Current Command(s) Discarded" bit set */
        PyErr_SetString(hub_protocol_error, "Port busy");
        return -1;
    }

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
