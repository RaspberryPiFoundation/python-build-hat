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


PyObject *cmd_get_hardware_version(void)
{
    uint8_t *buffer = malloc(5);
    uint8_t *response;
    PyObject *version;

    if (buffer == NULL)
        return PyErr_NoMemory();

    buffer[0] = 5; /* Length */
    buffer[1] = 0; /* Hub ID (reserved) */
    buffer[2] = TYPE_HUB_PROPERTY;
    buffer[3] = PROP_HW_VERSION;
    buffer[4] = PROP_OP_REQUEST;
    if (queue_add_buffer(buffer) != 0)
    {
        /* Python exception already raised. */
        free(buffer);
        return NULL;
    }
    /* At this point, responsibility for `buffer` has been handed
     * over to the comms thread.
     */

    if (queue_get(&response) != 0)
        /* Python exception already raised */
        return NULL;

    /* Response is dynamically allocated and our responsibility */
    if (response[1] != 0x00)
    {
        free(response);
        PyErr_Format(hub_protocol_error, "Bad hub ID 0x%02x", response[1]);
        return NULL;
    }

    /* Check for an error return */
    if (response[2] == TYPE_GENERIC_ERROR)
    {
        handle_generic_error(TYPE_HUB_PROPERTY, response);
        free(response);
        return NULL;
    }

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
    uint8_t *buffer = malloc(5);
    uint8_t *response;
    PyObject *version;

    if (buffer == NULL)
        return PyErr_NoMemory();

    buffer[0] = 5; /* Length */
    buffer[1] = 0; /* Hub ID (reserved) */
    buffer[2] = TYPE_HUB_PROPERTY;
    buffer[3] = PROP_FW_VERSION;
    buffer[4] = PROP_OP_REQUEST;
    if (queue_add_buffer(buffer) != 0)
    {
        /* Python exception already raised. */
        free(buffer);
        return NULL;
    }
    /* At this point, responsibility for `buffer` has been handed
     * over to the comms thread.
     */

    if (queue_get(&response) != 0)
        /* Python exception already raised */
        return NULL;

    /* Response is dynamically allocated and our responsibility */
    if (response[1] != 0x00)
    {
        free(response);
        PyErr_Format(hub_protocol_error, "Bad hub ID 0x%02x", response[1]);
        return NULL;
    }

    /* Check for an error return */
    if (response[2] == TYPE_GENERIC_ERROR)
    {
        handle_generic_error(TYPE_HUB_PROPERTY, response);
        free(response);
        return NULL;
    }

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


port_modes_t *cmd_get_port_modes(uint8_t port_id)
{
    uint8_t *buffer = malloc(5);
    uint8_t *response;
    port_modes_t *results;

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    buffer[0] = 5; /* Length */
    buffer[1] = 0; /* Hub ID (reserved) */
    buffer[2] = TYPE_PORT_INFO_REQ;
    buffer[3] = port_id;
    buffer[4] = PORT_INFO_MODE;
    if (queue_add_buffer(buffer) != 0)
    {
        /* Python exception already reaised */
        free(buffer);
        return NULL;
    }
    /* "buffer" now belongs to the comms system */

    if (queue_get(&response) != 0)
        /* Python exception already raised */
        return NULL;

    /* "response" is now our responsibility */
    if (response[1] != 0x00)
    {
        PyErr_Format(hub_protocol_error, "Bad hub ID 0x%02x", response[1]);
        free(response);
        return NULL;
    }

    /* Check for an error return */
    if (response[2] == TYPE_GENERIC_ERROR)
    {
        handle_generic_error(TYPE_PORT_INFO_REQ, response);
        free(response);
        return NULL;
    }

    if (response[0] != 11 ||
        response[2] != TYPE_PORT_INFO ||
        response[3] != port_id ||
        response[4] != PORT_INFO_MODE)
    {
        free(response);
        PyErr_SetString(hub_protocol_error,
                        "Unexpected reply to Port Information Request");
        return NULL;
    }

    if ((results = malloc(sizeof(port_modes_t))) == NULL)
    {
        free(response);
        PyErr_NoMemory();
        return NULL;
    }
    results->capabilities = response[5];
    results->count = response[6];
    results->input_mode_mask = response[7] | (response[8] << 8);
    results->output_mode_mask = response[9] | (response[10] << 8);
    free(response);

    return results;
}


int cmd_get_combi_modes(uint8_t port_id, combi_mode_t combi)
{
    uint8_t *buffer = malloc(5);
    uint8_t *response;
    int i;

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    buffer[0] = 5;
    buffer[1] = 0;
    buffer[2] = TYPE_PORT_INFO_REQ;
    buffer[3] = port_id;
    buffer[4] = PORT_INFO_MODE_COMBINATIONS;
    if (queue_add_buffer(buffer) != 0)
    {
        free(buffer);
        return -1;
    }
    /* "buffer" is now the responsibility of comms */

    if (queue_get(&response) != 0)
        return -1;

    /* "response" now belongs to this thread */
    if (response[1] != 0x00)
    {
        PyErr_Format(hub_protocol_error, "Bad hub ID 0x%02x", response[1]);
        free(response);
        return -1;
    }

    if (response[2] == TYPE_GENERIC_ERROR)
    {
        handle_generic_error(TYPE_PORT_INFO_REQ, response);
        free(response);
        return -1;
    }

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
    uint8_t *buffer = malloc(6);
    uint8_t *response;

    if (buffer == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    buffer[0] = 6;
    buffer[1] = 0;
    buffer[2] = TYPE_PORT_MODE_REQ;
    buffer[3] = port_id;
    buffer[4] = mode_id;
    buffer[5] = MODE_INFO_NAME;
    if (queue_add_buffer(buffer) != 0)
    {
        free(buffer);
        return -1;
    }
    /* "buffer" has now been given to the comms thread to manage */

    if (queue_get(&response) != 0)
        return -1;

    /* "response" is now our responsibility */
    if (response[1] != 0x00)
    {
        PyErr_Format(hub_protocol_error, "Bad hub ID 0x%02x", response[1]);
        free(response);
        return -1;
    }

    if (response[2] == TYPE_GENERIC_ERROR)
    {
        handle_generic_error(TYPE_PORT_MODE_REQ, response);
        free(response);
        return -1;
    }

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
