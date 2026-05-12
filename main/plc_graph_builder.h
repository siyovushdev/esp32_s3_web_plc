#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "plc_graph_binary.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t plc_graph_builder_from_json(const char *json,
                                      plc_graph_bin_t *out_graph,
                                      char *err,
                                      uint16_t err_cap);

void plc_graph_builder_sha256_hex(const char *text,
                                  char out_hex[65]);

#ifdef __cplusplus
}
#endif
