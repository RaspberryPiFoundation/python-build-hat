/* debug-i2c.c
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Debug logging for the I2C module
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "debug-i2c.h"


typedef struct debug_log_s
{
    struct debug_log_s *next;
    clock_t timestamp;
    int direction;
    size_t nbytes;
    uint8_t *buffer;
} debug_log_t;

static debug_log_t *debug_log_head;
static debug_log_t *debug_log_tail;
static pthread_mutex_t mutex;
static int initialised = 0;
static int error_count = 0;


int log_i2c_init(void)
{
    int rv;

    debug_log_head = debug_log_tail = NULL;
    if ((rv = pthread_mutex_init(&mutex, NULL)) != 0)
    {
        fprintf(stderr,
                "Failed to initialise I2C debug log: %s\n",
                strerror(rv));
        return -1;
    }
    initialised = 1;
    return 0;
}


void log_i2c(uint8_t *buffer, int direction)
{
    debug_log_t *item;
    size_t nbytes;

    if (!initialised)
        return;

    nbytes = buffer[0];
    if (nbytes >= 0x80)
        nbytes = (nbytes & 0x7f) | (buffer[1] << 7);

    if ((item = malloc(sizeof(debug_log_t))) == NULL)
    {
        error_count++;
        return;
    }
    if ((item->buffer = malloc(nbytes)) == NULL)
    {
        free(item);
        error_count++;
        return;
    }
    item->next = NULL;
    item->timestamp = clock();
    item->direction = direction;
    item->nbytes = nbytes;
    memcpy(item->buffer, buffer, nbytes);

    if (pthread_mutex_lock(&mutex) != 0)
    {
        /* Can't safely access the queue */
        free(item->buffer);
        free(item);
        error_count++;
        return;
    }
    if (debug_log_head == NULL)
    {
        debug_log_head = debug_log_tail = item;
    }
    else
    {
        debug_log_tail->next = item;
        debug_log_tail = item;
    }
    pthread_mutex_unlock(&mutex);
}


void log_i2c_dump(void)
{
    debug_log_t *item;
    int rv;
    size_t i;

    if (!initialised)
    {
        fprintf(stderr, "I2C debug log not initialised\n");
        return;
    }

    if ((rv = pthread_mutex_lock(&mutex)) != 0)
    {
        fprintf(stderr, "Error locking debug queue: %s\n", strerror(rv));
        return;
    }
    while (debug_log_head != NULL)
    {
        item = debug_log_head;
        debug_log_head = item->next;
        pthread_mutex_unlock(&mutex);

        printf("%12.2f %c",
               1000.0 * item->timestamp / CLOCKS_PER_SEC,
               (item->direction < 0) ? '!' :
               item->direction ? '>' : '<');
        for (i = 0; i < item->nbytes; i++)
        {
            printf(" %02x", item->buffer[i]);
        }
        printf("\n");

        free(item->buffer);
        free(item);

        if ((rv = pthread_mutex_lock(&mutex)) != 0)
        {
            fprintf(stderr,
                    "Error locking debug queue: %s\n",
                    strerror(rv));
            return;
        }
    }
    pthread_mutex_unlock(&mutex);
}
