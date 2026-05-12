#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t plc_client_init(void);
void plc_client_start_polling(void);

esp_err_t plc_client_get_status_ext(void);

esp_err_t plc_client_get_node(uint16_t node_index,
                              uint8_t *rsp,
                              size_t rsp_cap,
                              uint16_t *rsp_len);

esp_err_t plc_client_force_output(uint16_t node_index,
                                  bool value,
                                  uint32_t hold_ms);

esp_err_t plc_client_release_output(uint16_t node_index);

esp_err_t plc_client_safe_reset(void);

esp_err_t plc_client_activate(void);

esp_err_t plc_client_mem_info(uint8_t *rsp,
                              size_t rsp_cap,
                              uint16_t *rsp_len);

esp_err_t plc_client_mem_read(uint8_t mem_type,
                              uint16_t index,
                              uint16_t count,
                              uint8_t *rsp,
                              size_t rsp_cap,
                              uint16_t *rsp_len);

esp_err_t plc_client_mem_write(uint8_t mem_type,
                               uint16_t index,
                               uint16_t count,
                               const uint8_t *encoded_values,
                               uint16_t encoded_values_len);

#ifdef __cplusplus
}
#endif
