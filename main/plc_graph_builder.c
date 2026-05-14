#include "plc_graph_builder.h"

#include "psa/crypto.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool json_find_key(const char *json,
                          const char *key,
                          const char **out)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);

    const char *p = strstr(json, pat);
    if (p == NULL) {
        return false;
    }

    p = strchr(p, ':');
    if (p == NULL) {
        return false;
    }

    p++;

    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    *out = p;
    return true;
}

static bool json_get_int32(const char *json,
                           const char *key,
                           int32_t *out)
{
    const char *p = NULL;
    if (!json_find_key(json, key, &p)) {
        return false;
    }

    *out = (int32_t)strtol(p, NULL, 10);
    return true;
}

static bool json_get_float(const char *json,
                           const char *key,
                           float *out)
{
    const char *p = NULL;
    if (!json_find_key(json, key, &p)) {
        return false;
    }

    *out = strtof(p, NULL);
    return true;
}

static bool json_get_string(const char *json,
                            const char *key,
                            char *out,
                            size_t cap)
{
    const char *p = NULL;
    if (!json_find_key(json, key, &p)) {
        return false;
    }

    if (*p != '\"') {
        return false;
    }

    p++;

    size_t i = 0u;
    while (*p && *p != '\"' && i < cap - 1u) {
        out[i++] = *p++;
    }

    out[i] = '\0';
    return true;
}

static uint8_t parse_node_type(const char *type)
{
    if (strcmp(type, "DI") == 0) return PLC_NODE_DIGITAL_IN;
    if (strcmp(type, "DO") == 0) return PLC_NODE_DIGITAL_OUT;
    if (strcmp(type, "TON") == 0) return PLC_NODE_TON;
    if (strcmp(type, "TOFF") == 0) return PLC_NODE_TOFF;
    if (strcmp(type, "AND") == 0) return PLC_NODE_AND2;
    if (strcmp(type, "OR") == 0) return PLC_NODE_OR2;
    if (strcmp(type, "NOT") == 0) return PLC_NODE_NOT;
    if (strcmp(type, "GT") == 0) return PLC_NODE_COMPARE_GT;
    if (strcmp(type, "LT") == 0) return PLC_NODE_COMPARE_LT;
    if (strcmp(type, "CONST_BOOL") == 0) return PLC_NODE_CONST_BOOL;
    if (strcmp(type, "CONST_INT") == 0) return PLC_NODE_CONST_INT;
    if (strcmp(type, "CONST_FLOAT") == 0) return PLC_NODE_CONST_FLOAT;

    return PLC_NODE_CONST_BOOL;
}

static uint8_t parse_value_type(const char *type)
{
    if (strcmp(type, "BOOL") == 0) return PLC_VAL_BOOL;
    if (strcmp(type, "INT") == 0) return PLC_VAL_INT;
    if (strcmp(type, "FLOAT") == 0) return PLC_VAL_FLOAT;

    return PLC_VAL_BOOL;
}

static esp_err_t parse_node(const char *json,
                            plc_node_bin_t *node)
{
    int32_t v = 0;
    float f = 0.0f;
    char type[32];
    char value_type[32];

    memset(node, 0, sizeof(*node));

    if (!json_get_int32(json, "id", &v)) return ESP_FAIL;
    node->id = (uint16_t)v;

    if (!json_get_string(json, "type", type, sizeof(type))) return ESP_FAIL;
    node->type = parse_node_type(type);

    if (json_get_string(json, "valueType", value_type, sizeof(value_type))) {
        node->valueType = parse_value_type(value_type);
    }

    if (json_get_int32(json, "inA", &v)) node->inA = (int16_t)v;
    if (json_get_int32(json, "inB", &v)) node->inB = (int16_t)v;

    if (json_get_int32(json, "paramInt", &v)) node->paramInt = v;
    if (json_get_float(json, "paramFloat", &f)) node->paramFloat = f;
    if (json_get_int32(json, "paramMs", &v)) node->paramMs = (uint32_t)v;
    if (json_get_int32(json, "flags", &v)) node->flags = (uint32_t)v;

    return ESP_OK;
}

esp_err_t plc_graph_builder_from_json(const char *json,
                                      plc_graph_bin_t *out_graph,
                                      char *err,
                                      uint16_t err_cap)
{
    if (json == NULL || out_graph == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out_graph, 0, sizeof(*out_graph));

    int32_t cycle_ms = 10;
    (void)json_get_int32(json, "cycleMs", &cycle_ms);

    out_graph->cycleMs = (uint32_t)cycle_ms;

    const char *nodes = strstr(json, "\"nodes\"");
    if (nodes == NULL) {
        snprintf(err, err_cap, "nodes[] not found");
        return ESP_FAIL;
    }

    const char *p = strchr(nodes, '[');
    if (p == NULL) {
        snprintf(err, err_cap, "nodes array broken");
        return ESP_FAIL;
    }

    p++;

    uint16_t node_index = 0u;

    while (*p && node_index < PLC_GRAPH_MAX_NODES) {
        const char *obj_begin = strchr(p, '{');
        if (obj_begin == NULL) {
            break;
        }

        const char *obj_end = strchr(obj_begin, '}');
        if (obj_end == NULL) {
            snprintf(err, err_cap, "node object broken");
            return ESP_FAIL;
        }

        char obj[512];
        size_t len = (size_t)(obj_end - obj_begin + 1);

        if (len >= sizeof(obj)) {
            snprintf(err, err_cap, "node object too large");
            return ESP_FAIL;
        }

        memcpy(obj, obj_begin, len);
        obj[len] = '\0';

        if (parse_node(obj, &out_graph->nodes[node_index]) != ESP_OK) {
            snprintf(err, err_cap, "node parse failed");
            return ESP_FAIL;
        }

        node_index++;
        p = obj_end + 1;
    }

    out_graph->nodeCount = node_index;

    return ESP_OK;
}

void plc_graph_builder_sha256_hex(const char *text,
                                  char out_hex[65])
{
    uint8_t sha[32];
    size_t sha_len = 0;

    psa_crypto_init();

    psa_status_t st = psa_hash_compute(
            PSA_ALG_SHA_256,
            (const uint8_t *)text,
            strlen(text),
            sha,
            sizeof(sha),
            &sha_len
    );

    if (st != PSA_SUCCESS || sha_len != 32u) {
        memset(out_hex, 0, 65);
        return;
    }

    for (uint8_t i = 0; i < 32u; i++) {
        sprintf(&out_hex[i * 2u], "%02x", sha[i]);
    }

    out_hex[64] = '\0';
}
