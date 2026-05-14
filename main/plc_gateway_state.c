#include "plc_gateway_state.h"
#include "plc_protocol.h"

#include "esp_timer.h"

#include <string.h>

#define STATUS_EXT_MARKER 0x31545853u /* STX1 */
#define NODE_MARKER       0x31444F4Eu /* NOD1 */

static plc_gateway_state_t s_state;

static float get_f32_le(const uint8_t *p)
{
    uint32_t bits = plc_get_u32_le(p);
    float v = 0.0f;
    memcpy(&v, &bits, sizeof(v));
    return v;
}

static bool get_bool_u32(const uint8_t *p)
{
    return plc_get_u32_le(p) != 0u;
}

void plc_gateway_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
}

void plc_gateway_state_get(plc_gateway_state_t *out_state)
{
    if (out_state != NULL) {
        *out_state = s_state;
    }
}

void plc_gateway_state_set_connected(bool connected)
{
    s_state.connected = connected;
    if (!connected) {
        s_state.running = false;
        s_state.active_graph_valid = false;
    }
}

void plc_gateway_state_clear_nodes(void)
{
    for (uint32_t i = 0; i < PLC_GATEWAY_MAX_NODES; i++) {
        s_state.nodes[i].valid = false;
    }
    s_state.cached_node_count = 0;
}

void plc_gateway_state_update_from_status_ext(const uint8_t *body, uint16_t len)
{
    if (body == NULL || len < 224u) {
        return;
    }

    if (plc_get_u32_le(&body[0]) != STATUS_EXT_MARKER) {
        return;
    }

    const uint32_t runtime_flags = plc_get_u32_le(&body[12]);

    s_state.connected = true;
    s_state.last_exchange_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
    s_state.mcu_uptime_ms = plc_get_u32_le(&body[8]);

    s_state.active_graph_valid = (runtime_flags & 0x01u) != 0u;
    s_state.running = (runtime_flags & 0x02u) != 0u;
    s_state.safe_or_fault = (runtime_flags & 0x04u) != 0u;

    s_state.cycle_ms = plc_get_u32_le(&body[16]);
    s_state.node_count = plc_get_u32_le(&body[20]);
    if (s_state.node_count > PLC_GATEWAY_MAX_NODES) {
        s_state.node_count = PLC_GATEWAY_MAX_NODES;
    }

    s_state.cycle_counter = plc_get_u32_le(&body[24]);
    s_state.last_cycle_us = plc_get_u32_le(&body[28]);
    s_state.max_cycle_us = plc_get_u32_le(&body[32]);

    s_state.runtime_fault_counter = plc_get_u32_le(&body[36]);
    s_state.runtime_fault = plc_get_u32_le(&body[40]);

    s_state.active_graph_version = plc_get_u32_le(&body[44]);

    s_state.rx_ok = plc_get_u32_le(&body[68]);
    s_state.crc_errors = plc_get_u32_le(&body[72]);
    s_state.size_errors = plc_get_u32_le(&body[76]);
    s_state.rx_overflows = plc_get_u32_le(&body[80]);
    s_state.tx_frames = plc_get_u32_le(&body[88]);
    s_state.tx_errors = plc_get_u32_le(&body[92]);
    s_state.uart_errors = plc_get_u32_le(&body[96]);

    s_state.active_graph_size = plc_get_u32_le(&body[120]);
    s_state.active_graph_crc32 = plc_get_u32_le(&body[124]);

    s_state.heap_total = plc_get_u32_le(&body[164]);
    s_state.heap_free = plc_get_u32_le(&body[168]);
    s_state.heap_min = plc_get_u32_le(&body[172]);
}

bool plc_gateway_state_update_node_from_node_rsp(const uint8_t *body, uint16_t len)
{
    if (body == NULL || len < 108u) {
        return false;
    }

    if (plc_get_u32_le(&body[0]) != NODE_MARKER) {
        return false;
    }

    uint16_t index = plc_get_u16_le(&body[8]);
    if (index >= PLC_GATEWAY_MAX_NODES) {
        return false;
    }

    plc_gateway_node_state_t *n = &s_state.nodes[index];
    memset(n, 0, sizeof(*n));

    n->valid = true;
    n->index = index;
    n->id = plc_get_u16_le(&body[10]);
    n->type = plc_get_u32_le(&body[12]);
    n->flags = plc_get_u32_le(&body[16]);

    n->out_bool = get_bool_u32(&body[20]);
    n->out_int = (int32_t)plc_get_u32_le(&body[24]);
    n->out_float = get_f32_le(&body[28]);

    n->force_active = get_bool_u32(&body[32]);
    n->force_value = get_bool_u32(&body[36]);
    n->force_left_ms = plc_get_u32_le(&body[40]);

    n->ton_active = get_bool_u32(&body[44]);
    n->ton_accum_ms = plc_get_u32_le(&body[48]);

    n->toff_holding = get_bool_u32(&body[52]);
    n->toff_left_ms = plc_get_u32_le(&body[56]);

    n->sr_q = get_bool_u32(&body[60]);
    n->trig_prev = get_bool_u32(&body[64]);

    n->acc = (int32_t)plc_get_u32_le(&body[68]);
    n->prev_clk = get_bool_u32(&body[72]);

    n->pid_inited = get_bool_u32(&body[76]);
    n->pid_i = get_f32_le(&body[80]);
    n->pid_prev_meas = get_f32_le(&body[84]);

    n->filter_inited = get_bool_u32(&body[88]);
    n->filter_prev = get_f32_le(&body[92]);

    n->ramp_inited = get_bool_u32(&body[96]);
    n->ramp_prev = get_f32_le(&body[100]);

    n->ao_zero_hold = get_bool_u32(&body[104]);

    if ((uint32_t)(index + 1u) > s_state.cached_node_count) {
        s_state.cached_node_count = (uint32_t)(index + 1u);
    }

    return true;
}
