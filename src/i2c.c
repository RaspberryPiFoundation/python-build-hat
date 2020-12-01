/* i2c.c
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
 * I2C communications handling
 *
 * This takes place in separate OS threads (not Python threads!) so
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
#include <poll.h>
#include <sys/eventfd.h>
#include <time.h>

#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

#include "i2c.h"
#include "queue.h"
#include "port.h"
#include "pair.h"
#include "firmware.h"
#include "callback.h"
#include "protocol.h"
#include "debug-i2c.h"


#define I2C_DEVICE_NAME "/dev/i2c-1"
#define HAT_ADDRESS 0x12

#define I2C_GPIO_NUMBER "5"
#define BASE_DIRECTORY "/sys/class/gpio"
#define EXPORT_PSEUDOFILE BASE_DIRECTORY "/export"
#define UNEXPORT_PSEUDOFILE BASE_DIRECTORY "/unexport"
#define GPIO_DIRECTORY BASE_DIRECTORY "/gpio" I2C_GPIO_NUMBER
#define DIRECTION_PSEUDOFILE GPIO_DIRECTORY "/direction"
#define INTERRUPT_PSEUDOFILE GPIO_DIRECTORY "/edge"
#define VALUE_PSEUDOFILE GPIO_DIRECTORY "/value"

#define RESET_GPIO_NUMBER "4"
#define BOOT0_GPIO_NUMBER "22"

static int gpio_fd = -1;
static int gpio_state = 0;

static int i2c_fd = -1;
static int rx_event_fd = -1;
static pthread_t comms_rx_thread;
static pthread_t comms_tx_thread;
static int shutdown = 0;
static int heard_from_hat = 0;

static PyObject *firmware_object = NULL;


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
/* Bitmap indicating that the given alert is expected */
DEFINE_BITMAP(expecting_alert, 5);


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


static int read_wake_gpio(void)
{
    char buffer;

    if (lseek(gpio_fd, 0, SEEK_SET) == (off_t)-1 ||
        read(gpio_fd, &buffer, 1) < 0)
    {
        return -1;
    }
    gpio_state =  (buffer == '1') ? 1 : 0;
    return 0;
}


static int export_gpio(const char *gpio)
{
    int fd;
    int rv = 0;

    if ((fd = open(EXPORT_PSEUDOFILE, O_WRONLY)) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }
    if (write(fd, gpio, strlen(gpio)) < 0)
    {
        /* EBUSY implies the GPIO is already exported.  That's OK */
        if (errno != EBUSY)
        {
            PyErr_SetFromErrno(PyExc_IOError);
            rv = -1;
        }
    }
    close(fd);

    return rv;
}


static void unexport_gpio(const char *gpio)
{
    int fd;

    if ((fd = open(UNEXPORT_PSEUDOFILE, O_WRONLY)) < 0)
        return;
    if (write(fd, gpio, strlen(gpio)) < 0)
        fprintf(stderr, "Error unexporting wake GPIO");
    close(fd);
}


static int set_gpio_direction(const char *direction_pseudofile,
                              const char *direction)
{
    int fd;

    if ((fd = open(direction_pseudofile, O_WRONLY)) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }
    if (write(fd, direction, strlen(direction)) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}


static int open_writeable_gpio(const char *gpio)
{
    int fd;
    struct timespec timeout = { 0, 50000000 }; /* 50ms */
    struct timespec remaining;
    char filename[64];

    if (export_gpio(gpio) < 0)
        return -1;

    /* Give Linux a moment to get its act together */
    while (nanosleep(&timeout, &remaining) < 0 && errno == EINTR)
    {
        timeout = remaining;
    }

    /* Now set the direction */
    sprintf(filename, "/sys/class/gpio/gpio%s/direction", gpio);
    if (set_gpio_direction(filename, "out") < 0)
    {
        unexport_gpio(gpio);
        return -1;
    }

    /* And open for writing */
    sprintf(filename, "/sys/class/gpio/gpio%s/value", gpio);
    if ((fd = open(filename, O_WRONLY)) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        unexport_gpio(gpio);
        return -1;
    }

    return fd;
}


static void close_gpio(int fd, const char *gpio)
{
    close(fd);
    unexport_gpio(gpio);
}


static int write_gpio(int fd, int value)
{
    char buffer = value ? '1' : '0';
    ssize_t rv = write(fd, &buffer, 1);

    if (rv < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }
    else if (rv != 1)
    {
        PyErr_SetString(PyExc_IOError, "Unable to write to GPIO");
        return -1;
    }
    return 0;
}


int i2c_reset_hat(void)
{
    int boot0_fd, reset_fd;
    struct timespec timeout = { 0, 10000000 }; /* 10ms */
    struct timespec remaining;

    /* First acquire the boot0 pin and set it low */
    /* Otherwise we could reset the hat into its embedded bootloader */
    if ((boot0_fd = open_writeable_gpio(BOOT0_GPIO_NUMBER)) < 0)
        return -1;
    if (write_gpio(boot0_fd, 0) < 0)
    {
        close_gpio(boot0_fd, BOOT0_GPIO_NUMBER);
        return -1;
    }

    /* Now acquire the reset pin and set it low,
     * putting the chip in reset.
     */
    if ((reset_fd = open_writeable_gpio(RESET_GPIO_NUMBER)) < 0)
    {
        close_gpio(boot0_fd, BOOT0_GPIO_NUMBER);
        return -1;
    }
    if (write_gpio(reset_fd, 0) < 0)
    {
        close_gpio(boot0_fd, BOOT0_GPIO_NUMBER);
        close_gpio(reset_fd, RESET_GPIO_NUMBER);
        return -1;
    }

    /* Give ourselves a good 10ms for the reset to take */
    while (nanosleep(&timeout, &remaining) < 0 && errno == EINTR)
    {
        timeout = remaining;
    }

    /* Now set the reset pin back high (out of reset) */
    if (write_gpio(reset_fd, 1) < 0)
    {
        close_gpio(boot0_fd, BOOT0_GPIO_NUMBER);
        close_gpio(reset_fd, RESET_GPIO_NUMBER);
        return -1;
    }

    /* And rest awhile again */
    timeout.tv_sec = 0;
    timeout.tv_nsec = 50000000;
    while (nanosleep(&timeout, &remaining) < 0 && errno == EINTR)
    {
        timeout = remaining;
    }

    /* Release our hold on the pins */
    close_gpio(boot0_fd, BOOT0_GPIO_NUMBER);
    close_gpio(reset_fd, RESET_GPIO_NUMBER);

    return 0;
}


static int open_wake_gpio(void)
{
    int fd;
    const char *edge = "both";
    struct timespec timeout = { 0, 50000000 }; /* 50ms */
    struct timespec remaining;

    /* First export the GPIO */
    if (export_gpio(I2C_GPIO_NUMBER) < 0)
        return -1;

    /* Give Linux a moment to get its act together */
    while (nanosleep(&timeout, &remaining) < 0 && errno == EINTR)
    {
        timeout = remaining;
    }

    /* Now set the direction */
    if (set_gpio_direction(DIRECTION_PSEUDOFILE, "in") < 0)
    {
        unexport_gpio(I2C_GPIO_NUMBER);
        return -1;
    }

    /* ...and the interrupt generation */
    if ((fd = open(INTERRUPT_PSEUDOFILE, O_WRONLY)) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        unexport_gpio(I2C_GPIO_NUMBER);
        return -1;
    }
    if (write(fd, edge, 4) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        close(fd);
        unexport_gpio(I2C_GPIO_NUMBER);
        return -1;
    }

    /* Finally open the GPIO for reading */
    if ((gpio_fd = open(VALUE_PSEUDOFILE, O_RDWR)) < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        unexport_gpio(I2C_GPIO_NUMBER);
        return -1;
    }
    /* ...and read it */
    if (read_wake_gpio() < 0)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        close(gpio_fd);
        gpio_fd = -1;
        unexport_gpio(I2C_GPIO_NUMBER);
        return -1;
    }

    return 0;
}


static void close_wake_gpio(void)
{
    if (gpio_fd == -1)
        return;

    close(gpio_fd);
    gpio_fd = -1;
    unexport_gpio(I2C_GPIO_NUMBER);
}


static void report_comms_error(void)
{
    /* XXX: Figure out something to do here */
}


/* Called from foreground */
int i2c_check_comms_error(void)
{
    if (!heard_from_hat)
    {
        PyErr_SetString(cmd_get_exception(), "HAT not responding");
        return -1;
    }
    /* XXX: else return error code somehow */

    /* Otherwise all is well */
    return 0;
}


static int signal_rx_shutdown(void)
{
    uint64_t value = 1;
    if (write(rx_event_fd, (uint8_t *)&value, 8) < 0)
        return -1;
    return 0;
}


static int read_rx_event(void)
{
    uint64_t value;

    /* We only care about reading to kill the poll flag */
    if (read(rx_event_fd, (uint8_t *)&value, 8) < 0)
        return -1;
    return 0;
}


static int poll_for_rx(void)
{
    struct pollfd pfds[2];
    int rv;

    /* First check if the gpio has changed */
    DEBUG0(I2C, CHECK_GPIO);
    pfds[0].fd = gpio_fd;
    pfds[0].events = POLLPRI;
    pfds[0].revents = 0;
    if ((rv = poll(pfds, 1, 0)) < 0)
    {
        report_comms_error();
        return 0;
    }
    else if (rv != 0)
    {
        /* Yes it has.  Read the new value */
        if (read_wake_gpio() < 0)
            report_comms_error();
        /* Always return, in case there is more */
        return 0;
    }

    /* If the GPIO is raised, keep reading */
    if (gpio_state)
        return 1;

    /* Otherwise wait for a GPIO state change or an event */
    DEBUG0(I2C, WAIT_FOR_RX);
    pfds[0].fd = gpio_fd;
    pfds[0].events = POLLPRI;
    pfds[0].revents = 0;
    pfds[1].fd = rx_event_fd;
    pfds[1].events = POLLIN;
    pfds[1].revents = 0;

    if ((rv = poll(pfds, 2, -1)) < 0)
    {
        report_comms_error();
        return 0;
    }
    DEBUG1(I2C, WAIT_DONE, rv);
    if ((pfds[1].revents & POLLIN) != 0)
        read_rx_event();
    if ((pfds[0].revents & POLLPRI) != 0)
        if (read_wake_gpio() < 0)
            report_comms_error();
    /* Loop for another check just in case */
    return 0;
}


static int read_message(uint8_t **pbuffer)
{
    size_t nbytes;
    uint8_t byte;
    uint8_t *buffer;
    int offset = 1;

    *pbuffer = NULL;

    DEBUG0(I2C, START_IOCTL);
    if (ioctl(i2c_fd, I2C_SLAVE, HAT_ADDRESS) < 0)
        return -1;

    /* Read in the length */
    DEBUG0(I2C, READ_LEN);
    if (read(i2c_fd, &byte, 1) < 0)
        return -1;
    if (byte == 0)
        return 0; /* Use a completely empty message as a NOP */
    if ((nbytes = byte) >= 0x80)
    {
        DEBUG0(I2C, READ_LEN_2);
        if (read(i2c_fd, &byte, 1) < 0)
            return -1;
        nbytes = (nbytes & 0x7f) | (byte << 7);
    }

    if ((buffer = malloc(nbytes)) == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    buffer[0] = nbytes & 0x7f;
    if (nbytes >= 0x80)
    {
        buffer[0] |= 0x80;
        buffer[1] = (nbytes >> 7) & 0xff;
        offset = 2;
    }

    DEBUG0(I2C, READ_BODY);
    if (read(i2c_fd, buffer+offset, nbytes-offset) < 0)
    {
        free(buffer);
        return -1;
    }
    DEBUG0(I2C, READ_DONE);

    *pbuffer = buffer;
    return 0;
}


/* The handle_ functions return -1 on error (with errno set), 1 if the
 * message has been handled, and 0 if another handler should look at
 * it.
 */

static int handle_attached_io_message(uint8_t *buffer, uint16_t nbytes)
{
    /* Hub Attacked I/O messages are at least 5 bytes long */
    if (nbytes < 5 || buffer[2] != TYPE_HUB_ATTACHED_IO)
        return 0; /* Not for us */
    if (buffer[1] != 0)
    {
        errno = EPROTO; /* Protocol error */
        return -1;
    }
    switch (buffer[4])
    {
        case 0: /* Detached I/O message */
            if (buffer[3] < NUM_HUB_PORTS)
            {
                if (port_detach_port(buffer[3]) < 0)
                {
                    errno = EPROTO;
                    return -1;
                }
            }
            /* Otherwise it must be a virtual port */
            else if (pair_detach_port(buffer[3]) < 0)
            {
                errno = EPROTO;
                return -1;
            }
            break;

        case 1: /* Attached I/O message */
            /* Attachment messages have another 10 bytes of data */
            if (nbytes < 15 || buffer[3] >= NUM_HUB_PORTS)
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

        case 2: /* Attached Virtual I/O */
            if (nbytes < 9 ||
                pair_attach_port(buffer[3],
                                 buffer[7],
                                 buffer[8],
                                 extract_uint16(buffer+5)) < 0)
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
        return 0; /* Not for us */

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

    /* We still want to pass this on, but deal with that in the caller */
    return 1;
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


static int handle_port_value_combi(uint8_t *buffer,
                                   uint16_t nbytes,
                                   int *ppassback)
{
    int i, rv;
    uint8_t port_id;
    uint16_t entry_mask;

    /* Assume nothing was waiting for these values */
    *ppassback = 0;

    /* PV(C) messages are at least 6 bytes long */
    if (nbytes < 6 || buffer[2] != TYPE_PORT_VALUE_COMBINED)
        return 0; /* Not for us */
    if (buffer[1] != 0)
    {
        errno = EPROTO; /* Protocol error */
        return -1;
    }

    port_id = buffer[3];
    if (BITMAP_IS_SET(expecting_value_on_port, port_id))
    {
        BITMAP_CLEAR(expecting_value_on_port, port_id);
        *ppassback = 1;
    }
    entry_mask = buffer[5] | (buffer[4] << 8);
    buffer += 6;
    nbytes -= 6;

    for (i = 0; i < 16; i++)
    {
        if (nbytes == 0)
        {
            /* No more data, should there be? */
            if (entry_mask != 0)
            {
                /* There should have been.  Complain */
                errno = EPROTO;
                return -1;
            }
            return 1; /* Packet has been handled */
        }
        if ((entry_mask & (1 << i)) != 0)
        {
            /* Expecting a value here */
            if ((rv = port_new_combi_value(port_id, i, buffer, nbytes)) < 0)
            {
                errno = EPROTO;
                return -1;
            }
            nbytes -= rv;
            buffer += rv;
            entry_mask &= ~(1 << i);
        }
    }

    if (nbytes != 0)
    {
        errno = EPROTO;
        return -1;
    }

    /* Packet has been handled */
    return 1;
}


static int handle_output_feedback(uint8_t *buffer, uint16_t nbytes)
{
    /* The Port Output Command Feedback message must be at least 5 bytes */
    if (nbytes < 5 || buffer[2] != TYPE_PORT_OUTPUT_FEEDBACK)
        return 0; /* Not for us */
    if (buffer[1] != 0)
    {
        errno = EPROTO; /* Protocol error */
        return -1;
    }

    /* Life still isn't easy: this time the feedback message can be
     * for many ports.  We process the message two bytes at a time
     * (port number and status, in that order), starting at byte 3.
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

        if (buffer[0] < NUM_HUB_PORTS)
            rv = port_feedback_status(buffer[0], buffer[1]);
        else
            rv = pair_feedback_status(buffer[0], buffer[1]);
        if (rv < 0)
        {
            errno = EPROTO;
            return -1;
        }
        nbytes -= 2;
        buffer += 2;
    }

    return 0;
}


static int handle_firmware_response(uint8_t *buffer, uint16_t nbytes)
{
    if (nbytes < 5 || buffer[2] != TYPE_FIRMWARE_RESPONSE)
        return 0;
    if (buffer[1] != 0)
    {
        errno = EPROTO; /* Protocol error */
        return -1;
    }

    if (buffer[3] == FIRMWARE_INITIALIZE)
    {
        /* This is passed on via callback -- erase can take a while */
        if (nbytes != 5)
        {
            errno = EPROTO;
            return -1;
        }
        if (firmware_object != NULL)
        {
            if (firmware_action_done(firmware_object,
                                     FIRMWARE_INITIALIZE,
                                     buffer[4]) < 0)
            {
                errno = EPROTO;
                return -1;
            }
        }
        return 1;
    }

    return 0;
}


static int handle_alert(uint8_t *buffer, uint16_t nbytes, int *ppassback)
{
    /* Assume nothing was waiting for this alert */
    *ppassback = 0;

    if (nbytes < 6 ||
        buffer[2] != TYPE_HUB_ALERT ||
        buffer[4] != ALERT_OP_UPDATE)
        return 0;
    if (buffer[1] != 0 || nbytes != 6)
    {
        errno = EPROTO;
        return -1;
    }

    /* Don't drop this packet if it's expected */
    if (BITMAP_IS_SET(expecting_alert, buffer[3]))
    {
        BITMAP_CLEAR(expecting_alert, buffer[3]);
        *ppassback = 1;
    }

    callback_queue(CALLBACK_ALERT, buffer[3], buffer[5], NULL);
    return 1;
}


static int handle_immediate(uint8_t *buffer, uint16_t nbytes)
{
    int rv;
    int passback = 0;

    /* Is this something to deal with immediately? */
    if ((rv = handle_attached_io_message(buffer, nbytes)) != 0)
        return rv;

    if ((rv = handle_port_format_single(buffer, nbytes)) != 0)
    {
        /* We still want to pass this to the foreground if valid */
        return (rv < 0) ? rv : 0;
    }

    if ((rv = handle_port_value_single(buffer, nbytes, &passback)) < 0 ||
        (rv > 0 && !passback))
    {
        return rv;
    }
    else if (rv > 0)
    {
        /* Handled, but pass to foreground */
        return 0;
    }

    if ((rv = handle_port_value_combi(buffer, nbytes, &passback)) < 0 ||
        (rv > 0 && !passback))
    {
        return rv;
    }
    else if (rv > 0)
    {
        /* Handled, but pass to foreground */
        return 0;
    }

    if ((rv = handle_output_feedback(buffer, nbytes)) != 0)
        return rv;

    if ((rv = handle_firmware_response(buffer, nbytes)) != 0)
        return rv;

    if ((rv = handle_alert(buffer, nbytes, &passback)) < 0 ||
        (rv > 0 && !passback))
    {
        return rv;
    }
    else if (rv > 0)
    {
        /* Handled, but pass to foreground */
        return 0;
    }

    return 0; /* Nothing interesting here, guv */
}


/* Thread function for background I2C reception */
static void *run_comms_rx(void *args __attribute__((unused)))
{
    uint8_t *buffer;
    int rv;
    uint16_t nbytes;

    while (!shutdown)
    {
        if (poll_for_rx() != 0)
        {
            if (read_message(&buffer) < 0)
            {
                report_comms_error();
                continue;
            }
            heard_from_hat = 1;
            if (buffer == NULL)
                continue; /* Nothing to do */

#ifdef DEBUG_I2C
            log_i2c(buffer, 0);
#endif

            nbytes = buffer[0];
            if (nbytes >= 0x80)
            {
                buffer++;
                nbytes = (nbytes & 0x7f) | (buffer[0] << 7);
            }

            if ((rv = handle_immediate(buffer, nbytes)) < 0)
            {
                free(buffer);
                report_comms_error();
            }
            else if (rv > 0)
            {
                /* Buffer should not be passed to foreground */
                free(buffer);
            }
            else if (queue_return_buffer(buffer) < 0)
            {
                free(buffer);
                report_comms_error();
            }
        }
    }

    return NULL;
}


/* Thread function for background I2C reception */
static void *run_comms_tx(void *args __attribute__((unused)))
{
    size_t nbytes;
    uint8_t *buffer;
    int rv;

    while (!shutdown)
    {
        if ((rv = queue_check(&buffer)) < 0)
        {
            report_comms_error();
        }
        else if (buffer != NULL)
        {
#ifdef DEBUG_I2C
            log_i2c(buffer, 1);
#endif

            if (ioctl(i2c_fd, I2C_SLAVE, HAT_ADDRESS) < 0)
            {
                report_comms_error();
                free(buffer);
                continue;
            }
            DEBUG0(I2C, TX_IOCTL_DONE);

            /* This is the Port Info request asking for the value? */
            if (buffer[0] < 0x80)
            {
                if (buffer[2] == TYPE_PORT_INFO_REQ &&
                    buffer[4] == PORT_INFO_VALUE)
                {
                    BITMAP_SET(expecting_value_on_port, buffer[3]);
                }
                else if (buffer[2] == TYPE_HUB_ALERT &&
                         buffer[4] == ALERT_OP_REQUEST)
                {
                    BITMAP_SET(expecting_alert, buffer[3]);
                }
            }

            /* Construct the buffer length */
            nbytes = buffer[0];
            if (nbytes >= 0x80)
                nbytes = (nbytes & 0x7f) | (buffer[1] << 7);
            if (write(i2c_fd, buffer, nbytes) < 0)
                report_comms_error();
            DEBUG0(I2C, TX_DONE);
            free(buffer);
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

    if ((i2c_fd = open(I2C_DEVICE_NAME, O_RDWR)) < 0)
    {
        if (errno == ENOENT)
        {
            /* "File not found" probably means I2C is not enabled */
            PyErr_SetString(PyExc_IOError,
                            "Unable to access I2C: has it been enabled?");
        }
        else
        {
            PyErr_SetFromErrno(PyExc_IOError);
        }
        return -1;
    }
    if (open_wake_gpio() < 0)
        return -1; /* Exception already raised */

#ifdef DEBUG_I2C
    if (log_i2c_init() < 0)
    {
        PyErr_SetString(PyExc_IOError, "I2C log init failed");
        return -1;
    }
#endif /* DEBUG_I2C */

    /* Reset the hat */
    if (i2c_reset_hat() < 0)
    {
        /* Exception already raised */
        close_wake_gpio();
        close(i2c_fd);
        return -1;
    }

    /* Initialise thread work queues */
    if ((rv = queue_init()) != 0)
    {
        errno = rv;
        close_wake_gpio();
        PyErr_SetFromErrno(PyExc_IOError);
        close(i2c_fd);
        return -1;
    }

    /* Create the event for signalling to the receiver */
    if ((rx_event_fd = eventfd(0, 0)) == -1)
    {
        errno = rv;
        close_wake_gpio();
        PyErr_SetFromErrno(PyExc_IOError);
        close(i2c_fd);
        return -1;
    }

    /* Start the Rx and Tx threads */
    if (!PyEval_ThreadsInitialized())
        PyEval_InitThreads();
    if ((rv = pthread_create(&comms_rx_thread, NULL, run_comms_rx, NULL)) != 0)
    {
        errno = rv;
        close_wake_gpio();
        close(rx_event_fd);
        PyErr_SetFromErrno(PyExc_IOError);
        close(i2c_fd);
        return -1;
    }
    if ((rv = pthread_create(&comms_tx_thread, NULL, run_comms_tx, NULL)) != 0)
    {
        shutdown = 1;
        signal_rx_shutdown();
        pthread_join(comms_rx_thread, NULL);

        errno = rv;
        PyErr_SetFromErrno(PyExc_IOError);

        close_wake_gpio();
        close(rx_event_fd);
        close(i2c_fd);
        i2c_fd = -1;
        return -1;
    }

    return i2c_fd;
}

/* Close the connection to the Hat (so that others can access the I2C
 * connector)
 */
int i2c_close_hat(void)
{
    PyObject *tmp;

    /* Kill comms thread */
    shutdown = 1;
    signal_rx_shutdown();
    queue_shutdown(); /* Kicks the tx thread */
    Py_BEGIN_ALLOW_THREADS
    pthread_join(comms_rx_thread, NULL);
    pthread_join(comms_tx_thread, NULL);
    Py_END_ALLOW_THREADS

    if (i2c_fd != -1)
    {
        close(i2c_fd);
        i2c_fd = -1;
    }
    if (rx_event_fd != -1)
    {
        close(rx_event_fd);
        rx_event_fd = -1;
    }

    close_wake_gpio();

    tmp = firmware_object;
    firmware_object = NULL;
    Py_XDECREF(tmp);

    return 0;
}


/* Register the firmware object with the I2C subsystem so it knows
 * where to direct firmware callbacks
 */
void i2c_register_firmware_object(PyObject *firmware)
{
    PyObject *tmp = firmware_object;

    Py_INCREF(firmware);
    firmware_object = firmware;
    Py_XDECREF(tmp);
}
