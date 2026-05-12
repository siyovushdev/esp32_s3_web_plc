#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLC_GRAPH_JSON_PATH   "/littlefs/active_graph.json"
#define PLC_GRAPH_SHA_PATH    "/littlefs/active_graph.sha256"

esp_err_t plc_graph_storage_save(const char *json,
                                 const char *sha256_hex);

esp_err_t plc_graph_storage_load(char *json,
                                 size_t json_cap,
                                 char *sha256_hex,
                                 size_t sha_cap);

bool plc_graph_storage_exists(void);

#ifdef __cplusplus
}
#endif
