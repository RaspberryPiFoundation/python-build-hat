/* callback.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Functions to handle callbacks from the receiver to Python code
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <errno.h>

#include "callback.h"
#include "port.h"
#include "pair.h"
#include "firmware.h"

typedef struct cb_queue_s
{
    struct cb_queue_s *next;
    struct cb_queue_s *prev;
    uint8_t type;
    uint8_t event;
    uint8_t port_id;
    PyObject *firmware;
} cb_queue_t;

static cb_queue_t *cb_q_head;
static cb_queue_t *cb_q_tail;
static pthread_mutex_t cb_mutex;
static int cb_event_fd;
static int shutdown;
static pthread_t callback_thread;


static int get_callback(cb_queue_t **pitem)
{
    struct pollfd pfds[1];

    /* First check if there's something already on the queue */
    if (pthread_mutex_lock(&cb_mutex) != 0)
        return -1;

    while (!shutdown && cb_q_tail == NULL)
    {
        /* Nothing on the queue and we haven't been told to quit yet.
         * Release the lock and wait for something.
         */
        if (pthread_mutex_unlock(&cb_mutex) != 0)
            return -1;

        pfds[0].fd = cb_event_fd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        if (poll(pfds, 1, -1) < 0) /* Wait forever */
            return -1;
        if (shutdown)
        {
            /* We have been told to bail out, so do that */
            *pitem = NULL;
            return 0;
        }
        else
        {
            /* Read the event to clear it */
            uint64_t dummy;
            if (read(cb_event_fd, (uint8_t *)&dummy, 8) != 8)
            {
                *pitem = NULL;
                return 0;
            }
        }

        /* Relock the mutex and see if that event was for something
         * we've already taken.
         */
        if (pthread_mutex_lock(&cb_mutex) != 0)
            return -1;
    }

    if (shutdown)
    {
        pthread_mutex_unlock(&cb_mutex);
        *pitem = NULL;
        return 0;
    }

    /* We have the lock and something is on the queue.  Remove it */
    *pitem = cb_q_tail;
    cb_q_tail = (*pitem)->prev;
    if (cb_q_tail == NULL)
        cb_q_head = NULL;
    else
        cb_q_tail->next = NULL;

    if (pthread_mutex_unlock(&cb_mutex) != 0)
    {
        /* Don't leak memory! */
        free(*pitem);
        *pitem = NULL;
        return -1;
    }

    return 0;
}


static void report_callback_error(void)
{
    /* XXX: Figure out something to do here */
}



/* Thread function for running callbacks */
static void *run_callbacks(void *arg __attribute__((unused)))
{
    int rv;
    cb_queue_t *item;

    while (!shutdown)
    {
        if ((rv = get_callback(&item)) < 0)
        {
            report_callback_error();
            continue;
        }
        if (item == NULL) /* Probably a shutdown */
            continue;

        switch (item->type)
        {
            case CALLBACK_PORT:
                if (port_handle_callback(item->port_id, item->event) < 0)
                    report_callback_error();
                break;

            case CALLBACK_MOTOR:
                if (port_handle_motor_callback(item->port_id,
                                               item->event) < 0)
                    report_callback_error();
                break;

            case CALLBACK_PAIR:
                if (pair_handle_callback(item->port_id, item->event) < 0)
                    report_callback_error();
                break;

            case CALLBACK_FIRMWARE:
                if (firmware_handle_callback(item->port_id,
                                             item->event,
                                             item->firmware) < 0)
                    report_callback_error();
                break;

            default:
                report_callback_error();
        }

        Py_XDECREF(item->firmware);
        free(item);
    }

    return NULL;
}


int callback_init(void)
{
    int rv;

    shutdown = 0;
    cb_q_head = cb_q_tail = NULL;
    if ((rv = pthread_mutex_init(&cb_mutex, NULL)) != 0)
    {
        errno = rv;
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }
    if ((cb_event_fd = eventfd(0, EFD_SEMAPHORE)) == -1)
    {
        PyErr_SetFromErrno(PyExc_IOError);
        return -1;
    }

    /* Ensure the Thread system is up and running */
    if (!PyEval_ThreadsInitialized())
        PyEval_InitThreads();

    /* Create the worker thread for handling callbacks */
    if ((rv = pthread_create(&callback_thread, NULL, run_callbacks, NULL)) != 0)
    {
        errno = rv;
        PyErr_SetFromErrno(PyExc_IOError);
        close(cb_event_fd);
        return -1;
    }

    return 0;
}


/* Shut down the callback queue processing */
int callback_finalize(void)
{
    uint64_t dummy = 3;
    int rv = 0;

    /* Kill the thread */
    shutdown = 1;
    if (write(cb_event_fd, (uint8_t *)&dummy, 8) < 0)
        rv = 1;
    /* Push on anyway if there is an error */
    pthread_join(callback_thread, NULL);

    return rv;
}


/* Called from the receiver thread only */
int callback_queue(uint8_t cb_type,
                   uint8_t port_id,
                   uint8_t event,
                   PyObject *firmware)
{
    cb_queue_t *item = malloc(sizeof(cb_queue_t));
    int rv;
    uint64_t dummy = 2;

    if (item == NULL)
        return -1;

    item->type = cb_type;
    item->port_id = port_id;
    item->event = event;
    Py_XINCREF(firmware);
    item->firmware = firmware;

    if (pthread_mutex_lock(&cb_mutex) != 0)
    {
        return -1;
    }
    if ((rv = write(cb_event_fd, &dummy, 8)) < 0)
    {
        pthread_mutex_unlock(&cb_mutex);
        return -1;
    }
    else if (rv != 8)
    {
        pthread_mutex_unlock(&cb_mutex);
        return -1;
    }

    /* Put the item at the head of the queue */
    item->prev = NULL;
    item->next = cb_q_head;
    if (cb_q_head == NULL)
        cb_q_tail = item;
    else
        cb_q_head->prev = item;
    cb_q_head = item;

    if (pthread_mutex_unlock(&cb_mutex) < 0)
    {
        return -1;
    }

    return 0;
}
