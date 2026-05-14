#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PLC_GRAPH_MAX_NODES 100u

typedef enum {
    PLC_VAL_BOOL  = 0,
    PLC_VAL_INT   = 1,
    PLC_VAL_FLOAT = 2
} plc_value_type_t;

typedef enum {
    PLC_NODE_CONST_BOOL = 0,
    PLC_NODE_CONST_INT,
    PLC_NODE_DIGITAL_IN,
    PLC_NODE_DIGITAL_OUT,
    PLC_NODE_AND2,
    PLC_NODE_OR2,
    PLC_NODE_NOT,
    PLC_NODE_SR,
    PLC_NODE_TON,
    PLC_NODE_TOFF,
    PLC_NODE_R_TRIG,
    PLC_NODE_F_TRIG,
    PLC_NODE_AI_IN,
    PLC_NODE_COMPARE_GT,
    PLC_NODE_COMPARE_LT,
    PLC_NODE_COMPARE_GE,
    PLC_NODE_MUX2,
    PLC_NODE_TP,
    PLC_NODE_HYST,
    PLC_NODE_SCALE,
    PLC_NODE_ADD,
    PLC_NODE_LIMIT,
    PLC_NODE_PID,
    PLC_NODE_ANALOG_AVG,
    PLC_NODE_RAMP,
    PLC_NODE_LOG,
    PLC_NODE_AO,
    PLC_NODE_CTU,
    PLC_NODE_CTD,
    PLC_NODE_CTUD,
    PLC_NODE_WINDOW_CHECK,
    PLC_NODE_SAFE_OUTPUT,
    PLC_NODE_ALARM_GEN,
    PLC_NODE_ALARM_LATCH,
    PLC_NODE_HEARTBEAT,
    PLC_NODE_MEM_BOOL,
    PLC_NODE_MEM_INT,
    PLC_NODE_MEM_REAL,
    PLC_NODE_FILTER_AVG,
    PLC_NODE_MATH_OP,
    PLC_NODE_CONST_FLOAT,
    PLC_NODE_HSC_IN,
    PLC_NODE_ENCODER_IN,
    PLC_NODE_MAX
} plc_node_type_t;

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

_Static_assert(sizeof(plc_node_bin_t) == 100, "plc_node_bin_t size must match STM32 PlcNode");
_Static_assert(sizeof(plc_graph_bin_t) == 10008, "plc_graph_bin_t size must match STM32 PlcGraph");
