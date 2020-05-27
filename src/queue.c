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

#include "debug-i2c.h"


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
    {
        pthread_mutex_unlock(&q->mutex);
        return rv;
    }
    else if (rv != 8)
    {
        pthread_mutex_unlock(&q->mutex);
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


/* Foreground clearing queue from Rx thread */
int queue_clear_responses(void)
{
    queue_item_t *item;
    int rv;

    if ((rv = pthread_mutex_lock(&from_i2c_q.mutex)) != 0)
    {
        errno = rv;
        PyErr_SetFromErrno(PyExc_OSError);
        return rv;
    }

    while (from_i2c_q.qtail != NULL)
    {
        item = from_i2c_q.qtail;
        from_i2c_q.qtail = item->prev;
        if (from_i2c_q.qtail == NULL)
            from_i2c_q.qhead = NULL;
        else
            from_i2c_q.qtail->next = NULL;
        free(item->buffer);
        free(item);
    }

    if ((rv = pthread_mutex_unlock(&from_i2c_q.mutex)) != 0)
    {
        errno = rv;
        PyErr_SetFromErrno(PyExc_OSError);
        return rv;
    }

    return 0;
}


/* Rx thread queue for foreground thread */
int queue_return_buffer(uint8_t *buffer)
{
    queue_item_t *item;
    int rv;

    if ((item = malloc(sizeof(queue_item_t))) == NULL)
        return ENOMEM;
    item->buffer = buffer;

    DEBUG0(QUEUE, RETURNING_BUFFER);
    if ((rv = push(&from_i2c_q, item)) != 0)
        free(item);
    DEBUG0(QUEUE, RETURNED_BUFFER);
    return rv;
}


static int get_item(queue_t *q, uint8_t **pbuffer, int timeout)
{
    int rv;
    queue_item_t *item;
#ifdef DEBUG_I2C
    int is_tx_thread = (timeout == -1);
#endif

    /* First check if there's already something on the queue */
    DEBUG1(QUEUE, GET_LOCKING, is_tx_thread);
    if ((rv = pthread_mutex_lock(&q->mutex)) != 0)
    {
        DEBUG0(QUEUE, GET_LOCK_FAILED);
        return rv;
    }

    while (!shutdown && q->qtail == NULL)
    {
        struct pollfd pfds[1];

        /* Nothing on the queue.  Release the lock and wait */
        if ((rv = pthread_mutex_unlock(&q->mutex)) != 0)
        {
            DEBUG0(QUEUE, GET_LOOP_UNLOCK_FAILED);
            return rv;
        }

        DEBUG1(QUEUE, GET_POLLING, is_tx_thread);
        pfds[0].fd = q->eventfd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        /* Release the GIL around the potentially long action */
        if (PyGILState_Check())
        {
            Py_BEGIN_ALLOW_THREADS
            rv = poll(pfds, 1, timeout);
            Py_END_ALLOW_THREADS
        }
        else
        {
            rv = poll(pfds, 1, timeout);
        }
        if (rv < 0)
        {
            DEBUG0(QUEUE, GET_POLL_FAILED);
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
        DEBUG1(QUEUE, GET_RELOCKING, is_tx_thread);
        if ((rv = pthread_mutex_lock(&q->mutex)) != 0)
        {
            DEBUG0(QUEUE, GET_LOOP_LOCK_FAILED);
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
    DEBUG1(QUEUE, GET_UNLOCKING, is_tx_thread);
    if ((rv = pthread_mutex_unlock(&q->mutex)) != 0)
    {
        DEBUG0(QUEUE, GET_UNLOCK_FAILED);
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
    return get_item(&from_i2c_q, pbuffer, 1000);
}


/* Request from foreground to kill the transmitter wait */
void queue_shutdown(void)
{
    uint64_t value = 1;

    shutdown = 1;
    if (write(to_i2c_q.eventfd, (uint8_t *)&value, 8) != 8)
        fprintf(stderr, "Unable to kill transmitter thread\n");
}
