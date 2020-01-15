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


#endif /* RPI_STRAWBERRY_PROTOCOL_H_INCLUDED */
