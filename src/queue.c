/* queue.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Functions to manage the foreground-to-I2C thread queue
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <errno.h>

#include "queue.h"

#ifdef DEBUG_I2C
#include "debug-i2c.h"
#endif

typedef struct queue_item_s
{
    struct queue_item_s *next;
    struct queue_item_s *prev;
    uint8_t *buffer;
} queue_item_t;


typedef struct queue_s
{
    queue_item_t *qhead;
    queue_item_t *qtail;
    pthread_mutex_t mutex;
    int eventfd;
} queue_t;

//typedef int (*wait_fn_t)(sem_t *semaphore);


static queue_t to_i2c_q;
static queue_t from_i2c_q;

static int shutdown;



static int init_queue(queue_t *q)
{
    int rv;

    shutdown = 0;
    q->qhead = q->qtail = NULL;
    if ((rv = pthread_mutex_init(&q->mutex, NULL)) != 0)
        return rv;
    if ((q->eventfd = eventfd(0, EFD_SEMAPHORE)) == -1)
        return errno;
    return 0;
}


int queue_init(void)
{
    int rv;

    if ((rv = init_queue(&to_i2c_q)) != 0)
        return rv;
    return init_queue(&from_i2c_q);
}


static int push(queue_t *q, queue_item_t *item)
{
    int rv;
    uint64_t dummy = 1;

    /* Lock the queue against the other thread */
    if ((rv = pthread_mutex_lock(&q->mutex)) != 0)
        return rv;
    if ((rv = write(q->eventfd, &dummy, 8)) < 0)
        return rv;
    else if (rv != 8)
    {
        errno = EPROTO;
        return -1;
    }

    /* Put the item at the head of the queue */
    item->prev = NULL;
    item->next = q->qhead;
    if (q->qhead == NULL)
        q->qtail = item;
    else
        q->qhead->prev = item;
    q->qhead = item;

    /* Let the other thread get a look in now */
    if ((rv = pthread_mutex_unlock(&q->mutex)) != 0)
    {
        /* Something has gone badly wrong, probably fatally.  Remove the
         * item from the queue to be consistent, but we are likely going
         * to die soon.
         */
        q->qhead = item->next;
        if (q->qhead == NULL)
            q->qtail = NULL;
        return errno;
    }

    return 0;
}


/* Foreground thread queueing for Tx thread */
int queue_add_buffer(uint8_t *buffer)
{
    queue_item_t *item;
    int rv;

    if ((item = malloc(sizeof(queue_item_t))) == NULL)
    {
        PyErr_NoMemory();
        return ENOMEM;
    }
    item->buffer = buffer;

    if ((rv = push(&to_i2c_q, item)) != 0)
    {
        free(item);
        errno = rv;
        PyErr_SetFromErrno(PyExc_OSError);
    }
    return rv;
}


/* Rx thread queue for foreground thread */
int queue_return_buffer(uint8_t *buffer)
{
    queue_item_t *item;
    int rv;

    if ((item = malloc(sizeof(queue_item_t))) == NULL)
        return ENOMEM;
    item->buffer = buffer;

    if ((rv = push(&from_i2c_q, item)) != 0)
        free(item);
    return rv;
}


static int get_item(queue_t *q, uint8_t **pbuffer, int timeout)
{
    int rv;
    queue_item_t *item;

    /* First check if there's already something on the queue */
    if ((rv = pthread_mutex_lock(&q->mutex)) != 0)
    {
#ifdef DEBUG_I2C
        uint8_t debug[4] = { 4, 0xff, 0x00 };
        debug[3] = (uint8_t)rv & 0xff;
        log_i2c(debug, -1);
#endif
        return rv;
    }

    while (!shutdown && q->qtail == NULL)
    {
        struct pollfd pfds[1];

        /* Nothing on the queue.  Release the lock and wait */
        if ((rv = pthread_mutex_unlock(&q->mutex)) != 0)
        {
#ifdef DEBUG_I2C
            uint8_t debug[4] = { 4, 0xff, 0x01 };
            debug[3] = (uint8_t)rv & 0xff;
            log_i2c(debug, -1);
#endif
            return rv;
        }

        pfds[0].fd = q->eventfd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        if ((rv = poll(pfds, 1, timeout)) < 0)
        {
#ifdef DEBUG_I2C
            uint8_t debug[4] = { 4, 0xff, 0x02 };
            debug[3] = (uint8_t)rv & 0xff;
            log_i2c(debug, -1);
#endif
            return rv;
        }
        else if (rv == 0 || shutdown)
        {
            /* Timeout (when permitted) or bailout */
            *pbuffer = NULL;
            return 0;
        }
        else
        {
            /* Read the event to clear it */
            uint64_t dummy;
            int rv;
            if ((rv = read(q->eventfd, (uint8_t *)&dummy, 8)) != 8)
                return rv;
        }

        /* Relock the mutex and see if that event was for something
         * we've already taken (or EINTR, which isn't really an error)
         */
        if ((rv = pthread_mutex_lock(&q->mutex)) != 0)
        {
#ifdef DEBUG_I2C
            uint8_t debug[4] = { 4, 0xff, 0x03 };
            debug[3] = (uint8_t)rv & 0xff;
            log_i2c(debug, -1);
#endif
            return rv;
        }
    }

    if (shutdown)
    {
        /* Should only be the transmitter */
        pthread_mutex_unlock(&q->mutex);
        *pbuffer = NULL;
        return 0;
    }

    /* We have the lock and there is something on the queue */
    item = q->qtail;
    q->qtail = item->prev;
    if (q->qtail == NULL)
        q->qhead = NULL;
    else
        q->qtail->next = NULL;

    /* Release the lock */
    if ((rv = pthread_mutex_unlock(&q->mutex)) != 0)
    {
#ifdef DEBUG_I2C
        uint8_t debug[4] = { 4, 0xff, 0x04 };
        debug[3] = (uint8_t)rv & 0xff;
        log_i2c(debug, -1);
#endif
        free(item->buffer);
        free(item);
        return rv;
    }

    /* Extract the buffer and free the queue object */
    *pbuffer = item->buffer;
    free(item);
    return 0;
}


/* Tx thread from foreground or Rx thread */
int queue_check(uint8_t **pbuffer)
{
    return get_item(&to_i2c_q, pbuffer, -1);
}


/* Foreground from Rx thread */
int queue_get(uint8_t **pbuffer)
{
    int rv;

    Py_BEGIN_ALLOW_THREADS
    rv = get_item(&from_i2c_q, pbuffer, 1000);
    Py_END_ALLOW_THREADS

    return rv;
}


/* Request from foreground to kill the transmitter wait */
void queue_shutdown(void)
{
    uint64_t value = 1;

    shutdown = 1;
    if (write(to_i2c_q.eventfd, (uint8_t *)&value, 8) != 8)
        fprintf(stderr, "Unable to kill transmitter thread\n");
}
