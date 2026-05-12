#include "plc_gateway_state.h"
#include "plc_protocol.h"

#include <string.h>

static plc_gateway_state_t s_state;

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
}

void plc_gateway_state_update_from_status_ext(const uint8_t *body, uint16_t len)
{
    if (body == NULL || len < 224u) {
        return;
    }

    const uint32_t runtime_flags = plc_get_u32_le(&body[12]);

    s_state.connected = true;
    s_state.safe_or_fault = (runtime_flags & 0x04u) != 0u;

    s_state.cycle_ms = plc_get_u32_le(&body[16]);
    s_state.node_count = plc_get_u32_le(&body[20]);
    s_state.cycle_counter = plc_get_u32_le(&body[24]);
    s_state.last_cycle_us = plc_get_u32_le(&body[28]);
    s_state.max_cycle_us = plc_get_u32_le(&body[32]);

    s_state.runtime_fault_counter = plc_get_u32_le(&body[36]);
    s_state.runtime_fault = plc_get_u32_le(&body[40]);

    s_state.active_graph_version = plc_get_u32_le(&body[44]);
    s_state.active_graph_size = plc_get_u32_le(&body[120]);
    s_state.active_graph_crc32 = plc_get_u32_le(&body[124]);

    s_state.rx_ok = plc_get_u32_le(&body[68]);
    s_state.crc_errors = plc_get_u32_le(&body[72]);

    s_state.heap_free = plc_get_u32_le(&body[168]);
    s_state.heap_min = plc_get_u32_le(&body[172]);
}
