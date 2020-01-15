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


#define I2C_DEVICE_NAME "/dev/i2c-1"
#define HAT_ADDRESS 0x10 /* TODO: replace with the right number */


static int i2c_fd = -1;
static pthread_t comms_thread;


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

    if (ioctl(i2c_fd, I2C_SLAVE, HAT_ADDRESS) < 0)
        return 0;

    /* Construct the buffer length */
    nbytes = buffer[0];
    if (nbytes >= 0x80)
        nbytes = (nbytes & ~0x80) | (buffer[1] << 7);
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

    *pbuffer = NULL;

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
        buffer[1] = (nbytes >> 7) & 0xff;

    if (read(i2c_fd, buffer+2, nbytes-2) < 0)
    {
        free(buffer);
        return -1;
    }

    *pbuffer = buffer;
    return 0;
}


/* Returns 1 on success, 0 on failure */
static int poll_i2c(void)
{
    uint8_t *buffer;
    int rv;

    if ((rv = read_message(&buffer)) < 0)
    {
        if (rv == -2)
            errno = ENOMEM;
        return 0;
    }
    if (buffer == NULL)
        return 1;

    /* TODO: Is this something to deal with immediately? */

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

    while (running)
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
    int fd;
    int rv;

    if ((i2c_fd = open(I2C_DEVICE_NAME, O_RDWR)) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }
    /* Initialise thread work queue */
    if ((rv = queue_init()) != 0)
    {
        errno = rv;
        PyErr_SetFromErrno(PyExc_IOError);
        close(i2c_fd);
        return -1;
    }
    if ((rv = pthread_create(&comms_thread, NULL, run_comms, NULL)) != 0)
    {
        errno = rv;
        PyErr_SetFromErrno(PyExc_IOError);
        close(i2c_fd);
        return -1;
    }

    return fd;
}

/* Close the connection to the Hat (so that others can access the I2C
 * connector)
 */
int i2c_close_hat(void)
{
    /* TODO: Kill comms thread */
    if (close(i2c_fd) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }
    i2c_fd = -1;
    return 0;
}

