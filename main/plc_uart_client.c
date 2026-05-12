#include "plc_uart_client.h"
#include "plc_frame.h"

#include "driver/uart.h"
#include "esp_log.h"

#include <string.h>

#define PLC_UART_PORT UART_NUM_2
#define PLC_UART_TX_PIN GPIO_NUM_17
#define PLC_UART_RX_PIN GPIO_NUM_18
#define PLC_UART_BAUD 115200

static const char *TAG = "plc_uart";

static plc_uart_stats_t s_stats;

esp_err_t plc_uart_client_init(void)
{
    const uart_config_t cfg = {
        .baud_rate = PLC_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(
        PLC_UART_PORT,
        PLC_UART_RX_BUF_SIZE,
        PLC_UART_TX_BUF_SIZE,
        0,
        NULL,
        0
    ));

    ESP_ERROR_CHECK(uart_param_config(PLC_UART_PORT, &cfg));

    ESP_ERROR_CHECK(uart_set_pin(
        PLC_UART_PORT,
        PLC_UART_TX_PIN,
        PLC_UART_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));

    ESP_LOGI(TAG, "UART2 initialized");
    return ESP_OK;
}

esp_err_t plc_uart_client_request(const uint8_t *payload,
                                  uint16_t payload_len,
                                  uint8_t *rsp_payload,
                                  size_t rsp_cap,
                                  uint16_t *rsp_len,
                                  uint32_t timeout_ms)
{
    if (payload == NULL || rsp_payload == NULL || rsp_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t frame[PLC_FRAME_MAX_WIRE_SIZE];
    uint16_t frame_len = 0u;

    plc_frame_result_t fr = plc_frame_build(
        payload,
        payload_len,
        frame,
        sizeof(frame),
        &frame_len
    );

    if (fr != PLC_FRAME_OK) {
        s_stats.size_errors++;
        return ESP_FAIL;
    }

    uart_flush(PLC_UART_PORT);

    int written = uart_write_bytes(PLC_UART_PORT, frame, frame_len);
    if (written != frame_len) {
        s_stats.uart_errors++;
        return ESP_FAIL;
    }

    s_stats.tx_frames++;

    uint8_t rx_buf[PLC_FRAME_MAX_WIRE_SIZE];
    int rd = uart_read_bytes(
        PLC_UART_PORT,
        rx_buf,
        sizeof(rx_buf),
        pdMS_TO_TICKS(timeout_ms)
    );

    if (rd <= 0) {
        s_stats.timeouts++;
        return ESP_ERR_TIMEOUT;
    }

    size_t consumed = 0u;

    fr = plc_frame_try_parse(
        rx_buf,
        (size_t)rd,
        rsp_payload,
        rsp_cap,
        rsp_len,
        &consumed
    );

    switch (fr) {
        case PLC_FRAME_OK:
            s_stats.rx_frames++;
            return ESP_OK;

        case PLC_FRAME_ERR_CRC:
            s_stats.crc_errors++;
            return ESP_ERR_INVALID_CRC;

        case PLC_FRAME_ERR_SIZE:
            s_stats.size_errors++;
            return ESP_ERR_INVALID_SIZE;

        default:
            return ESP_FAIL;
    }
}

void plc_uart_client_get_stats(plc_uart_stats_t *out_stats)
{
    if (out_stats != NULL) {
        *out_stats = s_stats;
    }
}
