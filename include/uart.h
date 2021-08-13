/* uart.h
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
 * Functions for handling UART comms
 */

#ifndef RPI_STRAWBERRY_UART_H_INCLUDED
#define RPI_STRAWBERRY_UART_H_INCLUDED

/* Initialise the UART subsystem, creating the background thread that
 * communicates with Shortcake.  Returns the file descriptor of the
 * (open) UART bus, or -1 on error.  On failure, a Python exception
 * will have been raised.
 */
extern int uart_open_hat(char *firmware_path, char *signature_path, long version);

/* Finalise the UART subsystem, closing the file descriptor.  Returns 0
 * for success, -1 on failure.  If the close call fails, an exception
 * will be raised.
 */
extern int uart_close_hat(void);

/* Register the firmware object with the UART subsystem so it knows
 * where to direct firmware callbacks
 */
extern void uart_register_firmware_object(PyObject *firmware);

/* Pulses the reset line to the HAT, with the BOOT0 pin held low to
 * boot into firmware rather than the embedded bootloader.  Note that
 * this is AN EXTREMELY DANGEROUS THING TO DO: you must be sure no UART
 * communications are in progress, otherwise the library's comms will
 * break.
 */
extern int uart_reset_hat(void);

/* Checks to see if there has been an error on the comms background
 * threads.  Returns 0 if all is well, otherwise returns -1 and raises
 * an appropriate exception.
 */
extern int uart_check_comms_error(void);

#ifdef DEBUG_UART
#define DEBUG_PRINT(...) { fprintf( stderr, __VA_ARGS__ ); fflush(stderr);  }
#else
#define DEBUG_PRINT(...) do {} while ( false )
#endif


#endif /* RPI_STRAWBERRY_UART_H_INCLUDED */
