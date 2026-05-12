#include "plc_frame.h"

#include <string.h>

static uint16_t get_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static void put_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8u) & 0xFFu);
}

uint16_t plc_crc16_modbus(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;

    if (data == NULL && len != 0u) {
        return crc;
    }

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            if ((crc & 0x0001u) != 0u) {
                crc = (uint16_t)((crc >> 1u) ^ 0xA001u);
            } else {
                crc = (uint16_t)(crc >> 1u);
            }
        }
    }

    return crc;
}

plc_frame_result_t plc_frame_build(const uint8_t *payload,
                                   uint16_t payload_len,
                                   uint8_t *out_frame,
                                   size_t out_cap,
                                   uint16_t *out_frame_len)
{
    if (payload == NULL || out_frame == NULL || out_frame_len == NULL) {
        return PLC_FRAME_ERR_ARG;
    }

    if (payload_len == 0u || payload_len > PLC_FRAME_MAX_PAYLOAD) {
        return PLC_FRAME_ERR_SIZE;
    }

    const uint16_t frame_len = (uint16_t)(PLC_FRAME_LEN_SIZE + payload_len + PLC_FRAME_CRC_SIZE);
    if (out_cap < frame_len) {
        return PLC_FRAME_ERR_SIZE;
    }

    put_u16_le(&out_frame[0], payload_len);
    memcpy(&out_frame[PLC_FRAME_LEN_SIZE], payload, payload_len);

    const uint16_t crc = plc_crc16_modbus(out_frame, PLC_FRAME_LEN_SIZE + payload_len);
    put_u16_le(&out_frame[PLC_FRAME_LEN_SIZE + payload_len], crc);

    *out_frame_len = frame_len;
    return PLC_FRAME_OK;
}

plc_frame_result_t plc_frame_try_parse(const uint8_t *buf,
                                       size_t len,
                                       uint8_t *out_payload,
                                       size_t out_cap,
                                       uint16_t *out_payload_len,
                                       size_t *consumed)
{
    if (buf == NULL || out_payload == NULL || out_payload_len == NULL || consumed == NULL) {
        return PLC_FRAME_ERR_ARG;
    }

    *consumed = 0u;
    *out_payload_len = 0u;

    if (len < PLC_FRAME_LEN_SIZE) {
        return PLC_FRAME_ERR_INCOMPLETE;
    }

    const uint16_t payload_len = get_u16_le(buf);
    if (payload_len == 0u || payload_len > PLC_FRAME_MAX_PAYLOAD || payload_len > out_cap) {
        *consumed = PLC_FRAME_LEN_SIZE;
        return PLC_FRAME_ERR_SIZE;
    }

    const size_t frame_len = PLC_FRAME_LEN_SIZE + payload_len + PLC_FRAME_CRC_SIZE;
    if (len < frame_len) {
        return PLC_FRAME_ERR_INCOMPLETE;
    }

    const uint16_t expected = get_u16_le(&buf[PLC_FRAME_LEN_SIZE + payload_len]);
    const uint16_t actual = plc_crc16_modbus(buf, PLC_FRAME_LEN_SIZE + payload_len);

    *consumed = frame_len;

    if (expected != actual) {
        return PLC_FRAME_ERR_CRC;
    }

    memcpy(out_payload, &buf[PLC_FRAME_LEN_SIZE], payload_len);
    *out_payload_len = payload_len;
    return PLC_FRAME_OK;
}
