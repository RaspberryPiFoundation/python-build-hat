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

    version = PyUnicode_FromFormat("%d.%d.%d.%d",
                                   (response[8] >> 4) & 7,
                                   response[8] & 0x0f,
                                   bcd_byte(response[7]),
                                   bcd_2byte(response[6], response[5]));
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

    version = PyUnicode_FromFormat("%d.%d.%d.%d",
                                   (response[8] >> 4) & 7,
                                   response[8] & 0x0f,
                                   bcd_byte(response[7]),
                                   bcd_2byte(response[6], response[5]));
    free(response);

    return version;
}
