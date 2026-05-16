#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PLC_GRAPH_MAX_NODES 100u

typedef struct {
    bool b;
    int32_t i;
    float f;
} plc_value_bin_t;

typedef struct {
    uint16_t id;

    uint8_t type;
    uint8_t valueType;

    int16_t inA;
    int16_t inB;

    int32_t paramInt;
    float paramFloat;
    uint32_t paramMs;
    uint32_t flags;

    plc_value_bin_t out;

    bool sr_q;
    bool trig_prev;

    bool ton_active;
    uint32_t ton_accum_ms;

    bool toff_holding;
    uint32_t toff_left_ms;

    bool force_en;
    bool force_bool;
    uint32_t force_left_ms;

    float pid_i;
    float pid_prev_meas;
    bool pid_inited;

    float filt_prev;
    bool filt_inited;

    float ramp_prev;
    bool ramp_inited;

    uint32_t log_accum_ms;

    int32_t acc;
    bool prev_clk;

    bool ao_zero_hold;
} plc_node_bin_t;

typedef struct {
    uint32_t cycleMs;
    uint16_t nodeCount;
    plc_node_bin_t nodes[PLC_GRAPH_MAX_NODES];
} plc_graph_bin_t;

_Static_assert(sizeof(plc_node_bin_t) == 100,
               "plc_node_bin_t size must match STM32 PlcNode");

_Static_assert(sizeof(plc_graph_bin_t) == 10008,
               "plc_graph_bin_t size must match STM32 PlcGraph");