/* hubmodule.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Module to provide Python access to the Lego Hat via I2C
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>
#include <stddef.h>

#include "i2c.h"
#include "cmd.h"


static PyObject *
hub_info(PyObject *self, PyObject *args)
{
    PyObject *dict;
    PyObject *hw_revision;
    PyObject *fw_revision;

    if (!PyArg_ParseTuple(args, "")) /* No args to this function */
        return NULL;

    /* Add the hardware revision to the dictionary */
    hw_revision = cmd_get_hardware_version();
    if (hw_revision == NULL)
        return NULL;

    /* ...and the firmware revision while we're at it */
    fw_revision = cmd_get_firmware_version();
    if (fw_revision == NULL)
    {
        Py_DECREF(hw_revision);
        return NULL;
    }

    dict = Py_BuildValue("{sOsO}",
                         "hardware_revision", hw_revision,
                         "firmware_revision", fw_revision);
    Py_DECREF(hw_revision);
    Py_DECREF(fw_revision);
    return dict;
}


static PyMethodDef hub_methods[] = {
    { "info", hub_info, METH_VARARGS, "Information about the Hub" },
    { NULL, NULL, 0, NULL }
};


static struct PyModuleDef hubmodule = {
    PyModuleDef_HEAD_INIT,
    "hub",
    NULL, /* Documentation */
    -1,   /* module state is in globals */
    hub_methods, /* Methods */
};


PyMODINIT_FUNC
PyInit_hub(void)
{
    PyObject *hub = PyModule_Create(&hubmodule);
    if (hub == NULL)
        return NULL;

    if (cmd_init(hub) < 0)
    {
        Py_DECREF(hub);
        return NULL;
    }

    if (i2c_open_hat() < 0)
    {
        cmd_deinit();
        Py_DECREF(hub);
        return NULL;
    }

    return hub;
}
