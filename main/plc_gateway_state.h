#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLC_GATEWAY_MAX_NODES 64u

typedef struct {
    bool valid;

    uint16_t index;
    uint16_t id;
    uint32_t type;
    uint32_t flags;

    bool out_bool;
    int32_t out_int;
    float out_float;

    bool force_active;
    bool force_value;
    uint32_t force_left_ms;

    bool ton_active;
    uint32_t ton_accum_ms;

    bool toff_holding;
    uint32_t toff_left_ms;

    bool sr_q;
    bool trig_prev;

    int32_t acc;
    bool prev_clk;

    bool pid_inited;
    float pid_i;
    float pid_prev_meas;

    bool filter_inited;
    float filter_prev;

    bool ramp_inited;
    float ramp_prev;

    bool ao_zero_hold;
} plc_gateway_node_state_t;

typedef struct {
    bool connected;
    bool running;
    bool active_graph_valid;
    bool safe_or_fault;

    uint32_t last_exchange_ms;
    uint32_t mcu_uptime_ms;

    uint32_t cycle_ms;
    uint32_t cycle_counter;
    uint32_t last_cycle_us;
    uint32_t max_cycle_us;

    uint32_t run_state;
    uint32_t capabilities;

    uint32_t scan_avg_us;
    uint32_t scan_max_us;
    uint32_t work_avg_us;
    uint32_t work_max_us;
    uint32_t cycle_real_avg_ms;
    uint32_t cycle_real_max_ms;

    uint32_t memory_usage_x100;

    uint32_t node_count;
    uint32_t cached_node_count;

    uint32_t active_graph_version;
    uint32_t active_graph_size;
    uint32_t active_graph_crc32;

    uint32_t heap_total;
    uint32_t heap_free;
    uint32_t heap_min;

    uint16_t di_used;
    uint16_t di_total;
    uint16_t do_used;
    uint16_t do_total;
    uint16_t ai_used;
    uint16_t ai_total;
    uint16_t ao_used;
    uint16_t ao_total;
    uint16_t pwm_used;
    uint16_t pwm_total;
    uint16_t hsc_used;
    uint16_t hsc_total;
    uint16_t encoder_used;
    uint16_t encoder_total;

    uint32_t rx_ok;
    uint32_t crc_errors;
    uint32_t size_errors;
    uint32_t rx_overflows;
    uint32_t tx_frames;
    uint32_t tx_errors;
    uint32_t uart_errors;
    uint32_t timeouts;

    uint32_t runtime_fault;
    uint32_t runtime_fault_counter;

    plc_gateway_node_state_t nodes[PLC_GATEWAY_MAX_NODES];
} plc_gateway_state_t;

void plc_gateway_state_init(void);

void plc_gateway_state_get(plc_gateway_state_t *out_state);

void plc_gateway_state_set_connected(bool connected);

void plc_gateway_state_update_from_status_ext(const uint8_t *body, uint16_t len);

bool plc_gateway_state_update_node_from_node_rsp(const uint8_t *body, uint16_t len);

void plc_gateway_state_update_from_status_web_v2(const uint8_t *body, uint16_t len);

void plc_gateway_state_update_from_io_summary(const uint8_t *body, uint16_t len);

bool plc_gateway_state_update_nodes_from_snapshot(const uint8_t *body, uint16_t len);

void plc_gateway_state_clear_nodes(void);

#ifdef __cplusplus
}
#endif
