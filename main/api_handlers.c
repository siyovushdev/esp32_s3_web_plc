#include "api_handlers.h"

#include "esp_log.h"
#include "plc_client.h"
#include "plc_gateway_state.h"
#include "plc_protocol.h"
#include "plc_graph_builder.h"
#include "plc_graph_storage.h"
#include "friendly_plc/plc_types.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_timer.h>

static const char *TAG = "API";

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t read_body(httpd_req_t *req, char *buf, size_t cap)
{
    if (buf == NULL || cap == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t total = req->content_len;
    if (total >= cap) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Body too large");
        return ESP_FAIL;
    }

    size_t off = 0u;
    while (off < total) {
        int r = httpd_req_recv(req, buf + off, total - off);
        if (r <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read body");
            return ESP_FAIL;
        }
        off += (size_t)r;
    }

    buf[off] = '\0';
    return ESP_OK;
}

static bool json_get_int(const char *json, const char *key, int *out)
{
    char pat[48];
    snprintf(pat, sizeof(pat), "\"%s\"", key);

    const char *p = strstr(json, pat);
    if (p == NULL) return false;

    p = strchr(p, ':');
    if (p == NULL) return false;
    p++;

    while (*p && isspace((unsigned char)*p)) p++;

    *out = atoi(p);
    return true;
}

static bool json_get_type(const char *json, uint8_t *out_type)
{
    const char *p = strstr(json, "\"type\"");
    if (p == NULL) return false;

    p = strchr(p, ':');
    if (p == NULL) return false;
    p++;

    while (*p && (*p == ' ' || *p == '\t' || *p == '\"')) p++;

    if (strncmp(p, "bool", 4) == 0) {
        *out_type = PLC_MEM_TYPE_BOOL;
        return true;
    }
    if (strncmp(p, "int", 3) == 0) {
        *out_type = PLC_MEM_TYPE_INT;
        return true;
    }
    if (strncmp(p, "real", 4) == 0) {
        *out_type = PLC_MEM_TYPE_REAL;
        return true;
    }

    return false;
}

static esp_err_t api_health_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true}");
}

static const char *run_state_to_str(uint32_t run_state)
{
    switch (run_state) {
        case 1u: return "RUN";
        case 2u: return "SAFE";
        case 3u: return "FAULT";
        case 0u:
        default: return "STOP";
    }
}

static const char *node_type_to_str(uint32_t type)
{
    switch ((PlcNodeType)type) {

        case PLC_NODE_CONST_BOOL:   return "CONST_BOOL";
        case PLC_NODE_CONST_INT:    return "CONST_INT";

        case PLC_NODE_DIGITAL_IN:   return "DIGITAL_IN";
        case PLC_NODE_DIGITAL_OUT:  return "DIGITAL_OUT";

        case PLC_NODE_AND2:         return "AND";
        case PLC_NODE_OR2:          return "OR";
        case PLC_NODE_NOT:          return "NOT";
        case PLC_NODE_SR:           return "SR";

        case PLC_NODE_TON:          return "TON";
        case PLC_NODE_TOFF:         return "TOFF";
        case PLC_NODE_R_TRIG:       return "R_TRIG";
        case PLC_NODE_F_TRIG:       return "F_TRIG";

        case PLC_NODE_AI_IN:        return "AI_IN";

        case PLC_NODE_COMPARE_GT:   return "COMPARE_GT";
        case PLC_NODE_COMPARE_LT:   return "COMPARE_LT";
        case PLC_NODE_COMPARE_GE:   return "COMPARE_GE";

        case PLC_NODE_MUX2:         return "MUX2";

        case PLC_NODE_TP:           return "TP";
        case PLC_NODE_HYST:         return "HYST";
        case PLC_NODE_SCALE:        return "SCALE";
        case PLC_NODE_ADD:          return "ADD";
        case PLC_NODE_LIMIT:        return "LIMIT";
        case PLC_NODE_PID:          return "PID";
        case PLC_NODE_ANALOG_AVG:   return "ANALOG_AVG";
        case PLC_NODE_RAMP:         return "RAMP";
        case PLC_NODE_LOG:          return "LOG";
        case PLC_NODE_AO:           return "AO";

        case PLC_NODE_CTU:          return "CTU";
        case PLC_NODE_CTD:          return "CTD";
        case PLC_NODE_CTUD:         return "CTUD";

        case PLC_NODE_WINDOW_CHECK: return "WINDOW_CHECK";
        case PLC_NODE_SAFE_OUTPUT:  return "SAFE_OUTPUT";

        case PLC_NODE_ALARM_GEN:    return "ALARM_GEN";
        case PLC_NODE_ALARM_LATCH:  return "ALARM_LATCH";

        case PLC_NODE_HEARTBEAT:    return "HEARTBEAT";

        case PLC_NODE_MEM_BOOL:     return "MEM_BOOL";
        case PLC_NODE_MEM_INT:      return "MEM_INT";
        case PLC_NODE_MEM_REAL:     return "MEM_REAL";

        case PLC_NODE_FILTER_AVG:   return "FILTER_AVG";
        case PLC_NODE_MATH_OP:      return "MATH_OP";

        case PLC_NODE_CONST_FLOAT:  return "CONST_FLOAT";

        case PLC_NODE_HSC_IN:       return "HSC_IN";
        case PLC_NODE_ENCODER_IN:   return "ENCODER_IN";

        default:
            return "NODE";
    }
}

static esp_err_t api_status_handler(httpd_req_t *req)
{
    plc_gateway_state_t st;
    plc_gateway_state_get(&st);

    char json[8192];
    size_t off = 0u;

    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
    const uint32_t age_ms =
            (st.last_exchange_ms == 0u || now_ms < st.last_exchange_ms)
            ? 0u
            : (now_ms - st.last_exchange_ms);

    const float memory_usage_percent =
            st.memory_usage_x100 > 0u
            ? ((float)st.memory_usage_x100 / 100.0f)
            : 0.0f;

    off += snprintf(&json[off], sizeof(json) - off,
                    "{"
                    "\"connection\":{"
                    "\"ip\":\"192.168.4.1\","
                    "\"port\":80,"
                    "\"mode\":\"%s\","
                    "\"uptime\":\"%lu cycles\","
                    "\"lastExchangeAgo\":\"%lu ms\","
                    "\"linkStatus\":\"%s\","
                    "\"running\":%s,"
                    "\"activeGraphValid\":%s,"
                    "\"safeOrFault\":%s"
                    "},"
                    "\"performance\":{"
                    "\"scanAvgMs\":%.3f,"
                    "\"scanMaxMs\":%.3f,"
                    "\"scanAvgUs\":%lu,"
                    "\"scanMaxUs\":%lu,"
                    "\"workAvgMs\":%.3f,"
                    "\"workMaxMs\":%.3f,"
                    "\"workAvgUs\":%lu,"
                    "\"workMaxUs\":%lu,"
                    "\"cycleRealAvgMs\":%.3f,"
                    "\"cycleRealMaxMs\":%.3f,"
                    "\"scanLimitMs\":%lu,"
                    "\"cpuLoadPercent\":%.2f,"
                    "\"memoryUsagePercent\":%.2f,"
                    "\"crcErrors\":%lu,"
                    "\"timeouts\":%lu,"
                    "\"scanLongSteps\":%lu"
                    "},"
                    "\"ioSummary\":{"
                    "\"diUsed\":%u,"
                    "\"diTotal\":%u,"
                    "\"doUsed\":%u,"
                    "\"doTotal\":%u,"
                    "\"aiUsed\":%u,"
                    "\"aiTotal\":%u,"
                    "\"pwmUsed\":%u,"
                    "\"pwmTotal\":%u"
                    "},"
                    "\"activeGraph\":{"
                    "\"name\":\"\","
                    "\"version\":\"%lu\","
                    "\"size\":%lu,"
                    "\"crc32\":\"0x%08lX\","
                    "\"nodes\":%lu,"
                    "\"connections\":0,"
                    "\"inputs\":%u,"
                    "\"outputs\":%u,"
                    "\"compileErrors\":0,"
                    "\"activatedAt\":\"\","
                    "\"runState\":\"%s\","
                    "\"runtimeFault\":%lu,"
                    "\"runtimeFaultCounter\":%lu"
                    "},"
                    "\"alarms\":[],"
                    "\"nodes\":[",
                    run_state_to_str(st.run_state),
                    (unsigned long)st.cycle_counter,
                    (unsigned long)age_ms,
                    st.connected ? "ONLINE" : "OFFLINE",
                    st.running ? "true" : "false",
                    st.active_graph_valid ? "true" : "false",
                    st.safe_or_fault ? "true" : "false",
                    (double)st.scan_avg_us / 1000.0,
                    (double)st.scan_max_us / 1000.0,
                    (unsigned long)st.scan_avg_us,
                    (unsigned long)st.scan_max_us,

                    (double)st.work_avg_us / 1000.0,
                    (double)st.work_max_us / 1000.0,
                    (unsigned long)st.work_avg_us,
                    (unsigned long)st.work_max_us,

                    (double)st.cycle_real_avg_us / 1000.0,
                    (double)st.cycle_real_max_us / 1000.0,
                    (unsigned long)st.scan_limit_ms,
                    (double)st.cpu_load_x100 / 100.0,
                    (double)memory_usage_percent,
                    (unsigned long)st.crc_errors,
                    (unsigned long)st.timeouts,
                    (unsigned long)st.scan_long_steps,

                    (unsigned)st.di_used,
                    (unsigned)st.di_total,
                    (unsigned)st.do_used,
                    (unsigned)st.do_total,
                    (unsigned)st.ai_used,
                    (unsigned)st.ai_total,
                    (unsigned)st.pwm_used,
                    (unsigned)st.pwm_total,

                    (unsigned long)st.active_graph_version,
                    (unsigned long)st.active_graph_size,
                    (unsigned long)st.active_graph_crc32,
                    (unsigned long)st.node_count,
                    (unsigned)st.di_used,
                    (unsigned)st.do_used,
                    run_state_to_str(st.run_state),
                    (unsigned long)st.runtime_fault,
                    (unsigned long)st.runtime_fault_counter
    );

    uint32_t emitted = 0u;
    for (uint32_t i = 0u; i < PLC_GATEWAY_MAX_NODES; i++) {
        const plc_gateway_node_state_t *n = &st.nodes[i];
        if (!n->valid) {
            continue;
        }

        if (emitted > 0u) {
            off += snprintf(&json[off], sizeof(json) - off, ",");
        }

        off += snprintf(&json[off], sizeof(json) - off,
                        "{"
                        "\"index\":%u,"
                        "\"id\":%u,"
                        "\"type\":\"%s\","
                        "\"valueType\":0,"
                        "\"outBool\":%s,"
                        "\"outInt\":%ld,"
                        "\"outFloat\":%.3f,"
                        "\"tonMs\":null,"
                        "\"toffLeftMs\":null,"
                        "\"pidSp\":null,"
                        "\"pidPv\":null,"
                        "\"pidI\":null,"
                        "\"pidU\":null,"
                        "\"forceActive\":%s,"
                        "\"forceValue\":%s,"
                        "\"forceLeftMs\":%lu"
                        "}",
                        (unsigned)n->index,
                        (unsigned)n->id,
                        node_type_to_str(n->type),
                        n->out_bool ? "true" : "false",
                        (long)n->out_int,
                        (double)n->out_float,
                        n->force_active ? "true" : "false",
                        n->force_value ? "true" : "false",
                        (unsigned long)n->force_left_ms
        );

        emitted++;

        if (off > sizeof(json) - 512u) {
            break;
        }
    }

    off += snprintf(&json[off], sizeof(json) - off,
                    "],"
                    "\"isLoading\":false,"
                    "\"errorMessage\":null"
                    "}"
    );

    return send_json(req, json);
}

static esp_err_t api_graph_active_handler(httpd_req_t *req)
{
    char json[16384];
    char sha[128];

    if (plc_graph_storage_load(json, sizeof(json), sha, sizeof(sha)) != ESP_OK) {
        httpd_resp_set_status(req, "404 Not Found");
        return send_json(req, "{\"ok\":false,\"error\":\"No active graph stored\"}");
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t api_ok_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t api_force_output_handler(httpd_req_t *req)
{
    char body[256];
    if (read_body(req, body, sizeof(body)) != ESP_OK) return ESP_FAIL;

    int node_index = 0;
    int value_int = 0;
    int hold_ms = 0;

    if (!json_get_int(body, "nodeIndex", &node_index)) {
        return send_json(req, "{\"ok\":false,\"error\":\"nodeIndex required\"}");
    }

    (void)json_get_int(body, "valueInt", &value_int);
    (void)json_get_int(body, "holdMs", &hold_ms);

    esp_err_t r = plc_client_force_output((uint16_t)node_index, value_int != 0, (uint32_t)hold_ms);
    return send_json(req, r == ESP_OK ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"force failed\"}");
}

static esp_err_t api_release_output_handler(httpd_req_t *req)
{
    char body[128];
    if (read_body(req, body, sizeof(body)) != ESP_OK) return ESP_FAIL;

    int node_index = 0;
    if (!json_get_int(body, "nodeIndex", &node_index)) {
        return send_json(req, "{\"ok\":false,\"error\":\"nodeIndex required\"}");
    }

    esp_err_t r = plc_client_release_output((uint16_t)node_index);
    return send_json(req, r == ESP_OK ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"release failed\"}");
}

static esp_err_t api_mem_info_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t api_mem_reset_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true}\n");
}

static esp_err_t api_mem_read_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true,\"values\":[]}");
}

static esp_err_t api_mem_write_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":false,\"written\":0}");
}

static esp_err_t api_log_dump_handler(httpd_req_t *req)
{
    return send_json(req, "{\"total\":0,\"from\":0,\"count\":0,\"items\":[]}");
}

static bool extract_graph_json(const char *body, char *out, size_t out_cap)
{
    const char *key = strstr(body, "\"graphJson\"");
    if (key == NULL) {
        size_t len = strlen(body);
        if (len >= out_cap) {
            return false;
        }

        memcpy(out, body, len + 1u);
        return true;
    }

    const char *p = strchr(key, ':');
    if (p == NULL) {
        return false;
    }

    p++;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }

    if (*p != '"') {
        return false;
    }

    p++;

    size_t out_pos = 0u;
    bool escape = false;

    while (*p && out_pos < out_cap - 1u) {
        char c = *p++;

        if (escape) {
            switch (c) {
                case '"': out[out_pos++] = '"'; break;
                case '\\': out[out_pos++] = '\\'; break;
                case '/': out[out_pos++] = '/'; break;
                case 'b': out[out_pos++] = '\b'; break;
                case 'f': out[out_pos++] = '\f'; break;
                case 'n': out[out_pos++] = '\n'; break;
                case 'r': out[out_pos++] = '\r'; break;
                case 't': out[out_pos++] = '\t'; break;
                default: out[out_pos++] = c; break;
            }
            escape = false;
            continue;
        }

        if (c == '\\') {
            escape = true;
            continue;
        }

        if (c == '"') {
            out[out_pos] = '\0';
            return true;
        }

        out[out_pos++] = c;
    }

    return false;
}

static esp_err_t api_graph_upload_handler(httpd_req_t *req)
{
    static char body[16384];

    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        return ESP_FAIL;
    }

    static char graph_json[16384];

    if (!extract_graph_json(body, graph_json, sizeof(graph_json))) {
        return send_json(req, "{\"ok\":false,\"error\":\"graphJson extract failed\"}");
    }

    plc_graph_bin_t graph;
    char err[128];

    if (plc_graph_builder_from_json(graph_json, &graph, err, sizeof(err)) != ESP_OK) {
        char json[256];
        snprintf(json, sizeof(json), "{\"ok\":false,\"error\":\"%s\"}", err);
        return send_json(req, json);
    }

    char sha256[65];
    plc_graph_builder_sha256_hex(graph_json, sha256);

    if (plc_graph_storage_save(graph_json, sha256) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":\"storage save failed\"}");
    }

    esp_err_t r = plc_client_upload_graph_image(
        (const uint8_t *)&graph,
        sizeof(graph),
        1u
    );

    if (r != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":\"upload failed\"}");
    }

//    r = plc_client_activate();
//
//    if (r != ESP_OK) {
//        return send_json(req, "{\"ok\":false,\"error\":\"activate failed\"}");
//    }

    char json[256];
    snprintf(json, sizeof(json),
             "{\"ok\":true,\"sha256\":\"%s\",\"nodes\":%u}",
             sha256,
             graph.nodeCount);

    return send_json(req, json);
}

static esp_err_t api_safe_reset_handler(httpd_req_t *req)
{
    esp_err_t r = plc_client_safe_reset();

    if (r != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":\"safe reset failed\"}");
    }

    (void)plc_client_get_status_web_v2();
    (void)plc_client_get_io_summary();
    (void)plc_client_get_nodes_snapshot();

    return send_json(req, "{\"ok\":true}");
}

static esp_err_t api_graph_activate_handler(httpd_req_t *req)
{
    esp_err_t r = plc_client_activate();
    return send_json(req, r == ESP_OK ? "{\"ok\":true,\"active\":true}" : "{\"ok\":false,\"error\":\"activate failed\"}");
}

static void register_get(httpd_handle_t server, const char *uri, esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t route = {.uri = uri, .method = HTTP_GET, .handler = handler, .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
}

static void register_post(httpd_handle_t server, const char *uri, esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t route = {.uri = uri, .method = HTTP_POST, .handler = handler, .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
}

esp_err_t api_register_handlers(httpd_handle_t server)
{
    register_get(server, "/api/health", api_health_handler);
    register_get(server, "/api/plc/status", api_status_handler);
    register_get(server, "/api/plc/graph/active", api_graph_active_handler);
    register_get(server, "/api/plc/log/dump", api_log_dump_handler);

    register_post(server, "/api/plc/persist/save", api_ok_handler);
    register_post(server, "/api/plc/persist/load", api_ok_handler);

    register_post(server, "/api/plc/graph/upload", api_graph_upload_handler);
    register_post(server, "/api/plc/graph/activate", api_graph_activate_handler);

    register_post(server, "/api/plc/safe/reset", api_safe_reset_handler);

    register_post(server, "/api/plc/output/force", api_force_output_handler);
    register_post(server, "/api/plc/output/release", api_release_output_handler);

    register_post(server, "/api/plc/mem/reset", api_mem_reset_handler);
    register_post(server, "/api/plc/mem/info", api_mem_info_handler);
    register_post(server, "/api/plc/mem/read", api_mem_read_handler);
    register_post(server, "/api/plc/mem/write/bool", api_mem_write_handler);
    register_post(server, "/api/plc/mem/write/int", api_mem_write_handler);
    register_post(server, "/api/plc/mem/write/real", api_mem_write_handler);

    ESP_LOGI(TAG, "API handlers registered");
    return ESP_OK;
}
