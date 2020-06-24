/* firmware.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Python class for handling firmware upgrade
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdint.h>

#include "firmware.h"
#include "cmd.h"
#include "i2c.h"
#include "callback.h"

/**

.. py:class:: Firmware

    Python class to handle firmware upgrades

    .. note::

        This class is not directly available to the user.  It is only
        used to create the ``hub.hub.firmware`` instance.

    A firmware update will follow these steps:

    1.  Use :py:meth:`firmware.appl_image_initialize()` to clear the
        buffer in external flash and set the size of the binary image
        to be transferred.
    #.  Use :py:meth:`firmware.appl_image_store()` to upload the image
        as one or more byte arrays (data) one after the other.
    #.  **Restart the hub and wait for the bootloader to update the
        internal flash.  When the update has been done the hub will
        start the new application.**

        a.  The bootloader will detect that a new image has been
            stored.
        #.  The new image will be validated.  The stored size will be
            used to calculate the CRC32 checksum.  This checksum will
            be compared to the CRC32 checksum stored in the image.
        #.  Assuming the image is valid, the internal flash will be
            erased.
        #.  The new image will be copied to the internal flash.
        #.  The CRC32 checksum of the new image in internal flash is
            calculated and compared to the one stored in the external
            flash as well as the one in the image.
        #.  If the image in internal flash is valid, the image in
            external flash is invalidated.
        #.  The bootloader will start the new application.

    .. py:method:: info() -> dict

        Returns a dict with the firmware subsystem information, much
        of which is calculated during the function call.  Avoid
        calling this function every time you need a specific item.
        It's better to use the same dictionary object and reference
        items as needed unless you specifically need to refresh the
        information.

        The following keys are present:

        * ``appl_checksum`` : the CRC32 checksum of the application in
          internal flash, as stored in internal flash.
        * ``new_appl_image_stored_checksum`` : the CRC32 checksum of
          the new application in external flash, as stored in external
          flash.
        * ``appl_calc_checksum`` : the CRC32 checksum of the
          application in internal flash, as calculated on the fly.
        * ``new_appl_valid`` : the (boolean) validity of the
          application in external flash.  ``True`` if valid, ``False``
          if not.
        * ``new_appl_image_calc_checksum`` : the CRC32 checksum  of
          the new application in external flash, as calculated on the
          fly.
        * ``new_image_size`` : the size of the new firmware image in
          bytes, as declared in the call to
          :py:meth:`.appl_image_initialise()`.  This is not the
          actual uploaded byte count.
        * ``currently_stored_bytes`` : the number of bytes of image
          that have been uploaded via :py:meth:`.appl_image_store()`.
        * ``upload_finished`` : ``True`` when
          ``currently_stored_bytes`` is equal to ``new_image_size``.
        * ``spi_flash_size`` : The size in bytes of the external
          flash, given as a string.  Will be one of '32 MBytes', '16
          MBytes', '8 MBytes', '4 MBytes' or 'unknown'.
        * ``valid`` : the validity of the firmware image in external
          flash.  Will be ``1`` if the image is valid, ``0`` if there
          is no image present, or ``-1`` if the image is invalid.

    .. py:method:: appl_checksum()

        :return: The CRC32 checksum of the application in internal
            flash.
        :rtype: int (unsigned 32-bit value)

    .. py:method:: appl_image_initialize(size)

        Initialises the firmware upgrade system, erasing the external
        flash and setting the expected size of the firmware image.

        :param int size: the number of bytes of the new image that
            will be eventually written to external flash.

        This method will return immediately, but the erase operation
        will take several seconds.  Further ``Firmware`` methods will
        raise a ``hub.hub.HubProtocolError`` if the erase operation is
        still in progress.  When the erase operation completes, it
        will generate a firmware callback (see
        :py:meth:`Firmware.callback()` below).

    .. py:method:: appl_image_store(data)

        Stores the new image data supplied in external flash.  Can be
        called successively until the full image is stored.  Before
        this method is used, :py:meth:`appl_image_initialize()` must
        be called.

        :param data: the data to send
        :type data: bytes-like object

    .. py:method:: ext_flash_read_length()

        :return: the number of bytes that have been written to
            external flash.
        :rtype: int

    .. py:method:: callback([fn])

        Gets or sets the function to be called when long (background)
        firmware operation completes.

        :param fn: The function to call.  This is a positional
            parameter only.
        :type fn: Callable or None
        :raises TypeError: if ``fn`` is present, not ``None`` and not
            callable.
        :return: the current callback function if ``fn`` is omitted,
            otherwise ``None``.
        :rtype: Callable or None

        ``fn``, if present, should be a function taking two
        parameters.  The first parameter is the reason for the
        background activity.  Currently this is always ``1``,
        indicating an erase operation caused by
        :py:meth:`appl_image_initialize()`.

        The second parameter is a status code.  It will be ``1`` if
        the background activity completed successfully, or ``0`` if an
        error occurred.

    .. py:method:: flash_read(addr)

        Reads 16 bytes from internal or external flash memory.

        :param int addr: The address to start reading from.  Internal
        flash addresses run from 0x00000000, mapped into flash to
        exclude the bootloader.  External addresses from from
        0x1000000.
        :return: the 16 bytes of flash starting at the given address.
        :rtype: bytes

        This function is intended for debugging only, so its error
        handling is not very comprehensive.

    .. py:method:: reboot()

        Reboots the hat to allow a firmware upgrade to complete.  The
        bootloader will validate any image in external flash, and if
        valid copy it into internal flash and run it.

        Note that this method is not intended as a general purpose
        reset mechanism.  It does not wait for the Hat to finish
        resetting, so any subsequent attempts to communicate with the
        hat will likely time out.

*/


/* Status values for the firmware object */
#define FW_STATUS_IDLE    0
#define FW_STATUS_ERASING 1

#define FW_STATUS_MAX     (FW_STATUS_ERASING+1)


/* The actual Firmware type */
typedef struct
{
    PyObject_HEAD
    volatile uint8_t status;
    uint32_t image_bytes;
    PyObject *callback;
} FirmwareObject;

const char *status_messages[FW_STATUS_MAX] = {
    NULL,
    "Erase in progress",
};


static int
Firmware_traverse(FirmwareObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->callback);
    return 0;
}


static int
Firmware_clear(FirmwareObject *self)
{
    Py_CLEAR(self->callback);
    return 0;
}


static void
Firmware_dealloc(FirmwareObject *self)
{
    PyObject_GC_UnTrack(self);
    Firmware_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *
Firmware_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    FirmwareObject *self = (FirmwareObject *)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        self->status = FW_STATUS_IDLE;
        self->image_bytes = 0;
        i2c_register_firmware_object((PyObject *)self);
        self->callback = Py_None;
        Py_INCREF(Py_None);
    }
    return (PyObject *)self;
}


static int
check_fw_status(FirmwareObject *firmware)
{
    uint8_t status = firmware->status;

    if (status != FW_STATUS_IDLE)
    {
        if (status >= FW_STATUS_MAX)
        {
            PyErr_Format(cmd_get_exception(),
                         "Unexpected firmware state %d",
                         status);
        }
        else
        {
            PyErr_SetString(cmd_get_exception(), status_messages[status]);
        }
        return 0;
    }
    return 1;
}


#define FLASH_SIZE_UNKNOWN  0
#define FLASH_SIZE_4MB      1
#define FLASH_SIZE_8MB      2
#define FLASH_SIZE_16MB     3
#define FLASH_SIZE_32MB     4

#define FLASH_SIZE_COUNT 5

const char *flash_sizes[FLASH_SIZE_COUNT] = {
    "unknown",
    "4 MBytes",
    "8 MBytes",
    "16 MBytes",
    "32 MBytes",
};

static PyObject *
Firmware_info(PyObject *self, PyObject *args)
{
    FirmwareObject *firmware = (FirmwareObject *)self;
    PyObject *dict;
    uint32_t appl_checksum;
    uint32_t new_appl_image_stored_checksum;
    uint32_t new_appl_image_calc_checksum;
    uint32_t appl_calc_checksum;
    uint32_t flash_device_id;
    int valid;
    int currently_stored_bytes;
    const char *flash_size;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    if (!check_fw_status(firmware))
        return NULL;

    if (cmd_firmware_checksum(FW_CHECKSUM_STORED, &appl_checksum) < 0)
        return NULL;
    if (cmd_firmware_checksum(FW_CHECKSUM_CALC, &appl_calc_checksum) < 0)
        return NULL;
    if (cmd_firmware_validate_image(&valid,
                                    &new_appl_image_stored_checksum,
                                    &new_appl_image_calc_checksum) < 0)
        return NULL;
    if ((currently_stored_bytes = cmd_firmware_length()) < 0)
        return NULL;
    if (cmd_firmware_get_flash_devid(&flash_device_id) < 0)
        return NULL;

    switch (flash_device_id)
    {
        case 0x1640EF:
            flash_size = flash_sizes[FLASH_SIZE_4MB];
            break;

        case 0x1740EF:
            flash_size = flash_sizes[FLASH_SIZE_8MB];
            break;

        case 0x1840EF:
            flash_size = flash_sizes[FLASH_SIZE_16MB];
            break;

        case 0x1940EF:
            flash_size = flash_sizes[FLASH_SIZE_32MB];
            break;

        default:
            flash_size = flash_sizes[FLASH_SIZE_UNKNOWN];
    }

    dict = Py_BuildValue("{sk sk sk sN sk sk si sN ss si}",
                         "appl_checksum", appl_checksum,
                         "new_appl_image_stored_checksum",
                         new_appl_image_stored_checksum,
                         "appl_calc_checksum", appl_calc_checksum,
                         "new_appl_valid", PyBool_FromLong(valid == 1),
                         "new_appl_image_calc_checksum",
                         new_appl_image_calc_checksum,
                         "new_image_size", firmware->image_bytes,
                         "currently_stored_bytes",
                         currently_stored_bytes,
                         "upload_finished",
                         PyBool_FromLong(currently_stored_bytes ==
                                         firmware->image_bytes),
                         "spi_flash_size", flash_size,
                         "valid", valid);
    return dict;
}


static PyObject *
Firmware_appl_image_initialize(PyObject *self, PyObject *args)
{
    FirmwareObject *firmware = (FirmwareObject *)self;
    uint32_t nbytes;

    if (!PyArg_ParseTuple(args, "k:nbytes", &nbytes))
        return NULL;
    if (!check_fw_status(firmware))
        return NULL;

    firmware->status = FW_STATUS_ERASING;
    if (cmd_firmware_init(nbytes) < 0)
    {
        firmware->status = FW_STATUS_IDLE;
        return NULL;
    }
    firmware->image_bytes = nbytes;

    Py_RETURN_NONE;
}


/* We go to some lengths to support the extended two-byte length format for
 * packet lengths of 128 bytes or more.  Sadly the firmware's main command
 * processor only supports single byte lengths.
 */
#define CHUNK_BYTES 64
static PyObject *
Firmware_appl_image_store(PyObject *self, PyObject *args)
{
    FirmwareObject *firmware = (FirmwareObject *)self;
    const char *buffer;
    int nbytes;

    if (!PyArg_ParseTuple(args, "y#", &buffer, &nbytes))
        return NULL;
    if (!check_fw_status(firmware))
        return NULL;

    if (nbytes == 0)
        Py_RETURN_NONE;

    /* Writing 64 bytes doesn't take that long, so we may block */
    /* ...as long as we only write 64 bytes at a time */
    while (nbytes > CHUNK_BYTES)
    {
        if (cmd_firmware_store((const uint8_t *)buffer, CHUNK_BYTES) < 0)
            return NULL;
        nbytes -= CHUNK_BYTES;
        buffer += CHUNK_BYTES;
    }
    if (nbytes > 0)
        if (cmd_firmware_store((const uint8_t *)buffer, nbytes) < 0)
            return NULL;

    Py_RETURN_NONE;
}
#undef CHUNK_BYTES


static PyObject *
Firmware_ext_flash_read_length(PyObject *self, PyObject *args)
{
    FirmwareObject *firmware = (FirmwareObject *)self;
    int rv;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    if (!check_fw_status(firmware))
        return NULL;

    if ((rv = cmd_firmware_length()) < 0)
        return NULL;
    return PyLong_FromLong(rv);
}


static PyObject *
Firmware_appl_checksum(PyObject *self, PyObject *args)
{
    FirmwareObject *firmware = (FirmwareObject *)self;
    uint32_t checksum;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    if (!check_fw_status(firmware))
        return NULL;

    if (cmd_firmware_checksum(FW_CHECKSUM_STORED, &checksum) < 0)
        return NULL;
    return PyLong_FromUnsignedLong(checksum);
}


static PyObject *
Firmware_callback(PyObject *self, PyObject *args)
{
    FirmwareObject *firmware = (FirmwareObject *)self;
    PyObject *callable = NULL;

    if (!PyArg_ParseTuple(args, "|O:callback", &callable))
        return NULL;
    if (callable == NULL)
    {
        /* Return the current callback */
        Py_INCREF(firmware->callback);
        return firmware->callback;
    }
    if (!check_fw_status(firmware))
        return NULL;

    /* Otherwise we are setting a new callback (or unsetting it with
     * None).  Ensure that we have something legitimate.
     */
    if (callable != Py_None && !PyCallable_Check(callable))
    {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    Py_INCREF(callable);
    Py_XDECREF(firmware->callback);
    firmware->callback = callable;

    Py_RETURN_NONE;
}


static PyObject *
Firmware_read_flash(PyObject *self, PyObject *args)
{
    FirmwareObject *firmware = (FirmwareObject *)self;
    uint32_t addr;
    uint8_t buffer[16];

    if (!PyArg_ParseTuple(args, "k:addr", &addr))
        return NULL;
    if (!check_fw_status(firmware))
        return NULL;

    if (cmd_firmware_read_flash(addr, buffer) < 0)
        return NULL;
    return Py_BuildValue("y#", buffer, 16);
}


static PyObject *
Firmware_reboot(PyObject *self, PyObject *args)
{
    FirmwareObject *firmware = (FirmwareObject *)self;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    if (!check_fw_status(firmware))
        return NULL;

    if (cmd_action_reset() < 0)
        return NULL;
    Py_RETURN_NONE;
}


static PyMethodDef Firmware_methods[] = {
    {
        "info",
        Firmware_info,
        METH_VARARGS,
        "Information about the firmware"
    },
    {
        "appl_image_initialize",
        Firmware_appl_image_initialize,
        METH_VARARGS,
        "Initialize HAT to receive a new firmware image"
    },
    {
        "appl_image_store",
        Firmware_appl_image_store,
        METH_VARARGS,
        "Store bytes to the new firmware image in external flash"
    },
    {
        "appl_checksum",
        Firmware_appl_checksum,
        METH_VARARGS,
        "Retrieve the checksum of the running application"
    },
    {
        "ext_flash_read_length",
        Firmware_ext_flash_read_length,
        METH_VARARGS,
        "Read the number of bytes (so far) written to external flash"
    },
    {
        "callback",
        Firmware_callback,
        METH_VARARGS,
        "Get or set callback function"
    },
    {
        "read_flash",
        Firmware_read_flash,
        METH_VARARGS,
        "Read 16 bytes from internal or external flash memory"
    },
    {
        "reboot",
        Firmware_reboot,
        METH_VARARGS,
        "Reboot the hat and let it finish the upgrade"
    },
    { NULL, NULL, 0, NULL }
};


static PyTypeObject FirmwareType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "Firmware",
    .tp_doc = "Firmware Upgrade Manager",
    .tp_basicsize = sizeof(FirmwareObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = Firmware_new,
    .tp_dealloc = (destructor)Firmware_dealloc,
    .tp_traverse = (traverseproc)Firmware_traverse,
    .tp_clear = (inquiry)Firmware_clear,
    .tp_methods = Firmware_methods
};


int firmware_modinit(void)
{
    if (PyType_Ready(&FirmwareType) < 0)
        return -1;
    Py_INCREF(&FirmwareType);

    return 0;
}


void firmware_demodinit(void)
{
    Py_DECREF(&FirmwareType);
}


PyObject *firmware_init(void)
{
    return PyObject_CallObject((PyObject *)&FirmwareType, NULL);
}


int firmware_action_done(PyObject *firmware,
                         uint8_t reason,
                         uint8_t param)
{
    FirmwareObject *self = (FirmwareObject *)firmware;

    self->status = FW_STATUS_IDLE;

    return callback_queue(CALLBACK_FIRMWARE, reason, param, firmware);
}


int firmware_handle_callback(uint8_t reason, uint8_t param, PyObject *firmware)
{
    FirmwareObject *self = (FirmwareObject *)firmware;
    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject *arg_list;
    int rv;

    if (self->callback == Py_None)
    {
        PyGILState_Release(gstate);
        return 0;
    }

    arg_list = Py_BuildValue("(ii)", reason, param);
    rv = (PyObject_CallObject(self->callback, arg_list) != NULL) ? 0 : -1;
    Py_XDECREF(arg_list);
    PyGILState_Release(gstate);

    return rv;
}
