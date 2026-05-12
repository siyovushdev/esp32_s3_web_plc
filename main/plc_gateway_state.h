#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool connected;
    bool safe_or_fault;

    uint32_t last_exchange_ms;

    uint32_t cycle_ms;
    uint32_t cycle_counter;
    uint32_t last_cycle_us;
    uint32_t max_cycle_us;

    uint32_t node_count;

    uint32_t active_graph_version;
    uint32_t active_graph_size;
    uint32_t active_graph_crc32;

    uint32_t heap_free;
    uint32_t heap_min;

    uint32_t rx_ok;
    uint32_t crc_errors;
    uint32_t timeouts;

    uint32_t runtime_fault;
    uint32_t runtime_fault_counter;
} plc_gateway_state_t;

void plc_gateway_state_init(void);

void plc_gateway_state_get(plc_gateway_state_t *out_state);

void plc_gateway_state_set_connected(bool connected);

void plc_gateway_state_update_from_status_ext(const uint8_t *body, uint16_t len);

#ifdef __cplusplus
}
#endif
