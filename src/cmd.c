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


static uint8_t *get_response(uint8_t type, bool return_feedback)
{
    uint8_t *response;

    while (1)
    {
        if (queue_get(&response) != 0)
        {
            PyErr_SetFromErrno(hub_protocol_error);
            return NULL;
        }
        
        if (response == NULL)
        {
            PyErr_SetString(hub_protocol_error, "Tx timeout");
            return NULL;
        }

        /* `response` is dynamically allocated and now our responsibility */
        if (response[1] != 0x00)
        {
            PyErr_Format(hub_protocol_error, "Bad hub ID 0x%02x", response[1]);
            free(response);
            return NULL;
        }

        /* Check for an error return */
        if (response[2] == TYPE_GENERIC_ERROR)
        {
            handle_generic_error(type, response);
            free(response);
            return NULL;
        }

        /* Ignore Feedback messages unless explicitly asked for them */
        if (return_feedback || response[2] != TYPE_PORT_OUTPUT_FEEDBACK)
        {
            return response;
        }
        free(response);
    }

    /* Unreachable code */
    return NULL;
}


static uint8_t *make_request(bool return_feedback,
                             uint8_t nbytes,
                             uint8_t type, ...)
{
    va_list args;
    uint8_t *buffer = malloc(nbytes);
    int i;

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
        buffer[i] = va_arg(args, int);
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

    return get_response(type, return_feedback);
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


int cmd_get_port_value(uint8_t port_id)
{
    uint8_t *response = make_request(false, 5, TYPE_PORT_INFO_REQ,
                                     port_id,
                                     PORT_INFO_VALUE);

    if (response == NULL)
        return -1;

    if (response[0] < 4 ||
        (response[2] != TYPE_PORT_VALUE_SINGLE &&
         response[2] != TYPE_PORT_VALUE_COMBINED) ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port Info (Value) request");
        return -1;
    }

    /* The values will already have been put in place */
    free(response);
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
            if (buffer[0] == 5 &&
                buffer[2] == TYPE_PORT_OUTPUT_FEEDBACK &&
                buffer[3] == port_id)
            {
                if ((buffer[4] & 0x20) != 0)
                {
                    /* The motor has stalled! */
                    free(buffer);
                    PyErr_SetString(hub_protocol_error, "Motor stalled");
                    return -1;
                }
                if ((buffer[4] & 0x04) != 0)
                {
                    /* "Current Command(s) Discarded" bit set */
                    free(buffer);
                    PyErr_SetString(hub_protocol_error, "Port busy");
                    return -1;
                }
                if ((buffer[4] & 0x02) != 0)
                {
                    /* "Current command(s) Complete" bit set */
                    free(buffer);
                    return 0;
                }
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
    uint8_t *response = make_request(true, 7, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_START_POWER,
                                     (uint8_t)pwm);
    if (response == NULL)
        return -1;

    return wait_for_complete_feedback(port_id, response);
}


int cmd_set_pwm_pair(uint8_t port_id, int8_t pwm0, int8_t pwm1)
{
    uint8_t *response = make_request(true, 8, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_START_POWER_2,
                                     (uint8_t)pwm0,
                                     (uint8_t)pwm1);
    if (response == NULL)
        return -1;

    return wait_for_complete_feedback(port_id, response);
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
    uint8_t *response = make_request(true, 9, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_START_SPEED,
                                     (uint8_t)speed,
                                     max_power,
                                     use_profile);
    if (response == NULL)
        return -1;

    return wait_for_complete_feedback(port_id, response);
}


int cmd_start_speed_pair(uint8_t port_id,
                         int8_t speed0,
                         int8_t speed1,
                         uint8_t max_power,
                         uint8_t use_profile)
{
    uint8_t *response = make_request(/*XXX:*/true, 10, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_START_SPEED_2,
                                     (uint8_t)speed0,
                                     (uint8_t)speed1,
                                     max_power,
                                     use_profile);
    if (response == NULL)
        return -1;

    return wait_for_complete_feedback(port_id, response);
}


int cmd_start_speed_for_time(uint8_t port_id,
                             uint16_t time,
                             int8_t speed,
                             uint8_t max_power,
                             uint8_t stop,
                             uint8_t use_profile,
                             bool blocking)
{
    uint8_t *response = make_request(true, 12, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_START_SPEED_FOR_TIME,
                                     U16_TO_BYTE_ARG(time),
                                     (uint8_t)speed,
                                     max_power,
                                     stop,
                                     use_profile);
    if (response == NULL)
        return -1;
    if (blocking)
        return wait_for_complete_feedback(port_id, response);

    if (response[0] != 5 ||
        response[2] != TYPE_PORT_OUTPUT_FEEDBACK ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Output Start Speed For Time");
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


int cmd_start_speed_for_time_pair(uint8_t port_id,
                                  uint16_t time,
                                  int8_t speed0,
                                  int8_t speed1,
                                  uint8_t max_power,
                                  uint8_t stop,
                                  uint8_t use_profile,
                                  bool blocking)
{
    uint8_t *response = make_request(true, 13, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_START_SPEED_2_FOR_TIME,
                                     U16_TO_BYTE_ARG(time),
                                     (uint8_t)speed0,
                                     (uint8_t)speed1,
                                     max_power,
                                     stop,
                                     use_profile);
    if (response == NULL)
        return -1;
    if (blocking)
        return wait_for_complete_feedback(port_id, response);

    if (response[0] != 5 ||
        response[2] != TYPE_PORT_OUTPUT_FEEDBACK ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Output Start Speed For Time");
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


int cmd_start_speed_for_degrees(uint8_t port_id,
                                int32_t degrees,
                                int8_t speed,
                                uint8_t max_power,
                                uint8_t stop,
                                uint8_t use_profile,
                                bool blocking)
{
    uint8_t *response = make_request(true, 14, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_START_SPEED_FOR_DEGREES,
                                     U32_TO_BYTE_ARG((uint32_t)degrees),
                                     (uint8_t)speed,
                                     max_power,
                                     stop,
                                     use_profile);
    if (response == NULL)
        return -1;
    if (blocking)
        return wait_for_complete_feedback(port_id, response);

    if (response[0] != 5 ||
        response[2] != TYPE_PORT_OUTPUT_FEEDBACK ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Output Start Speed For Degrees");
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


int cmd_start_speed_for_degrees_pair(uint8_t port_id,
                                     int32_t degrees,
                                     int8_t speed0,
                                     int8_t speed1,
                                     uint8_t max_power,
                                     uint8_t stop,
                                     uint8_t use_profile,
                                     bool blocking)
{
    uint8_t *response = make_request(true, 15, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_START_SPEED_2_FOR_DEGREES,
                                     U32_TO_BYTE_ARG((uint32_t)degrees),
                                     (uint8_t)speed0,
                                     (uint8_t)speed1,
                                     max_power,
                                     stop,
                                     use_profile);
    if (response == NULL)
        return -1;
    if (blocking)
        return wait_for_complete_feedback(port_id, response);

    if (response[0] != 5 ||
        response[2] != TYPE_PORT_OUTPUT_FEEDBACK ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Output Start Speed For Degrees");
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


int cmd_goto_abs_position(uint8_t port_id,
                          int32_t position,
                          int8_t speed,
                          uint8_t max_power,
                          uint8_t stop,
                          uint8_t use_profile,
                          bool blocking)
{
    uint8_t *response = make_request(true, 14, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_GOTO_ABS_POSITION,
                                     U32_TO_BYTE_ARG((uint32_t)position),
                                     (uint8_t)speed,
                                     max_power,
                                     stop,
                                     use_profile);
    if (response == NULL)
        return -1;
    if (blocking)
        return wait_for_complete_feedback(port_id, response);

    if (response[0] != 5 ||
        response[2] != TYPE_PORT_OUTPUT_FEEDBACK ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Output Goto Abs Position");
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


int cmd_goto_abs_position_pair(uint8_t port_id,
                               int32_t position0,
                               int32_t position1,
                               int8_t speed,
                               uint8_t max_power,
                               uint8_t stop,
                               uint8_t use_profile,
                               bool blocking)
{
    uint8_t *response = make_request(true, 18, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_GOTO_ABS_POSITION_2,
                                     U32_TO_BYTE_ARG((uint32_t)position0),
                                     U32_TO_BYTE_ARG((uint32_t)position1),
                                     (uint8_t)speed,
                                     max_power,
                                     stop,
                                     use_profile);
    if (response == NULL)
        return -1;
    if (blocking)
        return wait_for_complete_feedback(port_id, response);

    if (response[0] != 5 ||
        response[2] != TYPE_PORT_OUTPUT_FEEDBACK ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Output Goto Abs Position");
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


int cmd_preset_encoder_pair(uint8_t port_id,
                            int32_t position0,
                            int32_t position1)
{
    uint8_t *response = make_request(true, 14, TYPE_PORT_OUTPUT,
                                     port_id,
                                     OUTPUT_STARTUP_IMMEDIATE |
                                     OUTPUT_COMPLETE_STATUS,
                                     OUTPUT_CMD_PRESET_ENCODER_2,
                                     U32_TO_BYTE_ARG((uint32_t)position0),
                                     U32_TO_BYTE_ARG((uint32_t)position1));
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

    if ((response = get_response(TYPE_PORT_OUTPUT, true)) == NULL)
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
    uint8_t *response;

    /* Mode zero appears to be a legacy */
    response = make_request(false, 10, TYPE_PORT_FORMAT_SETUP_SINGLE,
                            port_id,
                            mode,
                            1, 0, 0, 0, /* Delta = 1 */
                            notifications);
    if (response == NULL)
        return -1;

    if (response[0] != 10 ||
        response[2] != TYPE_PORT_FORMAT_SINGLE ||
        response[3] != port_id ||
        response[4] != mode ||
        response[5] != 1 ||
        response[6] != 0 ||
        response[7] != 0 ||
        response[8] != 0 ||
        response[9] != notifications)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port Format Setup");
        return -1;
    }

    free(response);
    if (mode == 0)
        return 0;

    /* Non-legacy modes go through an unexplained dance to change
     * mode.  First the mode is set as for mode 0 above, then
     * issue a device reset, then set the mode again.
     */
    response = make_request(false, 5, TYPE_PORT_FORMAT_SETUP_COMBINED,
                            port_id,
                            INFO_FORMAT_RESET);
    if (response == NULL)
        return -1;

    /* The documentation is unclear as to what response to expect.
     * The code shows a TYPE_PORT_FORMAT_SINGLE for the current mode.
     */
    if (response[0] != 10 ||
        response[2] != TYPE_PORT_FORMAT_SINGLE ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port CombiFormat (Reset)");
        return -1;
    }

    /* Now set the mode again */
    response = make_request(false, 10, TYPE_PORT_FORMAT_SETUP_SINGLE,
                            port_id,
                            mode,
                            1, 0, 0, 0, /* Delta = 1 */
                            notifications); 
    if (response == NULL)
        return -1;

    if (response[0] != 10 ||
        response[2] != TYPE_PORT_FORMAT_SINGLE ||
        response[3] != port_id ||
        response[4] != mode ||
        response[5] != 1 ||
        response[6] != 0 ||
        response[7] != 0 ||
        response[8] != 0 ||
        response[9] != notifications)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port Format Setup");
        return -1;
    }

    free(response);
    return 0;
}


int cmd_set_combi_mode(uint8_t port_id,
                       int combi_index,
                       uint8_t *modes,
                       int num_modes,
                       uint8_t notifications)
{
    uint8_t *response;
    uint8_t *buffer;
    int i;
    uint16_t combi_map;

    /* First reset the device's mode */
    response = make_request(false, 5, TYPE_PORT_FORMAT_SETUP_COMBINED,
                            port_id,
                            INFO_FORMAT_RESET);
    if (response == NULL)
        return -1;

    /* See above */
    if (response[0] != 10 ||
        response[2] != TYPE_PORT_FORMAT_SINGLE ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port Format Combi Setup Reset");
        return -1;
    }

    free(response);

    /* Now lock the device against actual mode changes */
    response = make_request(false, 5, TYPE_PORT_FORMAT_SETUP_COMBINED,
                            port_id,
                            INFO_FORMAT_LOCK);
    if (response == NULL)
        return -1;

    /* This also gets a PORT_FORMAT_SETUP_SINGLE in response */
    if (response[0] != 10 ||
        response[2] != TYPE_PORT_FORMAT_SINGLE ||
        response[3] != port_id)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port Format Combi Setup Lock");
        return -1;
    }

    free(response);

    /* For each mode, do a Format Single Setup on it */
    for (i = 0; i < num_modes; i++)
    {
        /* We do not expect a response, so we can't use make_request() */
        if ((buffer = malloc(10)) == NULL)
        {
            PyErr_NoMemory();
            return -1;
        }

        buffer[0] = 10;
        buffer[1] = 0x00; /* Hub ID */
        buffer[2] = TYPE_PORT_FORMAT_SETUP_SINGLE;
        buffer[3] = port_id;
        buffer[4] = modes[i] >> 4;
        buffer[5] = 0; /* Delta = 0 ? */
        buffer[6] = 0;
        buffer[7] = 0;
        buffer[8] = 0;
        buffer[9] = notifications;

        if (queue_add_buffer(buffer) != 0)
        {
            /* Exception already raised */
            free(buffer);
            /* Do an emergency reset rather than just leave this unsafe */
            goto emergency_reset;
        }
    }

    /* Now we should get the responses */
    for (i = 0; i < num_modes; i++)
    {
        if (queue_get(&response) != 0)
            goto emergency_reset;
        if (response == NULL)
        {
            PyErr_SetString(hub_protocol_error, "Rx timeout");
            goto emergency_reset;
        }
        if (response[0] != 10 ||
            response[2] != TYPE_PORT_FORMAT_SINGLE ||
            response[3] != port_id ||
            response[4] != modes[i] >> 4 ||
            response[5] != 0 ||
            response[6] != 0 ||
            response[7] != 0 ||
            response[8] != 0 ||
            response[9] != notifications)
        {
            free(response);
            PyErr_Format(hub_protocol_error,
                         "Unexpected reply formatting mode %d",
                         modes[i] >> 4);

            /* Do an emergency reset rather than just leave this unsafe */
            goto emergency_reset;
        }
        free(response);
    }

    /* Now set the combination.  This one we can't make_request() */
    /* The documentation claims this is 7 bytes long, then promptly
     * declares a variable length structure.  Sigh.  The real answer
     * is that we need 6 bytes plus one per mode/dataset combination.
     */
    if ((buffer = malloc(6 + num_modes)) == NULL)
    {
        PyErr_NoMemory();
        /* Again, reset rather than just give up. */
        goto emergency_reset;
    }

    buffer[0] = 6 + num_modes;
    buffer[1] = 0x00;
    buffer[2] = TYPE_PORT_FORMAT_SETUP_COMBINED;
    buffer[3] = port_id;
    buffer[4] = INFO_FORMAT_SET;
    buffer[5] = combi_index;
    memcpy(buffer+6, modes, num_modes);

    if (queue_clear_responses() != 0 || queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        goto emergency_reset;
    }

    /* No response is expected */

    /* Unlock and restart the device: this does get the COMBI response */
    response = make_request(false, 5, TYPE_PORT_FORMAT_SETUP_COMBINED,
                            port_id,
                            INFO_FORMAT_UNLOCK_AND_START_MULTI_UPDATE_DISABLED);
    if (response == NULL)
        return -1;

    combi_map = (1 << num_modes) - 1;
    if (response[0] != 7 ||
        response[2] != TYPE_PORT_FORMAT_COMBINED ||
        response[3] != port_id ||
/* The colour sensor returns a bad value for combi_index,
 * so don't check it.
 */
//        response[4] != combi_index ||
        response[5] != (combi_map & 0xff) ||
        response[6] != ((combi_map >> 8) & 0xff))
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port Format Combi Setup Start");
        return -1;
    }

    free(response);

    return 0;


emergency_reset:
    /* Try to put the device back in a known state */
    response = make_request(false, 5, TYPE_PORT_FORMAT_SETUP_COMBINED,
                            port_id,
                            INFO_FORMAT_RESET);
    free(response);
    return -1;
}

/* For a virtual port connection, we do not expect a reply as such.
 * We should get a Hub Attached IO for the new virtual port, and we
 * give ourselves permission to use the MotorPair from that.  This
 * requires careful interlocking...
 */
int cmd_connect_virtual_port(uint8_t port_1_id, uint8_t port_2_id)
{
    uint8_t *buffer = malloc(6);

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    buffer[0] = 6;
    buffer[1] = 0x00; /* Hub ID */
    buffer[2] = TYPE_VIRTUAL_PORT_SETUP;
    buffer[3] = 1; /* Connect */
    buffer[4] = port_1_id;
    buffer[5] = port_2_id;

    if (queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return -1;
    }

    return 0;
}


/* On this occasion we expect a Hub Attached I/O to tell us the
 * virtual port is now detached.
 */
int cmd_disconnect_virtual_port(uint8_t port_id)
{
    uint8_t *buffer = malloc(5);

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    buffer[0] = 5;
    buffer[1] = 0x00; /* Hub ID */
    buffer[2] = TYPE_VIRTUAL_PORT_SETUP;
    buffer[3] = 0; /* Disconnect */
    buffer[4] = port_id;

    if (queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return -1;
    }

    return 0;
}


/* Send an initialization command to the firmware upgrade system */
int cmd_firmware_init(uint32_t nbytes)
{
    /* For now, send the packet and don't expect the reply (for timing) */
    uint8_t *buffer = malloc(8);

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    buffer[0] = 8;
    buffer[1] = 0x00; /* Hub ID */
    buffer[2] = TYPE_FIRMWARE_REQUEST;
    buffer[3] = FIRMWARE_INITIALIZE;
    buffer[4] = nbytes & 0xff;
    buffer[5] = (nbytes >> 8) & 0xff;
    buffer[6] = (nbytes >> 16) & 0xff;
    buffer[7] = (nbytes >> 24) & 0xff;

    if (queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return -1;
    }

    return 0;
}


/* Send data to write to the firmware upgrade area */
int cmd_firmware_store(const uint8_t *data, uint32_t nbytes)
{
    /* We have to allocate the packet ourself since it is variable length */
    uint32_t data_len = nbytes + 4;
    uint8_t *buffer;
    uint8_t *response;
    uint32_t bytes_written;
    int index = 0;

    if (data_len > 0x7f)
        data_len++; /* The data will be one byte longer than we thought */
    if ((buffer = malloc(data_len)) == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    if (data_len > 0x7f)
    {
        buffer[index++] = (data_len & 0x7f) | 0x80;
        buffer[index++] = data_len >> 7;
    }
    else
    {
        buffer[index++] = nbytes+4;
    }
    buffer[index++] = 0x00; /* Hub ID */
    buffer[index++] = TYPE_FIRMWARE_REQUEST;
    buffer[index++] = FIRMWARE_STORE;
    memcpy(buffer+index, data, nbytes);

    if (queue_clear_responses() != 0 || queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return -1;
    }

    /* 'buffer' is now the responsibility of the comms thread */

    if ((response = get_response(TYPE_FIRMWARE_REQUEST, false)) == NULL)
        return -1;
    if (response[0] != 9 ||
        response[2] != TYPE_FIRMWARE_RESPONSE ||
        response[3] != FIRMWARE_STORE)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Firmware Request (Store)");
        return -1;
    }
    if (response[4] == 0)
    {
        free(response);
        PyErr_SetString(hub_protocol_error, "Firmware Store failed");
        return -1;
    }
    bytes_written =
        response[5] |
        (response[6] << 8) |
        (response[7] << 16) |
        (response[8] << 24);

    free(response);
    return (int)bytes_written;
}


int cmd_firmware_length(void)
{
    uint8_t *response = make_request(false, 4, TYPE_FIRMWARE_REQUEST,
                                     FIRMWARE_READLENGTH);
    uint32_t nbytes;

    if (response == NULL)
        return -1;

    if (response[0] != 8 ||
        response[2] != TYPE_FIRMWARE_RESPONSE ||
        response[3] != FIRMWARE_READLENGTH)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Firmware Request");
        return -1;
    }

    nbytes = response[4] |
        (response[5] << 8) |
        (response[6] << 16) |
        (response[7] << 24);

    free(response);
    return (int)nbytes;
}


int cmd_firmware_checksum(uint8_t request_type, uint32_t *pchecksum)
{
    uint8_t *response = make_request(false, 5, TYPE_FIRMWARE_REQUEST,
                                     FIRMWARE_CHECKSUM, request_type);

    if (response == NULL)
        return -1;

    if (response[0] != 8 ||
        response[2] != TYPE_FIRMWARE_RESPONSE ||
        response[3] != FIRMWARE_CHECKSUM)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Firmware Request");
        return -1;
    }

    *pchecksum = response[4] |
        (response[5] << 8) |
        (response[6] << 16) |
        (response[7] << 24);

    free(response);
    return 0;
}


int cmd_firmware_validate_image(int *pvalid,
                                uint32_t *pstored_checksum,
                                uint32_t *pcalc_checksum)
{
    uint8_t *response = make_request(false, 4, TYPE_FIRMWARE_REQUEST,
                                     FIRMWARE_VALIDATE);

    if (response == NULL)
        return -1;

    if (response[0] != 13 ||
        response[2] != TYPE_FIRMWARE_RESPONSE ||
        response[3] != FIRMWARE_VALIDATE)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Firmware Request");
        return -1;
    }

    *pvalid = (int8_t)response[4];
    if (response[4] == 0xff)
    {
        *pstored_checksum = 0;
        *pcalc_checksum = 0;
    }
    else
    {
        *pstored_checksum = response[5] |
            (response[6] << 8) |
            (response[7] << 16) |
            (response[8] << 24);
        *pcalc_checksum = response[9] |
            (response[10] << 8) |
            (response[11] << 16) |
            (response[12] << 24);
    }

    free(response);
    return 0;
}


int cmd_firmware_get_flash_devid(uint32_t *pdev_id)
{
    uint8_t *response = make_request(false, 4, TYPE_FIRMWARE_REQUEST,
                                     FIRMWARE_FLASH_DEVID);

    if (response == NULL)
        return -1;

    if (response[0] != 8 ||
        response[2] != TYPE_FIRMWARE_RESPONSE ||
        response[3] != FIRMWARE_FLASH_DEVID)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Firmware Request");
        return -1;
    }

    *pdev_id = response[4] |
        (response[5] << 8) |
        (response[6] << 16) |
        (response[7] << 24);

    free(response);
    return 0;
}


int cmd_firmware_read_flash(uint32_t addr, uint8_t *buffer)
{
    uint8_t *response = make_request(false, 8, TYPE_FIRMWARE_REQUEST,
                                     FIRMWARE_READ_FLASH,
                                     U32_TO_BYTE_ARG(addr));

    if (response == NULL)
        return -1;

    if (response[0] != 20 ||
        response[2] != TYPE_FIRMWARE_RESPONSE ||
        response[3] != FIRMWARE_READ_FLASH)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Firmware Request");
        return -1;
    }

    memcpy(buffer, response+4, 16);
    free(response);
    return 0;
}


int cmd_set_vcc_port(int state)
{
    /* We don't expect a response */
    uint8_t *buffer = malloc(4);

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    buffer[0] = 4;
    buffer[1] = 0x00; /* Hub ID */
    buffer[2] = TYPE_HUB_ACTION;
    buffer[3] = state ? ACTION_VCC_PORT_CONTROL_ON :
        ACTION_VCC_PORT_CONTROL_OFF;

    if (queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return -1;
    }

    return 0;
}


int cmd_enable_alert(uint8_t alert)
{
    uint8_t *buffer = malloc(5);

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    buffer[0] = 5;
    buffer[1] = 0x00; /* Hub ID */
    buffer[2] = TYPE_HUB_ALERT;
    buffer[3] = alert;
    buffer[4] = ALERT_OP_ENABLE;

    if (queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return -1;
    }

    return 0;
}


int cmd_disable_alert(uint8_t alert)
{
    uint8_t *buffer = malloc(5);

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    buffer[0] = 5;
    buffer[1] = 0x00; /* Hub ID */
    buffer[2] = TYPE_HUB_ALERT;
    buffer[3] = alert;
    buffer[4] = ALERT_OP_DISABLE;

    if (queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return -1;
    }

    return 0;
}


int cmd_request_alert(uint8_t alert)
{
    uint8_t *response = make_request(false, 5, TYPE_HUB_ALERT,
                                     alert,
                                     ALERT_OP_REQUEST);
    uint32_t result;

    if (response == NULL)
        return -1;

    if (response[0] != 6 ||
        response[2] != TYPE_HUB_ALERT ||
        response[3] != alert ||
        response[4] != ALERT_OP_UPDATE)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Alert Request");
        return -1;
    }

    result = response[5];
    free(response);
    return result;
}
