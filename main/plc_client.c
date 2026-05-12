#include "plc_client.h"
#include "plc_gateway_state.h"
#include "plc_protocol.h"
#include "plc_uart_client.h"

#include <string.h>

static uint16_t s_seq = 1u;

static esp_err_t plc_request(uint8_t cmd,
                             const uint8_t *body,
                             uint16_t body_len,
                             uint8_t *rsp,
                             size_t rsp_cap,
                             uint16_t *rsp_len)
{
    uint8_t payload[PLC_LINK_MAX_PAYLOAD_SIZE];

    payload[0] = PLC_LINK_PROTO_VERSION;
    payload[1] = cmd;

    plc_put_u16_le(&payload[2], s_seq++);

    if (body != NULL && body_len != 0u) {
        memcpy(&payload[4], body, body_len);
    }

    return plc_uart_client_request(
        payload,
        (uint16_t)(4u + body_len),
        rsp,
        rsp_cap,
        rsp_len,
        1000u
    );
}

esp_err_t plc_client_init(void)
{
    plc_gateway_state_init();
    return plc_uart_client_init();
}

esp_err_t plc_client_get_status_ext(void)
{
    uint8_t rsp[PLC_LINK_MAX_PAYLOAD_SIZE];
    uint16_t rsp_len = 0u;

    esp_err_t r = plc_request(
        PLC_LINK_CMD_GET_STATUS_EXT,
        NULL,
        0u,
        rsp,
        sizeof(rsp),
        &rsp_len
    );

    if (r != ESP_OK) {
        plc_gateway_state_set_connected(false);
        return r;
    }

    if (rsp_len < 4u || rsp[1] != PLC_LINK_RSP_STATUS_EXT) {
        return ESP_FAIL;
    }

    plc_gateway_state_update_from_status_ext(&rsp[4], (uint16_t)(rsp_len - 4u));
    return ESP_OK;
}

esp_err_t plc_client_get_node(uint16_t node_index,
                              uint8_t *rsp,
                              size_t rsp_cap,
                              uint16_t *rsp_len)
{
    uint8_t body[2];
    plc_put_u16_le(body, node_index);

    return plc_request(
        PLC_LINK_CMD_GET_NODE,
        body,
        sizeof(body),
        rsp,
        rsp_cap,
        rsp_len
    );
}

esp_err_t plc_client_force_output(uint16_t node_index,
                                  bool value,
                                  uint32_t hold_ms)
{
    uint8_t body[10];
    memset(body, 0, sizeof(body));

    plc_put_u16_le(&body[0], node_index);
    plc_put_u32_le(&body[2], value ? 1u : 0u);
    plc_put_u32_le(&body[6], hold_ms);

    uint8_t rsp[64];
    uint16_t rsp_len = 0u;

    return plc_request(
        PLC_LINK_CMD_FORCE_OUTPUT,
        body,
        sizeof(body),
        rsp,
        sizeof(rsp),
        &rsp_len
    );
}

esp_err_t plc_client_release_output(uint16_t node_index)
{
    uint8_t body[2];
    plc_put_u16_le(body, node_index);

    uint8_t rsp[64];
    uint16_t rsp_len = 0u;

    return plc_request(
        PLC_LINK_CMD_RELEASE_OUTPUT,
        body,
        sizeof(body),
        rsp,
        sizeof(rsp),
        &rsp_len
    );
}

esp_err_t plc_client_mem_info(uint8_t *rsp,
                              size_t rsp_cap,
                              uint16_t *rsp_len)
{
    return plc_request(
        PLC_LINK_CMD_MEM_INFO,
        NULL,
        0u,
        rsp,
        rsp_cap,
        rsp_len
    );
}

esp_err_t plc_client_mem_read(uint8_t mem_type,
                              uint16_t index,
                              uint16_t count,
                              uint8_t *rsp,
                              size_t rsp_cap,
                              uint16_t *rsp_len)
{
    uint8_t body[8];
    memset(body, 0, sizeof(body));

    body[0] = mem_type;
    plc_put_u16_le(&body[2], index);
    plc_put_u16_le(&body[4], count);

    return plc_request(
        PLC_LINK_CMD_MEM_READ,
        body,
        sizeof(body),
        rsp,
        rsp_cap,
        rsp_len
    );
}

esp_err_t plc_client_mem_write(uint8_t mem_type,
                               uint16_t index,
                               uint16_t count,
                               const uint8_t *encoded_values,
                               uint16_t encoded_values_len)
{
    uint8_t body[PLC_LINK_MAX_BODY_SIZE];
    memset(body, 0, sizeof(body));

    body[0] = mem_type;
    plc_put_u16_le(&body[2], index);
    plc_put_u16_le(&body[4], count);

    if (encoded_values != NULL && encoded_values_len != 0u) {
        memcpy(&body[8], encoded_values, encoded_values_len);
    }

    uint8_t rsp[64];
    uint16_t rsp_len = 0u;

    return plc_request(
        PLC_LINK_CMD_MEM_WRITE,
        body,
        (uint16_t)(8u + encoded_values_len),
        rsp,
        sizeof(rsp),
        &rsp_len
    );
}
