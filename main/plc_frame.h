#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PLC_FRAME_MAX_PAYLOAD 1024u
#define PLC_FRAME_LEN_SIZE 2u
#define PLC_FRAME_CRC_SIZE 2u
#define PLC_FRAME_MAX_WIRE_SIZE (PLC_FRAME_LEN_SIZE + PLC_FRAME_MAX_PAYLOAD + PLC_FRAME_CRC_SIZE)

typedef enum {
    PLC_FRAME_OK = 0,
    PLC_FRAME_ERR_ARG,
    PLC_FRAME_ERR_SIZE,
    PLC_FRAME_ERR_CRC,
    PLC_FRAME_ERR_INCOMPLETE
} plc_frame_result_t;

uint16_t plc_crc16_modbus(const uint8_t *data, size_t len);

plc_frame_result_t plc_frame_build(const uint8_t *payload,
                                   uint16_t payload_len,
                                   uint8_t *out_frame,
                                   size_t out_cap,
                                   uint16_t *out_frame_len);

plc_frame_result_t plc_frame_try_parse(const uint8_t *buf,
                                       size_t len,
                                       uint8_t *out_payload,
                                       size_t out_cap,
                                       uint16_t *out_payload_len,
                                       size_t *consumed);
