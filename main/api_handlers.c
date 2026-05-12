#include "api_handlers.h"

#include "esp_log.h"
#include "plc_client.h"
#include "plc_gateway_state.h"
#include "plc_protocol.h"
#include "plc_graph_builder.h"
#include "plc_graph_storage.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static esp_err_t api_status_handler(httpd_req_t *req)
{
    plc_gateway_state_t st;
    plc_gateway_state_get(&st);

    char json[4096];

    snprintf(json, sizeof(json),
             "{\"connection\":{\"linkStatus\":\"%s\"},\"activeGraph\":{\"version\":\"%lu\",\"nodes\":%lu}}",
             st.connected ? "ONLINE" : "OFFLINE",
             (unsigned long)st.active_graph_version,
             (unsigned long)st.node_count);

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

static esp_err_t api_graph_upload_handler(httpd_req_t *req)
{
    static char body[16384];

    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        return ESP_FAIL;
    }

    plc_graph_bin_t graph;
    char err[128];

    if (plc_graph_builder_from_json(body, &graph, err, sizeof(err)) != ESP_OK) {
        char json[256];
        snprintf(json, sizeof(json), "{\"ok\":false,\"error\":\"%s\"}", err);
        return send_json(req, json);
    }

    char sha256[65];
    plc_graph_builder_sha256_hex(body, sha256);

    if (plc_graph_storage_save(body, sha256) != ESP_OK) {
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

    r = plc_client_activate();

    if (r != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":\"activate failed\"}");
    }

    char json[256];
    snprintf(json, sizeof(json),
             "{\"ok\":true,\"sha256\":\"%s\",\"nodes\":%u}",
             sha256,
             graph.nodeCount);

    return send_json(req, json);
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
