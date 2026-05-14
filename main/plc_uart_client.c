#include "plc_uart_client.h"
#include "plc_frame.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include <string.h>

#define PLC_UART_PORT UART_NUM_2
#define PLC_UART_TX_PIN GPIO_NUM_17
#define PLC_UART_RX_PIN GPIO_NUM_18
#define PLC_UART_BAUD 115200

static const char *TAG = "plc_uart";

static plc_uart_stats_t s_stats;
static SemaphoreHandle_t s_uart_mutex;

static uint16_t plc_get_u16_le_local(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static esp_err_t plc_uart_read_exact(uint8_t *dst, size_t len, uint32_t timeout_ms)
{
    if (dst == NULL || len == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t pos = 0u;

    while (pos < len) {
        const size_t left = len - pos;
        const int rd = uart_read_bytes(
                PLC_UART_PORT,
                &dst[pos],
                left,
                pdMS_TO_TICKS(timeout_ms)
        );

        if (rd <= 0) {
            return ESP_ERR_TIMEOUT;
        }

        pos += (size_t)rd;
    }

    return ESP_OK;
}

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

    s_uart_mutex = xSemaphoreCreateMutex();
    if (s_uart_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UART2 initialized: baud=%d tx=%d rx=%d",
             PLC_UART_BAUD,
             (int)PLC_UART_TX_PIN,
             (int)PLC_UART_RX_PIN);
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

    if (s_uart_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    *rsp_len = 0u;

    if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(timeout_ms + 100u)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t result = ESP_FAIL;

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
        ESP_LOGW(TAG, "TX frame build failed: fr=%d payload_len=%u", (int)fr, (unsigned)payload_len);
        result = ESP_FAIL;
        goto done;
    }

    uart_flush_input(PLC_UART_PORT);

    const int written = uart_write_bytes(PLC_UART_PORT, frame, frame_len);
    if (written != frame_len) {
        s_stats.uart_errors++;
        ESP_LOGW(TAG, "UART write failed: written=%d expected=%u", written, (unsigned)frame_len);
        result = ESP_FAIL;
        goto done;
    }

    if (uart_wait_tx_done(PLC_UART_PORT, pdMS_TO_TICKS(timeout_ms)) != ESP_OK) {
        s_stats.uart_errors++;
        ESP_LOGW(TAG, "UART TX wait timeout");
        result = ESP_ERR_TIMEOUT;
        goto done;
    }

    s_stats.tx_frames++;

    uint8_t rx_buf[PLC_FRAME_MAX_WIRE_SIZE];

    result = plc_uart_read_exact(rx_buf, PLC_FRAME_LEN_SIZE, timeout_ms);
    if (result != ESP_OK) {
        s_stats.timeouts++;
        ESP_LOGW(TAG, "RX timeout while reading length");
        goto done;
    }

    const uint16_t rx_payload_len = plc_get_u16_le_local(rx_buf);
    if (rx_payload_len == 0u ||
        rx_payload_len > PLC_FRAME_MAX_PAYLOAD ||
        rx_payload_len > rsp_cap) {
        s_stats.size_errors++;
        ESP_LOGW(TAG, "RX bad payload size: len=%u rsp_cap=%u",
                 (unsigned)rx_payload_len,
                 (unsigned)rsp_cap);
        result = ESP_ERR_INVALID_SIZE;
        goto done;
    }

    const size_t rx_rest_len = (size_t)rx_payload_len + PLC_FRAME_CRC_SIZE;
    result = plc_uart_read_exact(&rx_buf[PLC_FRAME_LEN_SIZE], rx_rest_len, timeout_ms);
    if (result != ESP_OK) {
        s_stats.timeouts++;
        ESP_LOGW(TAG, "RX timeout while reading payload/crc: payload_len=%u",
                 (unsigned)rx_payload_len);
        goto done;
    }

    const size_t rx_frame_len = PLC_FRAME_LEN_SIZE + (size_t)rx_payload_len + PLC_FRAME_CRC_SIZE;
    size_t consumed = 0u;

    fr = plc_frame_try_parse(
            rx_buf,
            rx_frame_len,
            rsp_payload,
            rsp_cap,
            rsp_len,
            &consumed
    );

    switch (fr) {
        case PLC_FRAME_OK:
            s_stats.rx_frames++;
            result = ESP_OK;
            break;

        case PLC_FRAME_ERR_CRC:
            s_stats.crc_errors++;
            ESP_LOGW(TAG, "RX CRC error: frame_len=%u", (unsigned)rx_frame_len);
            result = ESP_ERR_INVALID_CRC;
            break;

        case PLC_FRAME_ERR_SIZE:
            s_stats.size_errors++;
            ESP_LOGW(TAG, "RX size error: payload_len=%u rsp_cap=%u",
                     (unsigned)rx_payload_len,
                     (unsigned)rsp_cap);
            result = ESP_ERR_INVALID_SIZE;
            break;

        case PLC_FRAME_ERR_INCOMPLETE:
            s_stats.timeouts++;
            ESP_LOGW(TAG, "RX incomplete frame after exact read: frame_len=%u consumed=%u",
                     (unsigned)rx_frame_len,
                     (unsigned)consumed);
            result = ESP_ERR_TIMEOUT;
            break;

        default:
            ESP_LOGW(TAG, "RX parse failed: fr=%d frame_len=%u", (int)fr, (unsigned)rx_frame_len);
            result = ESP_FAIL;
            break;
    }

    done:
    xSemaphoreGive(s_uart_mutex);
    return result;
}

void plc_uart_client_get_stats(plc_uart_stats_t *out_stats)
{
    if (out_stats != NULL) {
        *out_stats = s_stats;
    }
}
