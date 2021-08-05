/* uart.c
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
 * UART communications handling
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
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/un.h>

#include "uart.h"
#include "queue.h"
#include "port.h"
#include "callback.h"
#include "protocol.h"

#define SIGSIZE 64
unsigned char signature[SIGSIZE];
#define IMAGEBUFSIZE (240*1024)
unsigned char imagebuf[IMAGEBUFSIZE + 1];

#define BAUD B115200
#define MAX_EVENTS 10
#define UART_DEVICE_NAME "/dev/serial0"
#define HAT_ADDRESS 0x12

#define DISCONNECTED "disconnected"
#define CONNECTED "connected to active ID "
#define PULSEDONE "pulse done"
#define RAMPDONE "ramp done"
#define ERROR "Error"
#define MODE "  M"
#define FORMAT "    format "

#define BASE_DIRECTORY "/sys/class/gpio"
#define EXPORT_PSEUDOFILE BASE_DIRECTORY "/export"
#define UNEXPORT_PSEUDOFILE BASE_DIRECTORY "/unexport"
#define DIRECTION_PSEUDOFILE GPIO_DIRECTORY "/direction"
#define INTERRUPT_PSEUDOFILE GPIO_DIRECTORY "/edge"
#define VALUE_PSEUDOFILE GPIO_DIRECTORY "/value"

#define RESET_GPIO_NUMBER "4"
#define BOOT0_GPIO_NUMBER "22"

#define INTERVAL 100000000

static int uart_fd = -1;
static int rx_event_fd = -1;
static pthread_t comms_rx_thread;
static pthread_t comms_tx_thread;
static int shutdown = 0;
static int heard_from_hat = 0;

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
    struct timespec timeout = { 0, INTERVAL };
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


int uart_reset_hat(void)
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
    timeout.tv_nsec = INTERVAL;
    while (nanosleep(&timeout, &remaining) < 0 && errno == EINTR)
    {
        timeout = remaining;
    }

    /* Release our hold on the pins */
    close_gpio(boot0_fd, BOOT0_GPIO_NUMBER);
    close_gpio(reset_fd, RESET_GPIO_NUMBER);

    return 0;
}


static void report_comms_error(void)
{
    /* XXX: Figure out something to do here */
}


/* Called from foreground */
int uart_check_comms_error(void)
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

int lastmode = 0;
int lastport = 0;
uint8_t ports[][2] = {{ 0 , 0 },
                     { 0 , 0 },
                     { 0 , 0 },
                     { 0 , 0 }};


void parse_line(char *serbuf)
{
    int parsed = 0;
    int port = -1;
    int ret = 0;
    uint8_t error = 0;

    if (serbuf[0] == 'P') {
        switch (serbuf[1]) {
        case '0':
            port = 0;
            break;
        case '1':
            port = 1;
            break;
        case '2':
            port = 2;
            break;
        case '3':
            port = 3;
            break;
        }

        if (serbuf[2] == ':') {
            lastport = port;
            if (strncmp(DISCONNECTED, serbuf + 4, strlen(DISCONNECTED)) == 0) {
                //parsed = 1;
                //ret = 1;
                DEBUG_PRINT("DISCONNECTING\n");
                if (port_detach_port(port) < 0) {
                    errno = EPROTO;
                    ret = -1;
                }
            }
            if (strncmp(CONNECTED, serbuf + 4, strlen(CONNECTED)) == 0) {
                DEBUG_PRINT("CONNECTING %s\n", serbuf + 4 + strlen(CONNECTED));

                uint8_t *hw = malloc(sizeof(uint8_t));
                uint8_t *fw = malloc(sizeof(uint8_t));
                uint16_t type_id =
                    strtol(serbuf + 4 + strlen(CONNECTED) - 1, NULL, 16);
                *hw = 0;
                *fw = 0;
                //parsed = 1;
                //ret = 1;
                if (port_attach_port(port, type_id, hw, fw) < 0) {
                    errno = EPROTO;
                    ret = -1;
                } else {
                   port_set_device_format(port,  ports[port][0] ,  ports[port][1] );
                }
            }
            if (strncmp(PULSEDONE, serbuf + 4, strlen(PULSEDONE)) == 0) {
                DEBUG_PRINT("Pulse done\n");
                parsed = 1;
                ret = 0;
            }
            if (strncmp(RAMPDONE, serbuf + 4, strlen(RAMPDONE)) == 0) {
                DEBUG_PRINT("Ramp done\n");
                parsed = 1;
                ret = 0;
            }
        } else if (serbuf[2] == '>') {
            parsed = 1;
        } else if (serbuf[2] == 'C' || serbuf[2] == 'M') {
            //int combiindex = serbuf[3] - 48;
            char *tmp = malloc((strlen(serbuf) - 5) + 1);
            memcpy(tmp, serbuf + 5, strlen(serbuf) - 5);
            tmp[strlen(serbuf) - 5] = 0;
            char * token = strtok(tmp, " ");
            int mcount = 0;
            while( token != NULL ) {
                data_t *tmpval = malloc(sizeof(data_t));
                if(strchr(token, '.') != NULL){
                    tmpval->i_data = strtof(token, NULL);
                    tmpval->t = FLOAT;
                } else {
                    tmpval->i_data = strtol(token, NULL, 10);
                    tmpval->t = INTEGER;
                }
                port_new_combi_value(port, mcount, tmpval);
                token = strtok(NULL, " ");
                mcount++;
            }
            callback_queue(CALLBACK_DEVICE, port, CALLBACK_DATA);
        }
    }
    if (strncmp(MODE, serbuf, strlen(MODE)) == 0) {
        int current = strtol(serbuf+strlen(MODE), NULL, 10);
        DEBUG_PRINT("cur mode: %d\n", current);
        lastmode = current;
    }
    if (strncmp(FORMAT, serbuf, strlen(FORMAT)) == 0) {
        char *type = strstr(serbuf+strlen(FORMAT),"type=");
        if(type != NULL){
            uint8_t current = (uint8_t)strtol(type+strlen("type="),NULL,10);
            DEBUG_PRINT("cur %d, %d %d\n", lastport, lastmode, current);
            ports[lastport][0] = lastmode;
            ports[lastport][1] = current;
        }
    }
    if (strncmp(ERROR, serbuf, strlen(ERROR)) == 0) {
        DEBUG_PRINT("Error occured\n");
        parsed = 1;
        error = TYPE_GENERIC_ERROR;
    }
    if (parsed) {
        DEBUG_PRINT("Sending message\n");
        uint8_t *buffer = malloc(10);
        memset(buffer, 0, 10);
        buffer[0] = 10;
        buffer[1] = 0;
        buffer[2] = error;
        buffer[3] = (uint8_t) port;
        if (ret < 0) {
            free(buffer);
            report_comms_error();
        } else if (ret > 0) {
            // Buffer should not be passed to foreground
            free(buffer);
        } else if (queue_return_buffer(buffer) < 0) {
            // Move buffer from UART Rx, to foreground queue, for python
            free(buffer);
            report_comms_error();
        }
    }
}


/* Thread function for background UART reception */
static void *run_comms_rx(void *args __attribute__((unused)))
{
    char buf[10];
    #define SERBUF_SIZE 300
    char serbuf[SERBUF_SIZE];
    int sercounter = 0;
    int nfds;
    int lfound = 0;

    struct epoll_event ev1, events[MAX_EVENTS];
    int epollfd;

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev1.events = EPOLLIN;
    ev1.data.fd = uart_fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, uart_fd, &ev1) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    ev1.events = EPOLLIN;
    ev1.data.fd = rx_event_fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, rx_event_fd, &ev1) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG_UART
    int debugfd = open("/tmp/serial.txt", O_RDWR | O_APPEND | O_CREAT, 0600);
    if (debugfd < 0){
        perror("serial debug file open failed!");
        exit(EXIT_FAILURE);
    }
#endif
    while (!shutdown)
    {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        int n;
        for (n = 0; n < nfds; ++n) {
            if ( events[n].data.fd == uart_fd){
                int rcount = read(events[n].data.fd, buf, sizeof buf);
#ifdef DEBUG_UART
                if (write(debugfd, buf, rcount) < 0){
                }
#endif
                if(sercounter+rcount > SERBUF_SIZE){
                    // Shouldn't happen, now that I increased buffer size!
                    sercounter = 0;
                }
                memcpy(serbuf+sercounter, buf, rcount);
                sercounter += rcount;

                for(int i=0; i<sercounter;i++){
                    if(serbuf[i] == '\n'){
                        serbuf[i] = 0;
                        serbuf[i-1] = 0;
                        parse_line(serbuf);
                        lfound = i + 1;
                        int tmp = 0;
                        for(tmp=0;tmp<sercounter;tmp++){
                            serbuf[tmp] = serbuf[lfound+tmp];
                        }
                        sercounter -= lfound;
                        i = 0;
                    }
                }
            }
        }
    }

    return NULL;
}


/* Thread function for background UART reception */
static void *run_comms_tx(void *args __attribute__((unused)))
{
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
            if (write(uart_fd, buffer, strlen((char*)buffer)) < 0){
                report_comms_error();
                free(buffer);
            }
        }
    }

    return NULL;
}


int i1ch()
{
    char c;
    if (read(uart_fd, &c, 1) == 1)
        return c;
    return -1;
}

// wait, with a timeout in milliseconds
// timeout==-1: never
int w1ch(int timeout)
{
    int c;
    for (;;) {
        c = i1ch();
        if (c != -1)
            return c;
        if (timeout > 0)
            timeout--;
        if (timeout == 0)
            return -1;
        usleep(1000);
    }
}

void och(int c)
{
    for (;;) {
        if (write(uart_fd, &c, 1) == 1)
            return;
        usleep(50);
    }
}

void wstr(char *s, int n, int timeout)
{
    int i, j;
    for (i = 0; i < n - 1;) {
        j = w1ch(timeout);
        if (j == -1)
            break;
        s[i] = j;
        if (s[i] == 0x0a)
            break;
        if (s[i] < 0x20)
            continue;
        i++;
    }
    s[i] = 0;
}

void ostr(char *s)
{
    int i;
    for (i = 0; s[i]; i++)
        och(s[i]);
}

unsigned int checksum(unsigned char *p, int l)
{
    unsigned int u = 1;
    int i;
    for (i = 0; i < l; i++) {
        if ((u & 0x80000000) != 0)
            u = (u << 1) ^ 0x1d872b41;
        else
            u = u << 1;
        u ^= p[i];
    }
    return u;
}

int getprompt()
{
    char s[100];
    int p = 0, t = 0, u = 0;
    for (;;) {
        wstr(s, 100, 10);
        if (strlen(s) == 0) {
            if (p == 1 && t == 10)
                return 1;       // wait for 10*timeout with no data
            if (p == 0 && t == 10) {    // no prompt, so send <RETURN>
                och(0x0d);
                t = 0;
                u++;
                if (u == 10)
                    return 0;
            }
            t++;
            continue;
        }
        if (!strcmp(s, "BHBL> "))
            p = 1;
        else {
            p = 0;
        }
        t = 0;
    }
}

int load_firmware(char *firmware_path, char *signature_path)
{
    char s[1000];
    int image_size;
    FILE *fp;
    int i;

    fp = fopen(firmware_path, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open image file\n");
        return -1;
    }
    image_size = fread(imagebuf, 1, IMAGEBUFSIZE + 1, fp);
    if (image_size < 1) {
        fprintf(stderr, "Error reading image file\n");
        return -1;
    }
    if (image_size > IMAGEBUFSIZE) {
        fprintf(stderr,
                "Image file is too large (maximum %d bytes)\n",
                IMAGEBUFSIZE);
        return -1;
    }
    fp=fopen(signature_path,"rb");
    if(!fp) {
        fprintf(stderr,"Failed to open signature file\n");
        return -1;
    }
    i=fread(signature,1,SIGSIZE,fp);
    if(i!=SIGSIZE) {
        fprintf(stderr,"Error reading signature file\n");
        return -1;
    }
    if (!getprompt())
        goto ew0;
    ostr("clear\r");
    if (!getprompt())
        goto ew0;
    sprintf(s, "load %d %u\r", image_size, checksum(imagebuf, image_size));
    ostr(s);
    usleep(100000);
    ostr("\x02");
    for (i = 0; i < image_size; i++)
        och(imagebuf[i]);
    ostr("\x03\r");
    if (!getprompt())
        goto ew0;
    sprintf(s,"signature %d\r",SIGSIZE);
    ostr(s);
    usleep(100000);
    ostr("\x02");
    for(i=0;i<SIGSIZE;i++)
        och(signature[i]);
    ostr("\x03\r");
    if(!getprompt())
        goto ew0;
    return 0;
 ew0:
    fprintf(stderr, "Failed to communicate with Build HAT\n");
    return -1;
}

/* Open the UART bus, select the Hat as the device to communicate with,
 * and return the file descriptor.  You must close the file descriptor
 * when you are done with it.
 */
int uart_open_hat(char *firmware_path, char *signature_path)
{
    int rv;
    struct termios ttyopt;

    if ((uart_fd = open(UART_DEVICE_NAME, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
    {
        if (errno == ENOENT)
        {
            /* "File not found" probably means UART is not enabled */
            PyErr_SetString(PyExc_IOError,
                            "Unable to access UART: has it been enabled?");
        }
        else
        {
            PyErr_SetFromErrno(PyExc_IOError);
        }
        return -1;
    }

    tcgetattr(uart_fd, &ttyopt);
    cfsetispeed(&ttyopt, BAUD);
    cfsetospeed(&ttyopt, BAUD);
    cfmakeraw(&ttyopt);
    tcsetattr(uart_fd, TCSANOW, &ttyopt);
    tcflush(uart_fd, TCIOFLUSH);

    //if (open_wake_gpio() < 0)
    //    return -1; /* Exception already raised */

    /* Reset the hat */
    if (uart_reset_hat() < 0)
    {
        /* Exception already raised */
        //close_wake_gpio();
        close(uart_fd);
        return -1;
    }

    if (load_firmware(firmware_path, signature_path) < 0){
        PyErr_SetString(PyExc_IOError, "Failed to load firmware");
        return -1;
    }

    /* Initialise thread work queues */
    if ((rv = queue_init()) != 0)
    {
        errno = rv;
        //close_wake_gpio();
        PyErr_SetFromErrno(PyExc_IOError);
        close(uart_fd);
        return -1;
    }

    /* Create the event for signalling to the receiver */
    if ((rx_event_fd = eventfd(0, 0)) == -1)
    {
        errno = rv;
        //close_wake_gpio();
        PyErr_SetFromErrno(PyExc_IOError);
        close(uart_fd);
        return -1;
    }

    /* Start the Rx and Tx threads */
    if (!PyEval_ThreadsInitialized())
        PyEval_InitThreads();
    if ((rv = pthread_create(&comms_rx_thread, NULL, run_comms_rx, NULL)) != 0)
    {
        errno = rv;
        //close_wake_gpio();
        close(rx_event_fd);
        PyErr_SetFromErrno(PyExc_IOError);
        close(uart_fd);
        return -1;
    }
    if ((rv = pthread_create(&comms_tx_thread, NULL, run_comms_tx, NULL)) != 0)
    {
        shutdown = 1;
        signal_rx_shutdown();
        pthread_join(comms_rx_thread, NULL);

        errno = rv;
        PyErr_SetFromErrno(PyExc_IOError);

        //close_wake_gpio();
        close(rx_event_fd);
        close(uart_fd);
        uart_fd = -1;
        return -1;
    }

    ostr("reboot\r");
    return uart_fd;
}

/* Close the connection to the Hat (so that others can access the UART
 * connector)
 */
int uart_close_hat(void)
{
    /* Kill comms thread */
    shutdown = 1;
    signal_rx_shutdown();
    queue_shutdown(); // Kicks the tx thread
    Py_BEGIN_ALLOW_THREADS
    pthread_join(comms_rx_thread, NULL);
    pthread_join(comms_tx_thread, NULL);
    Py_END_ALLOW_THREADS
    
    if (uart_fd != -1)
    {
        close(uart_fd);
        uart_fd = -1;
    }
    if (rx_event_fd != -1)
    {
        close(rx_event_fd);
        rx_event_fd = -1;
    }

    return 0;
}

