#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLC_UART_RX_BUF_SIZE 2048u
#define PLC_UART_TX_BUF_SIZE 2048u
#define PLC_UART_FRAME_MAX   2048u

typedef struct {
    uint32_t rx_frames;
    uint32_t tx_frames;
    uint32_t crc_errors;
    uint32_t size_errors;
    uint32_t timeouts;
    uint32_t uart_errors;
} plc_uart_stats_t;

esp_err_t plc_uart_client_init(void);

esp_err_t plc_uart_client_request(const uint8_t *payload,
                                  uint16_t payload_len,
                                  uint8_t *rsp_payload,
                                  size_t rsp_cap,
                                  uint16_t *rsp_len,
                                  uint32_t timeout_ms);

void plc_uart_client_get_stats(plc_uart_stats_t *out_stats);

#ifdef __cplusplus
}
#endif
