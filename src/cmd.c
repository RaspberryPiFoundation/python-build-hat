/* cmd.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * The command interface to the Hat
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>

#include "queue.h"
#include "cmd.h"
#include "protocol.h"


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


int cmd_init(PyObject *hub)
{
    hub_protocol_error = PyErr_NewException("hub.HubProtocolError", NULL, NULL);
    Py_XINCREF(hub_protocol_error);
    if (PyModule_AddObject(hub, "HubProtocolError", hub_protocol_error) < 0)
    {
        Py_XDECREF(hub_protocol_error);
        Py_CLEAR(hub_protocol_error);
        return -1;
    }

    return 0;
}


void cmd_deinit(void)
{
    if (hub_protocol_error == NULL)
        return;
    Py_DECREF(hub_protocol_error);
    Py_CLEAR(hub_protocol_error);
    hub_protocol_error = NULL;
}


PyObject *cmd_version_as_unicode(uint8_t *buffer)
{
    return PyUnicode_FromFormat("%d.%d.%d.%d",
                                (buffer[3] >> 4) & 7,
                                buffer[3] & 0x0f,
                                bcd_byte(buffer[2]),
                                bcd_2byte(buffer[1], buffer[0]));
}


static uint8_t *make_request(uint8_t nbytes, uint8_t type, ...)
{
    va_list args;
    uint8_t *buffer = malloc(nbytes);
    int i;
    uint8_t *response;

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

    if (queue_add_buffer(buffer) != 0)
    {
        /* Exception already raised */
        free(buffer);
        return NULL;
    }

    /* `buffer` now belongs to the comms thread, which will free it when
     * it is done with it.
     */

    if (queue_get(&response) != 0)
        return NULL; /* Python exception already raised */

    /* `response` is dynamically allocated and now our responsibility */
    if (response[1] != 0x00)
    {
        PyErr_Format(hub_protocol_error, "Buad hub ID 0x%02x", response[1]);
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

    return response;
}


PyObject *cmd_get_hardware_version(void)
{
    PyObject *version;
    uint8_t *response = make_request(5, TYPE_HUB_PROPERTY,
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
    uint8_t *response = make_request(5, TYPE_HUB_PROPERTY,
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


int cmd_get_port_modes(uint8_t port_id, port_modes_t *results)
{
    uint8_t *response = make_request(5, TYPE_PORT_INFO_REQ,
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
    uint8_t *response = make_request(5, TYPE_PORT_INFO_REQ,
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
    uint8_t *response = make_request(6, TYPE_PORT_MODE_REQ,
                                     port_id,
                                     mode_id,
                                     MODE_INFO_NAME);

    if (response == NULL)
        return -1;

    /* The length of this packet is variable, but should be between
     * 7 and 17 bytes.
     */
    if (response[0] < 7 || response[0] > 17 ||
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

    memcpy(name, response+6, response[0]-6);
    name[response[0]-6] = '\0';

    return 0;
}


static int get_mode_min_max(uint8_t port_id,
                            uint8_t mode_id,
                            float *pmin,
                            float *pmax,
                            uint8_t info_type,
                            const char *info_name)
{
    uint8_t *response = make_request(6, TYPE_PORT_MODE_REQ,
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
                     "Unexpected reposy to Mode %s Request");
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
    uint8_t *response = make_request(6, TYPE_PORT_MODE_REQ,
                                     port_id,
                                     mode_id,
                                     MODE_INFO_SYMBOL);

    if (response == NULL)
        return -1;

    /* The length of this packet is variable, but should be between
     * 7 and 11 bytes.
     */
    if (response[0] < 7 || response[0] > 11 ||
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

    memcpy(symbol, response+6, response[0]-6);
    symbol[response[0]-6] = '\0';

    return 0;
}


int cmd_get_mode_mapping(uint8_t port_id,
                         uint8_t mode_id,
                         uint8_t *pinput_mapping,
                         uint8_t *poutput_mapping)
{
    uint8_t *response = make_request(6, TYPE_PORT_MODE_REQ,
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
    uint8_t *response = make_request(6, TYPE_PORT_MODE_REQ,
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
    uint8_t *response = make_request(6, TYPE_PORT_MODE_REQ,
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
