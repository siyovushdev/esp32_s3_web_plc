#include "plc_gateway_state.h"
#include "plc_protocol.h"

#include "esp_timer.h"

#include <string.h>

#define STATUS_EXT_MARKER 0x31545853u /* STX1 */
#define NODE_MARKER       0x31444F4Eu /* NOD1 */

#define STATUS_WEB2_MARKER 0x32424557u /* WEB2 */
#define IO_SUMMARY_MARKER  0x31534F49u /* IOS1 */
#define NODES_SNAPSHOT_MARKER 0x314E534Eu /* NSN1 */

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

void plc_gateway_state_update_from_status_web_v2(const uint8_t *body, uint16_t len)
{
    if (body == NULL || len < 160u) {
        return;
    }

    if (plc_get_u32_le(&body[0]) != STATUS_WEB2_MARKER) {
        return;
    }

    s_state.connected = true;
    s_state.last_exchange_ms = (uint32_t)(esp_timer_get_time() / 1000LL);

    s_state.mcu_uptime_ms = plc_get_u32_le(&body[8]);
    s_state.capabilities = plc_get_u32_le(&body[12]);
    s_state.run_state = plc_get_u32_le(&body[16]);

    s_state.running = (s_state.run_state == 1u);
    s_state.safe_or_fault = (s_state.run_state == 2u || s_state.run_state == 3u);

    s_state.cycle_ms = plc_get_u32_le(&body[20]);
    s_state.node_count = plc_get_u32_le(&body[24]);
    if (s_state.node_count > PLC_GATEWAY_MAX_NODES) {
        s_state.node_count = PLC_GATEWAY_MAX_NODES;
    }

    s_state.cycle_counter = plc_get_u32_le(&body[28]);

    s_state.last_cycle_us = plc_get_u32_le(&body[32]);
    s_state.max_cycle_us = plc_get_u32_le(&body[36]);

    s_state.scan_avg_us = s_state.last_cycle_us;
    s_state.scan_max_us = s_state.max_cycle_us;
    s_state.work_avg_us = 0u;
    s_state.work_max_us = 0u;
    s_state.cycle_real_avg_ms = 0u;
    s_state.cycle_real_max_ms = 0u;

    s_state.runtime_fault_counter = plc_get_u32_le(&body[40]);
    s_state.runtime_fault = plc_get_u32_le(&body[44]);

    s_state.active_graph_version = plc_get_u32_le(&body[48]);
    s_state.active_graph_size = plc_get_u32_le(&body[52]);
    s_state.active_graph_crc32 = plc_get_u32_le(&body[56]);

    s_state.rx_ok = plc_get_u32_le(&body[68]);
    s_state.tx_frames = plc_get_u32_le(&body[72]);
    s_state.crc_errors = plc_get_u32_le(&body[76]);
    s_state.size_errors = plc_get_u32_le(&body[80]);
    s_state.rx_overflows = plc_get_u32_le(&body[84]);
    s_state.tx_errors = plc_get_u32_le(&body[88]);
    s_state.uart_errors = plc_get_u32_le(&body[92]);

    s_state.heap_total = plc_get_u32_le(&body[96]);
    s_state.heap_free = plc_get_u32_le(&body[100]);
    s_state.heap_min = plc_get_u32_le(&body[104]);
    s_state.memory_usage_x100 = plc_get_u32_le(&body[108]);

    s_state.active_graph_valid = (s_state.active_graph_size != 0u);
}

void plc_gateway_state_update_from_io_summary(const uint8_t *body, uint16_t len)
{
    if (body == NULL || len < 36u) {
        return;
    }

    if (plc_get_u32_le(&body[0]) != IO_SUMMARY_MARKER) {
        return;
    }

    s_state.di_used = plc_get_u16_le(&body[8]);
    s_state.di_total = plc_get_u16_le(&body[10]);

    s_state.do_used = plc_get_u16_le(&body[12]);
    s_state.do_total = plc_get_u16_le(&body[14]);

    s_state.ai_used = plc_get_u16_le(&body[16]);
    s_state.ai_total = plc_get_u16_le(&body[18]);

    s_state.ao_used = plc_get_u16_le(&body[20]);
    s_state.ao_total = plc_get_u16_le(&body[22]);

    s_state.pwm_used = plc_get_u16_le(&body[24]);
    s_state.pwm_total = plc_get_u16_le(&body[26]);

    s_state.hsc_used = plc_get_u16_le(&body[28]);
    s_state.hsc_total = plc_get_u16_le(&body[30]);

    s_state.encoder_used = plc_get_u16_le(&body[32]);
    s_state.encoder_total = plc_get_u16_le(&body[34]);
}

bool plc_gateway_state_update_nodes_from_snapshot(const uint8_t *body, uint16_t len)
{
    if (body == NULL || len < 16u) {
        return false;
    }

    if (plc_get_u32_le(&body[0]) != NODES_SNAPSHOT_MARKER) {
        return false;
    }

    const uint16_t total_nodes = plc_get_u16_le(&body[8]);
    const uint16_t chunk_index = plc_get_u16_le(&body[10]);
    const uint16_t chunk_count = plc_get_u16_le(&body[12]);
    const uint16_t nodes_in_chunk = plc_get_u16_le(&body[14]);

    if (chunk_count == 0u || chunk_index >= chunk_count) {
        return false;
    }

    const uint16_t expected_len = (uint16_t)(16u + ((uint16_t)nodes_in_chunk * 40u));
    if (len < expected_len) {
        return false;
    }

    if (chunk_index == 0u) {
        plc_gateway_state_clear_nodes();
    }

    s_state.node_count = total_nodes;
    if (s_state.node_count > PLC_GATEWAY_MAX_NODES) {
        s_state.node_count = PLC_GATEWAY_MAX_NODES;
    }

    for (uint16_t i = 0u; i < nodes_in_chunk; i++) {
        const uint8_t *p = &body[16u + ((uint16_t)i * 40u)];

        const uint16_t index = plc_get_u16_le(&p[0]);
        if (index >= PLC_GATEWAY_MAX_NODES) {
            continue;
        }

        plc_gateway_node_state_t *n = &s_state.nodes[index];
        memset(n, 0, sizeof(*n));

        n->valid = true;
        n->index = index;
        n->id = plc_get_u16_le(&p[2]);
        n->type = plc_get_u32_le(&p[4]);
        n->flags = plc_get_u32_le(&p[8]);

        n->out_bool = get_bool_u32(&p[12]);
        n->out_int = (int32_t)plc_get_u32_le(&p[16]);
        n->out_float = get_f32_le(&p[20]);

        n->force_active = get_bool_u32(&p[24]);
        n->force_value = get_bool_u32(&p[28]);
        n->force_left_ms = plc_get_u32_le(&p[32]);

        const uint32_t runtime_flags = plc_get_u32_le(&p[36]);
        n->ton_active = (runtime_flags & (1u << 0)) != 0u;
        n->toff_holding = (runtime_flags & (1u << 1)) != 0u;
        n->sr_q = (runtime_flags & (1u << 2)) != 0u;
        n->trig_prev = (runtime_flags & (1u << 3)) != 0u;
        n->pid_inited = (runtime_flags & (1u << 4)) != 0u;
        n->filter_inited = (runtime_flags & (1u << 5)) != 0u;
        n->ramp_inited = (runtime_flags & (1u << 6)) != 0u;
        n->ao_zero_hold = (runtime_flags & (1u << 7)) != 0u;

        if ((uint32_t)(index + 1u) > s_state.cached_node_count) {
            s_state.cached_node_count = (uint32_t)(index + 1u);
        }
    }

    return true;
}