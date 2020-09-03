/* queue.h
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
 * Functions to handling inter-thread buffer passing
 */


#ifndef RPI_STRAWBERRY_QUEUE_H_INCLUDED
#define RPI_STRAWBERRY_QUEUE_H_INCLUDED

#include <stdint.h>

/* Initialise the queueing system */
extern int queue_init(void);

/* Send a buffer to the comms Tx thread
 *
 * NB: `buffer` must be dynamically allocated, and will be
 * freed if sent.  If not sent, it is the caller's responsibility.
 *
 * Returns 0 on success, a standard errno on failure.  On failure, a
 * Python exception will have been raised, so no further exception
 * handling is required.
 */
extern int queue_add_buffer(uint8_t *buffer);

/* Discard all the packets currently queued to be read by the
 * foreground (presumably because they are all stale somehow).
 *
 * Returns 0 on success, a standard errno on failure.  On failure, a
 * Python exception will have been raised, so no further exception
 * handling is required.
 */
extern int queue_clear_responses(void);

/* Send a buffer to the forground thread (from comms Rx thread)
 *
 * As for queue_add_buffer(), `buffer` must be dynamically allocated
 * and will be freed if sent.  If not sent, the caller must free it
 * when it is no longer wanted.
 *
 * Returns 0 on success, a standard errno on failure.
 */
extern int queue_return_buffer(uint8_t *buffer);

/* Check the queue to the comms Tx thread for a buffer.
 *
 * Waits indefinitely for a buffer to be queued.
 *
 * Returns 0 on success, with `pbuffer` pointing to the received buffer.
 * On failure, returns a standard errno.  If the wait was interrupted
 * (by a call to queue_shutdown() for instance), the function returns
 * success but `pbuffer` will be NULL.
 */
extern int queue_check(uint8_t **pbuffer);

/* Wait on the queue from the comms Rx thread for a buffer.
 *
 * Times out after a second if no buffer is queued.  Timeouts are
 * considered an error, returning ETIMEOUT.
 *
 * Returns 0 on success, with `pbuffer` pointing to the received buffer.
 * On failure, returns a standard errno and raises a Python exception.
 */
extern int queue_get(uint8_t **pbuffer);

/* Wait patiently on the queue from the comms Rx thread for a buffer.
 *
 * Times out after 10 seconds, so only used when very long delays are
 * expected.  Timeouts are considered an error, returning ETIMEOUT.
 *
 * Returns 0 on success, with `pbuffer` pointing to the received buffer.
 * On failure, returns a standard errno and raises a Python exception.
 */
extern int queue_get_delayed(uint8_t **pbuffer);

/* Send the Tx thread a message to terminate */
extern void queue_shutdown(void);


#endif /* RPI_STRAWBERRY_QUEUE_H_INCLUDED */
