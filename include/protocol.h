/* protocol.h
 *
 * Copyright (c) Kynesim Ltd, 2020
 *
 * Constants and types for the Lego Wireless Protocol 3.0
 */

#ifndef RPI_STRAWBERRY_PROTOCOL_H_INCLUDED
#define RPI_STRAWBERRY_PROTOCOL_H_INCLUDED

/* XXX: these enums are incomplete.  Extend as necessary as
 * development ensues!
 */

/* Message types */
typedef enum protocol_type_e
{
    TYPE_HUB_PROPERTY = 1,
    TYPE_HUB_ACTION,
    TYPE_HUB_ALERT,
    TYPE_HUB_ATTACHED_IO,
    TYPE_GENERIC_ERROR,
    TYPE_PORT_INFO_REQ = 0x21,
    TYPE_PORT_MODE_REQ = 0x22,
    TYPE_PORT_FORMAT_SETUP_SINGLE = 0x41,
    TYPE_PORT_FORMAT_SETUP_COMBINED,
    TYPE_PORT_INFO,
    TYPE_PORT_MODE,
    TYPE_PORT_VALUE_SINGLE,
    TYPE_PORT_VALUE_COMBINED,
    TYPE_PORT_FORMAT_SINGLE,
    TYPE_PORT_FORMAT_COMBINED,
    TYPE_VIRTUAL_PORT_SETUP = 0x61,
    TYPE_PORT_OUTPUT = 0x81,
    TYPE_PORT_OUTPUT_FEEDBACK = 0x82
} protocol_type_t;

/* Hub Properties */
typedef enum protocol_property_e
{
    PROP_FW_VERSION = 3,
    PROP_HW_VERSION = 4
} protocol_property_t;

/* Hub Property Operations */
typedef enum protocol_prop_op_e
{
    PROP_OP_REQUEST = 5, /* Requests an update from Hat */
    PROP_OP_UPDATE = 6   /* Update from Hat */
} protocol_prop_op_t;

/* Port Information Types */
typedef enum protocol_port_info_e
{
    PORT_INFO_VALUE = 0,
    PORT_INFO_MODE,
    PORT_INFO_MODE_COMBINATIONS
} protocol_port_info_t;

/* Mode Information Types */
typedef enum protocol_mode_info_e
{
    MODE_INFO_NAME = 0,
    MODE_INFO_RAW,
    MODE_INFO_PCT,
    MODE_INFO_SI,
    MODE_INFO_SYMBOL,
    MODE_INFO_MAPPING,
    MODE_INFO_CAPABILITY = 8,
    MODE_INFO_FORMAT = 0x80
} protocol_mode_info_t;

typedef enum protocol_info_format_cmd_e
{
    INFO_FORMAT_SET_MODE_AND_DATASET = 1,
    INFO_FORMAT_LOCK,
    INFO_FORMAT_UNLOCK_AND_START_MULTI_UPDATE_ENABLED,
    INFO_FORMAT_UNLOCK_AND_START_MULTI_UPDATE_DISABLED,
    INFO_FORMAT_RESET = 6
} protocol_info_format_cmd_e;

typedef enum protocol_output_start_e
{
    OUTPUT_STARTUP_BUFFER = 0x00,
    OUTPUT_STARTUP_IMMEDIATE = 0x10
} protocol_output_start_t;

typedef enum protocol_output_complete_e
{
    OUTPUT_COMPLETE_QUIET = 0x00,
    OUTPUT_COMPLETE_STATUS = 0x01
} protocol_output_complete_t;

typedef enum protocol_output_cmd_e
{
    OUTPUT_CMD_START_POWER = 0x01,
    OUTPUT_CMD_START_POWER_2 = 0x02,
    OUTPUT_CMD_SET_ACC_TIME = 0x05,
    OUTPUT_CMD_SET_DEC_TIME = 0x06,
    OUTPUT_CMD_START_SPEED = 0x07,
    OUTPUT_CMD_START_SPEED_2 = 0x08,
    OUTPUT_CMD_START_SPEED_FOR_TIME = 0x09,
    OUTPUT_CMD_START_SPEED_2_FOR_TIME = 0x0a,
    OUTPUT_CMD_START_SPEED_FOR_DEGREES = 0x0b,
    OUTPUT_CMD_START_SPEED_2_FOR_DEGREES = 0x0c,
    OUTPUT_CMD_GOTO_ABS_POSITION = 0x0d,
    OUTPUT_CMD_GOTO_ABS_POSITION_2 = 0x0e,
    OUTPUT_CMD_PRESET_ENCODER = 0x13,
    OUTPUT_CMD_PRESET_ENCODER_2 = 0x14,
    OUTPUT_CMD_WRITE_DIRECT = 0x50,
    OUTPUT_CMD_WRITE_DIRECT_MODE_DATA = 0x51,
    OUTPUT_CMD_WRITE_PID = 0x52,
    OUTPUT_CMD_STALL_CONTROL = 0x54
} protocol_output_cmd_t;


#endif /* RPI_STRAWBERRY_PROTOCOL_H_INCLUDED */
