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
#include <semaphore.h>
#include <errno.h>

#include "queue.h"


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
    sem_t semaphore;
} queue_t;

typedef int (*wait_fn_t)(sem_t *semaphore);


static queue_t to_i2c_q;
static queue_t from_i2c_q;


static int init_queue(queue_t *q)
{
    int rv;

    q->qhead = q->qtail = NULL;
    if ((rv = pthread_mutex_init(&q->mutex, NULL)) != 0)
        return rv;
    if (sem_init(&q->semaphore, 0, 0) != 0)
        return errno;
    return 0;
}

static int push(queue_t *q, queue_item_t *item)
{
    int rv;

    /* Lock the queue against the other thread */
    if ((rv = pthread_mutex_lock(&q->mutex)) != 0)
        return rv;
    if (sem_post(&q->semaphore) < 0)
    {
        (void)pthread_mutex_unlock(&q->mutex);
        return errno;
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


int queue_init(void)
{
    int rv;

    if ((rv = init_queue(&to_i2c_q)) != 0)
        return rv;
    return init_queue(&from_i2c_q);
}


/* Called from foreground thread */
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


static int wait_on_sem_1ms(sem_t *semaphore)
{
    struct timespec timeout;

    if (clock_gettime(CLOCK_REALTIME, &timeout) < 0)
        return -1;

    timeout.tv_nsec += 1000000;
    if (timeout.tv_nsec >= 1000000000)
    {
        timeout.tv_sec++;
        timeout.tv_nsec -= 1000000000;
    }
    if (sem_timedwait(semaphore, &timeout) < 0)
    {
        if (errno == EINTR)
            return 0;
        else if (errno == ETIMEDOUT)
            return 1;
        return -1;
    }
    return 0;
}


static int wait_on_sem_1s(sem_t *semaphore)
{
    struct timespec timeout;

    if (clock_gettime(CLOCK_REALTIME, &timeout) < 0)
        return -1;

    timeout.tv_sec++;
    if (sem_timedwait(semaphore, &timeout) < 0)
        return (errno == EINTR) ? 0 : -1;
    return 0;
}


static int get_item(queue_t *q, uint8_t **pbuffer, wait_fn_t wait)
{
    queue_item_t *item;
    int rv;

    if ((rv = pthread_mutex_lock(&q->mutex)) != 0)
        return rv;

    while (q->qtail == NULL)
    {
        /* Release the lock, then wait for a semaphore */
        if ((rv = pthread_mutex_unlock(&q->mutex)) != 0)
            return rv; /* Not a good sign... */
        if ((rv = wait(&q->semaphore)) < 0)
            return errno;
        else if (rv != 0)
        {
            /* Timeout (when permitted) */
            *pbuffer = NULL;
            return 0;
        }
        /* Relock the mutex and see if that semaphore was for
         * something we already removed from the queue (or EINTR,
         * which isn't really an error).
         */
        if ((rv = pthread_mutex_lock(&q->mutex)) != 0)
            return rv;
    }

    /* We have the lock here and there is something on the queue */
    item = q->qtail;
    q->qtail = item->prev;
    if (q->qtail == NULL)
        q->qhead = NULL;
    else
        q->qtail->next = NULL;

    /* Release the lock */
    if ((rv = pthread_mutex_unlock(&q->mutex)) != 0)
    {
        free(item->buffer);
        free(item);
        return rv;
    }

    /* Extract the buffer and release the queue object */
    *pbuffer = item->buffer;
    free(item);
    return 0;
}


int queue_check(uint8_t **pbuffer)
{
    return get_item(&to_i2c_q, pbuffer, wait_on_sem_1ms);
}


int queue_get(uint8_t **pbuffer)
{
    int rv = get_item(&from_i2c_q, pbuffer, wait_on_sem_1s);

    if (rv != 0)
    {
        errno = rv;
        PyErr_SetFromErrno(PyExc_OSError);
    }
    return rv;
}
