#pragma once

#include <stdint.h>

#define PLC_LINK_PROTO_VERSION 1u
#define PLC_LINK_MAX_BODY_SIZE 1018u
#define PLC_LINK_MAX_PAYLOAD_SIZE 1024u

#define PLC_RSP_BODY_MAX PLC_LINK_MAX_BODY_SIZE

typedef enum {
    PLC_LINK_CMD_PING = 0x01,
    PLC_LINK_CMD_GET_STATUS = 0x02,
    PLC_LINK_CMD_GET_STATUS_EXT = 0x03,
    PLC_LINK_CMD_GET_NODE = 0x04,

    PLC_LINK_CMD_FORCE_OUTPUT = 0x05,
    PLC_LINK_CMD_RELEASE_OUTPUT = 0x06,

    PLC_LINK_CMD_MEM_INFO = 0x07,
    PLC_LINK_CMD_MEM_READ = 0x08,
    PLC_LINK_CMD_MEM_WRITE = 0x09,

    PLC_LINK_CMD_UPLOAD_BEGIN = 0x10,
    PLC_LINK_CMD_UPLOAD_CHUNK = 0x11,
    PLC_LINK_CMD_UPLOAD_END = 0x12,
    PLC_LINK_CMD_ACTIVATE = 0x13,

    PLC_LINK_CMD_SAFE_RESET = 0x20,

    PLC_LINK_RSP_ACK = 0x80,
    PLC_LINK_RSP_ERROR = 0x81,
    PLC_LINK_RSP_STATUS = 0x82,
    PLC_LINK_RSP_LOG = 0x83,
    PLC_LINK_RSP_STATUS_EXT = 0x84,
    PLC_LINK_RSP_NODE = 0x85,
    PLC_LINK_RSP_MEM_INFO = 0x86,
    PLC_LINK_RSP_MEM_READ = 0x87,
} plc_link_cmd_t;

typedef enum {
    PLC_LINK_ERR_OK = 0,
    PLC_LINK_ERR_BAD_SIZE = 1,
    PLC_LINK_ERR_BAD_VERSION = 2,
    PLC_LINK_ERR_UNKNOWN_CMD = 3,
    PLC_LINK_ERR_GRAPH_LOADER = 4,
    PLC_LINK_ERR_TX = 5,
    PLC_LINK_ERR_PERSIST = 6,
    PLC_LINK_ERR_PERSIST_BUSY = 7,
    PLC_LINK_ERR_BAD_INDEX = 8,
    PLC_LINK_ERR_NOT_FOUND = 9,
    PLC_LINK_ERR_SAFE_RESET_FAILED = 10,
    PLC_LINK_ERR_BAD_MEM_TYPE = 11,
    PLC_LINK_ERR_BAD_MEM_RANGE = 12,
    PLC_LINK_ERR_BAD_MEM_WRITE = 13,
} plc_link_error_t;

typedef enum {
    PLC_MEM_TYPE_BOOL = 0,
    PLC_MEM_TYPE_INT = 1,
    PLC_MEM_TYPE_REAL = 2,
} plc_mem_type_t;

static inline uint16_t plc_get_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static inline uint32_t plc_get_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) |
           ((uint32_t)p[3] << 24u);
}

static inline void plc_put_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8u) & 0xFFu);
}

static inline void plc_put_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8u) & 0xFFu);
    p[2] = (uint8_t)((v >> 16u) & 0xFFu);
    p[3] = (uint8_t)((v >> 24u) & 0xFFu);
}
