/* comms.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * I2C communications handling
 *
 * This takes place in a separate OS thread (not Python thread!) so
 * error reporting is not as easy as you might hope.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>

#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

#include "i2c.h"
#include "queue.h"
#include "port.h"
#include "protocol.h"

#ifdef DEBUG_I2C
#include "debug-i2c.h"
#endif


#define I2C_DEVICE_NAME "/dev/i2c-1"
#define HAT_ADDRESS 0x10 /* TODO: replace with the right number */


static int i2c_fd = -1;
static pthread_t comms_thread;
static int shutdown = 0;


#ifdef USE_DUMMY_I2C
#include "dummy-i2c.h"
#define read(f,b,n) dummy_i2c_read(f,b,n)
#define write(f,b,n) dummy_i2c_write(f,b,n)
#endif

/* Bit manipulation macros for multi-word bitmaps */
#define BITS_PER_WORD 32
#define BITMAP_INDEX(b) ((b) / BITS_PER_WORD)
#define BITMAP_SHIFT(b) ((b) % BITS_PER_WORD)
#define DEFINE_BITMAP(name, len) \
    uint32_t name[BITMAP_INDEX(len-1) + 1]
#define BITMAP_SET(name, bit) do { \
        name[BITMAP_INDEX(bit)] |= 1 << BITMAP_SHIFT(bit);      \
    } while (0)
#define BITMAP_CLEAR(name, bit) do { \
        name[BITMAP_INDEX(bit)] &= ~(1 << BITMAP_SHIFT(bit));   \
    } while (0)
#define BITMAP_IS_SET(name, bit) \
    (name[BITMAP_INDEX(bit)] & (1 << BITMAP_SHIFT(bit)))

/* Bitmap indicating that the given port is expecting a value response */
DEFINE_BITMAP(expecting_value_on_port, 256);


static inline uint16_t extract_uint16(uint8_t *buffer)
{
    return buffer[0] | (buffer[1] << 8);
}

static inline uint32_t extract_uint32(uint8_t *buffer)
{
    return buffer[0] |
        (buffer[1] << 8) |
        (buffer[2] << 16) |
        (buffer[3] << 24);
}


static void report_comms_error(int rv)
{
    /* Raise an exception in the comms thread */
    PyGILState_STATE gstate = PyGILState_Ensure();

    errno = rv;
    PyErr_SetFromErrno(PyExc_IOError);
    PyGILState_Release(gstate);
}


/* Returns 1 for success, 0 for failure */
static int send_command(uint8_t *buffer)
{
    size_t nbytes;

#ifndef USE_DUMMY_I2C
    if (ioctl(i2c_fd, I2C_SLAVE, HAT_ADDRESS) < 0)
        return 0;
#endif

    /* Is this a Port Info request asking for the value? */
    if (buffer[2] == TYPE_PORT_INFO_REQ &&
        buffer[4] == PORT_INFO_VALUE)
    {
        BITMAP_SET(expecting_value_on_port, buffer[3]);
    }

    /* Construct the buffer length */
    nbytes = buffer[0];
    if (nbytes >= 0x80)
        nbytes = (nbytes & 0x7f) | (buffer[1] << 7);
    if (write(i2c_fd, buffer, nbytes) < 0)
        return 0;
    return 1;
}


/* Returns 0 for success, -1 for I2C failure, -2 for out of memory */
static int read_message(uint8_t **pbuffer)
{
    size_t nbytes;
    uint8_t byte;
    uint8_t *buffer;
    int offset = 1;

    *pbuffer = NULL;

#ifndef USE_DUMMY_I2C
    if (ioctl(i2c_fd, I2C_SLAVE, HAT_ADDRESS) < 0)
        return 0;
#endif

    /* Read in the length */
    if (read(i2c_fd, &byte, 1) < 0)
        return -1;
    if (byte == 0)
        return 0; /* Use a completely empty message as a NOP */
    if ((nbytes = byte) >= 0x80)
    {
        if (read(i2c_fd, &byte, 1) < 0)
            return -1;
        nbytes = (nbytes & 0x7f) | (byte << 7);
    }

    if ((buffer = malloc(nbytes)) == NULL)
        return -2;
    buffer[0] = nbytes & 0x7f;
    if (nbytes >= 0x80)
    {
        buffer[1] = (nbytes >> 7) & 0xff;
        offset = 2;
    }

    if (read(i2c_fd, buffer+offset, nbytes-offset) < 0)
    {
        free(buffer);
        return -1;
    }

    *pbuffer = buffer;
    return 0;
}


/* NB: returns -1 on error (with errno set), 1 if the message has
 * been handled, and 0 if another handler should look at it.
 */
static int handle_attached_io_message(uint8_t *buffer, uint16_t nbytes)
{
    /* Hab Attacked I/O messages are at least 5 bytes long */
    if (nbytes < 5 || buffer[2] != TYPE_HUB_ATTACHED_IO)
        return 0; /* Not for us */
    if (buffer[1] != 0 || buffer[3] >= NUM_HUB_PORTS)
    {
        errno = EPROTO; /* Protocol error */
        return -1;
    }
    switch (buffer[4])
    {
        case 0: /* Detached I/O message */
            if (port_detach_port(buffer[3]) < 0)
            {
                errno = EPROTO;
                return -1;
            }
            break;

        case 1: /* Attached I/O message */
            /* Attachment messages have another 10 bytes of data */
            if (nbytes < 15)
            {
                errno = EPROTO;
                return -1;
            }
            if (port_attach_port(buffer[3],
                                 extract_uint16(buffer+5), /* ID */
                                 buffer+7, /* hw_revision */
                                 buffer+11 /* fw_revision */) < 0)
            {
                errno = EPROTO;
                return -1;
            }
            break;

        default:
            errno = EPROTO;
            return -1;
    }

    /* Packet was handled here */
    return 1;
}


static int handle_port_format_single(uint8_t *buffer, uint16_t nbytes)
{
    int rv;

    /* PF(S) messages are 10 bytes long */
    if (nbytes < 10 || buffer[2] != TYPE_PORT_FORMAT_SINGLE)
        return 0; /* NOt for us */

    if (buffer[1] != 0)
    {
        errno = EPROTO; /* Protocol error */
        return -1;
    }

    if ((rv = port_new_format(buffer[3])) < 0)
    {
        errno = EPROTO;
        return -1;
    }
    /* We still want to pass this on */
    return 0;
}


static int handle_port_value_single(uint8_t *buffer,
                                    uint16_t nbytes,
                                    int *ppassback)
{
    /* Assume nothing was waiting for these values */
    *ppassback = 0;

    /* PV(S) messages are at least 5 bytes long */
    if (nbytes < 5 || buffer[2] != TYPE_PORT_VALUE_SINGLE)
        return 0; /* Not for us */
    if (buffer[1] != 0)
    {
        errno = EPROTO; /* Protocol error */
        return -1;
    }

    /* Because life is never easy, the message can contain a sequence
     * of ports and values.  Further, decoding the values out of the
     * buffer requires knowing what the data format is, which is a
     * feature of the device mode.  We have to loop through until we
     * run out of buffer.
     */
    buffer += 3;
    nbytes -= 3;
    while (nbytes > 0)
    {
        int rv;

        if (nbytes < 2)
        {
            /* Less than the minimum possible, bomb out */
            errno = EPROTO;
            return -1;
        }

        /* Check if the foreground will be waiting for this */
        if (BITMAP_IS_SET(expecting_value_on_port, buffer[0]))
        {
            BITMAP_CLEAR(expecting_value_on_port, buffer[0]);
            *ppassback = 1;
        }
        if ((rv = port_new_value(buffer[0], buffer+1, nbytes-1)) < 0)
        {
            errno = EPROTO;
            return -1;
        }
        nbytes -= rv;
        buffer += rv;
    }

    /* Packet has been handled */
    return 1;
}


/* Returns 1 on success, 0 on failure */
static int poll_i2c(void)
{
    uint8_t *buffer;
    int rv;
    int passback;
    uint16_t nbytes;

    if ((rv = read_message(&buffer)) < 0)
    {
        if (rv == -2)
            errno = ENOMEM;
        return 0;
    }
    if (buffer == NULL)
        return 1;

#ifdef DEBUG_I2C
    log_i2c(buffer, 0);
#endif

    nbytes = buffer[0];
    if (nbytes >= 0x80)
    {
        buffer++;
        nbytes = (nbytes & 0x7f) | (buffer[0] << 7);
    }

    /* Is this something to deal with immediately? */
    if ((rv = handle_attached_io_message(buffer, nbytes)) != 0)
    {
        free(buffer);
        return (rv < 0) ? 0 : 1;
    }
    if ((rv = handle_port_format_single(buffer, nbytes)) < 0)
    {
        free(buffer);
        return 1;
    }
    if ((rv = handle_port_value_single(buffer, nbytes, &passback)) < 0 ||
        (rv > 0 && !passback))
    {
        free(buffer);
        return (rv < 0) ? 0 : 1;
    }

    if (queue_return_buffer(buffer) < 0)
    {
        free(buffer);
        return 0;
    }

    return 1;
}


/* Thread function for background I2C communications */
static void *run_comms(void *args __attribute__((unused)))
{
    uint8_t *buffer;
    int rv;
    int running = 1;

    while (running && !shutdown)
    {
        if ((rv = queue_check(&buffer)) != 0)
        {
            report_comms_error(rv);
            running = 0;
        }
        else
        {
            if (buffer != NULL)
            {
#ifdef DEBUG_I2C
                log_i2c(buffer, 1);
#endif
                running = send_command(buffer);
                free(buffer);
                /* XXX: wait for response? */
            }
            if (running)
                running = poll_i2c();
            if (!running)
                report_comms_error(errno);
        }
    }

    return NULL;
}


/* Open the I2C bus, select the Hat as the device to communicate with,
 * and return the file descriptor.  You must close the file descriptor
 * when you are done with it.
 */
int i2c_open_hat(void)
{
    int rv;

#ifdef USE_DUMMY_I2C
    const char *err_str;
    if ((i2c_fd = open_dummy_i2c_socket(&err_str)) < 0)
    {
        PyErr_SetString(PyExc_IOError, err_str);
        return -1;
    }
#else
    if ((i2c_fd = open(I2C_DEVICE_NAME, O_RDWR)) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }
#endif /* USE_DUMMY_I2C */

#ifdef DEBUG_I2C
    if (log_i2c_init() < 0)
    {
        PyErr_SetString(PyExc_IOError, "I2C log init failed");
        return -1;
    }
#endif /* DEBUG_I2C */

    /* Initialise thread work queue */
    if ((rv = queue_init()) != 0)
    {
        errno = rv;
        PyErr_SetFromErrno(PyExc_IOError);
        close(i2c_fd);
        return -1;
    }
    if (!PyEval_ThreadsInitialized())
        PyEval_InitThreads();
    if ((rv = pthread_create(&comms_thread, NULL, run_comms, NULL)) != 0)
    {
        errno = rv;
        PyErr_SetFromErrno(PyExc_IOError);
        close(i2c_fd);
        return -1;
    }

    return i2c_fd;
}

/* Close the connection to the Hat (so that others can access the I2C
 * connector)
 */
int i2c_close_hat(void)
{
    /* Kill comms thread */
    shutdown = 1;
    pthread_join(comms_thread, NULL);

    if (close(i2c_fd) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }
    i2c_fd = -1;
    return 0;
}

