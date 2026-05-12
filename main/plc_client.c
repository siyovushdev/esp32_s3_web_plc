#include "plc_client.h"
#include "plc_gateway_state.h"
#include "plc_protocol.h"
#include "plc_uart_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#define PLC_UPLOAD_CHUNK_SIZE 512u

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

static esp_err_t plc_request_ack(uint8_t cmd,
                                 const uint8_t *body,
                                 uint16_t body_len,
                                 uint32_t timeout_ms)
{
    (void)timeout_ms;

    uint8_t rsp[64];
    uint16_t rsp_len = 0u;

    esp_err_t r = plc_request(cmd, body, body_len, rsp, sizeof(rsp), &rsp_len);
    if (r != ESP_OK) {
        return r;
    }

    if (rsp_len < 5u || rsp[1] != PLC_LINK_RSP_ACK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void plc_poll_task(void *arg)
{
    (void)arg;

    for (;;) {
        (void)plc_client_get_status_ext();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

uint32_t plc_client_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;

    if (data == NULL && len != 0u) {
        return 0u;
    }

    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

esp_err_t plc_client_init(void)
{
    plc_gateway_state_init();
    return plc_uart_client_init();
}

void plc_client_start_polling(void)
{
    xTaskCreatePinnedToCore(
        plc_poll_task,
        "plc_poll",
        4096,
        NULL,
        5,
        NULL,
        1
    );
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

    return plc_request_ack(PLC_LINK_CMD_FORCE_OUTPUT, body, sizeof(body), 1000u);
}

esp_err_t plc_client_release_output(uint16_t node_index)
{
    uint8_t body[2];
    plc_put_u16_le(body, node_index);

    return plc_request_ack(PLC_LINK_CMD_RELEASE_OUTPUT, body, sizeof(body), 1000u);
}

esp_err_t plc_client_safe_reset(void)
{
    return plc_request_ack(PLC_LINK_CMD_SAFE_RESET, NULL, 0u, 1000u);
}

esp_err_t plc_client_activate(void)
{
    return plc_request_ack(PLC_LINK_CMD_ACTIVATE, NULL, 0u, 2000u);
}

esp_err_t plc_client_upload_graph_image(const uint8_t *image,
                                        uint32_t image_size,
                                        uint32_t graph_version)
{
    if (image == NULL || image_size == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t begin_body[12];
    const uint32_t crc32 = plc_client_crc32(image, image_size);

    plc_put_u32_le(&begin_body[0], graph_version);
    plc_put_u32_le(&begin_body[4], image_size);
    plc_put_u32_le(&begin_body[8], crc32);

    esp_err_t r = plc_request_ack(PLC_LINK_CMD_UPLOAD_BEGIN, begin_body, sizeof(begin_body), 2000u);
    if (r != ESP_OK) {
        return r;
    }

    uint32_t offset = 0u;
    while (offset < image_size) {
        uint32_t left = image_size - offset;
        uint16_t chunk = (left > PLC_UPLOAD_CHUNK_SIZE) ? PLC_UPLOAD_CHUNK_SIZE : (uint16_t)left;

        uint8_t chunk_body[4u + PLC_UPLOAD_CHUNK_SIZE];
        plc_put_u32_le(&chunk_body[0], offset);
        memcpy(&chunk_body[4], &image[offset], chunk);

        r = plc_request_ack(PLC_LINK_CMD_UPLOAD_CHUNK, chunk_body, (uint16_t)(4u + chunk), 2000u);
        if (r != ESP_OK) {
            return r;
        }

        offset += chunk;
    }

    return plc_request_ack(PLC_LINK_CMD_UPLOAD_END, NULL, 0u, 3000u);
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

    return plc_request_ack(
        PLC_LINK_CMD_MEM_WRITE,
        body,
        (uint16_t)(8u + encoded_values_len),
        1000u
    );
}
