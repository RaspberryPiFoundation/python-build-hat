/* cmd.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Functions implementing "wireless protocol" commands
 */

#ifndef RPI_STRAWBERRY_CMD_H_INCLUDED
#define RPI_STRAWBERRY_CMD_H_INCLUDED

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdint.h>

/* Return type for cmd_get_port_modes() */
typedef struct port_modes_s
{
    uint8_t capabilities;  /* CAP_ flags (see below) */
    uint8_t count;         /* Total number of port modes */
    uint16_t input_mode_mask;
    uint16_t output_mode_mask;
} port_modes_t;

#define CAP_MODE_HAS_OUTPUT     0x01
#define CAP_MODE_HAS_INPUT      0x02
#define CAP_MODE_COMBINABLE     0x04
#define CAP_MODE_SYNCHRONIZABLE 0x08

typedef uint16_t combi_mode_t[8];

typedef struct value_format_s
{
    uint8_t datasets;
    uint8_t type;
    uint8_t figures;
    uint8_t decimals;
} value_format_t;

#define FORMAT_8BIT  0x00
#define FORMAT_16BIT 0x01
#define FORMAT_32BIT 0x02
#define FORMAT_FLOAT 0x03


/* Initialises the command subsystem, creating the exception used for
 * protocol errors.  Returns 0 for success, -1 for error.
 */
extern int cmd_modinit(PyObject *hub);


/* Finalise the command subsystem */
extern void cmd_demodinit(void);


/* Return the hub_protocol_error exception object, as an alternative
 * to having a global running around.
 */
extern PyObject *cmd_get_exception(void);


/* Convert version data encoded in a buffer into a Python string */
extern PyObject *cmd_version_as_unicode(uint8_t *buffer);

/* Sends a Hub Property Request Update command for the HW Version
 * property and waits for the Hub Property Update in response.  The
 * version is converted into a Python string of the form "M.m.BB.bbbb"
 */
extern PyObject *cmd_get_hardware_version(void);

/* Sends a Hub Property Request Update command for the FW Version
 * property and waits for the Hub Property Update in response.  The
 * version is converted into a Python string of the form "M.m.BB.bbbb"
 */
extern PyObject *cmd_get_firmware_version(void);

/* Sends a Port Information Request command for the value and waits
 * for the Port Value (Single or Combination) response.  The values
 * will be inserted into the device structure.  Returns 0 on success
 * (and the device->values entry will be updated) or -1 on error (when
 * an exception will already be raised).
 */
extern int cmd_get_port_value(uint8_t port_id);

/* Sends a Hub Port Info Request command for the mode info of the
 * given port.  Fills in the port_modes_t structure with the returned
 * information, returning 0 on success or -1 on error (when an
 * exception will already be raised).
 */
extern int cmd_get_port_modes(uint8_t port_id, port_modes_t *results);

/* Sends a Hub Port Info Request command for the combination mode
 * info of the given port.  Returns the combinations in the
 * combi_mode_t passed in; up to 8 entries, the list being terminated
 * by a zero or running out of entries.  Returns 0 on success, -1 on
 * error (when an exception will already be raised).
 */
extern int cmd_get_combi_modes(uint8_t port_id, combi_mode_t combi);

/* Sends a Port Mode Information Request command for the name of a
 * given mode on a given port.  Return the name as NUL-terminated
 * string in `name`, which is expected to be a preallocated string
 * with space for up to 11 characters plus the NUL.  Returns 0 on
 * success, -1 on error (when an exception will already be raised).
 */
extern int cmd_get_mode_name(uint8_t port_id, uint8_t mode_id, char *name);

/* Sends a Port Mode Information Request command for the minimum and
 * maximum values of the raw data.  Values are returned through the
 * float pointers passed in.  Return 0 on success, -1 on error (when
 * a Python exception will have been raised).
 */
extern int cmd_get_mode_raw(uint8_t port_id,
                            uint8_t mode_id,
                            float *pmin,
                            float *pmax);

/* Sends a Port Mode Information Request command for the minimum and
 * maximum percentage values of the data.  Values are returned through
 * the float pointers passed in.  Return 0 on success, -1 on error
 * (when a Python exception will have been raised).
 */
extern int cmd_get_mode_percent(uint8_t port_id,
                                uint8_t mode_id,
                                float *pmin,
                                float *pmax);

/* Sends a Port Mode Information Request command for the minimum and
 * maximum SI values of the data.  Values are returned through the
 * float pointers passed in.  Return 0 on success, -1 on error (when a
 * Python exception will have been raised).
 */
extern int cmd_get_mode_si(uint8_t port_id,
                           uint8_t mode_id,
                           float *pmin,
                           float *pmax);

/* Sends a Port Mode Information Request command for the name of the
 * units symbol of the data.  Returns the symbol as NUL-terminated
 * string in `symbol`, which is expected to be a preallocated string
 * with space for up to 5 characters plus the NUL.  Returns 0 on
 * success, -1 on error (when an exception will already be raised).
 */
extern int cmd_get_mode_symbol(uint8_t port_id, uint8_t mode_id, char *symbol);

/* Sends a Port Mode Information Request command for the input and
 * output mappings of modes.  Returns the values through the byte
 * pointers passed in.  Return 0 on success, -1 on error (when a
 * Python exception will have been raised).
 */
extern int cmd_get_mode_mapping(uint8_t port_id,
                                uint8_t mode_id,
                                uint8_t *pinput_mapping,
                                uint8_t *poutput_mapping);

/* Sends a Port Mode Information Request command for the capability
 * bytes of a mode.  Returns the value in the six byte buffer
 * provided.  Returns 0 on success, -1 on error (when a Python
 * exception will have been raised).
 */
extern int cmd_get_mode_capability(uint8_t port_id,
                                   uint8_t mode_id,
                                   uint8_t capability[6]);


/* Sends a Port Mode Information Request command for the format of the
 * data presented in this mode.  Returns the value in the `format`
 * parameter.  Returns 0 on success, -1 on error (when a Python
 * exception will have been raised).
 */
extern int cmd_get_mode_format(uint8_t port_id,
                               uint8_t mode_id,
                               value_format_t *format);

/* Sends a Port Output command to set the start power of the given
 * port.  Returns 0 on success, -1 on error (when a Python exception
 * will have been raised).
 */
extern int cmd_set_pwm(uint8_t port_id, int8_t pwm);

/* Sends a Port Output command to set start or hold a motor.  Returns
 * 0 on success, -1 on error (when a Python exception will have been
 * raised).
 */
extern int cmd_start_speed(uint8_t port_id,
                           int8_t speed,
                           uint8_t max_power,
                           uint8_t use_profile);

/* Sends a Port Write Direct Mode Data command, writing a byte stream
 * directly to the target device.  Returns 0 on success, -1 on error
 * (when a Python exception will have been raised).
 */
extern int cmd_write_mode_data(uint8_t port_id,
                               uint8_t mode,
                               ssize_t nbytes,
                               const char *bytes);

/* Sends a Port Input Format Setup command (possibly more, depending)
 * to set the mode used on the given port.  Returns 0 on success, -1
 * on error (when a Python exception will have been raised already).
 */
extern int cmd_set_mode(uint8_t port_id, uint8_t mode);

#endif /* RPI_STRAWBERRY_CMD_H_INCLUDED */
