/* queue.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Functions to handling inter-thread buffer passing
 */


#ifndef RPI_STRAWBERRY_QUEUE_H_INCLUDED
#define RPI_STRAWBERRY_QUEUE_H_INCLUDED

#include <stdint.h>

/* Initialise the queueing system */
extern int queue_init(void);

/* Send a buffer to the comms thread
 *
 * NB: `buffer` must be dynamically allocated, and will be
 * freed if sent.  If not sent, it is the caller's responsibility.
 *
 * Returns 0 on success, a standard errno on failure.  On failure, a
 * Python exception will have been raised, so no further exception
 * handling is required.
 */
extern int queue_add_buffer(uint8_t *buffer);

/* Send a buffer to the forground thread (from comms)
 *
 * As for queue_add_buffer(), `buffer` must be dynamically allocated
 * and will be freed if sent.  If not sent, the caller must free it
 * when it is no longer wanted.
 *
 * Returns 0 on success, a standard errno on failure.
 */
extern int queue_return_buffer(uint8_t *buffer);

/* Check the queue to the comms thread for a buffer.
 *
 * Waits for up to 1ms for a buffer to be queued.  On a timeout,
 * returns success but sets `pbuffer` to NULL.
 *
 * Returns 0 on success, with `pbuffer` pointing to the received buffer.
 * On failure, returns a standard errno.
 */
extern int queue_check(uint8_t **pbuffer);

/* Wait on the queue from the comms thread for a buffer.
 *
 * Times out after a second if no buffer is queued.  Timeouts are
 * considered an error, returning ETIMEOUT.
 *
 * Returns 0 on success, with `pbuffer` pointing to the received buffer.
 * On failure, returns a standard errno and raises a Python exception.
 */
extern int queue_get(uint8_t **pbuffer);

/* Queue an item from the background thread
 *
 * Returns 0 on success and a standard errno on failure.
 */
extern int queue_add_buffer_background(uint8_t *buffer);

/* Check to see if something has been queued by the background
 *
 * Returns the buffer queued, or NULL if no buffer is waiting.
 */
extern uint8_t *queue_check_background(void);


#endif /* RPI_STRAWBERRY_QUEUE_H_INCLUDED */
