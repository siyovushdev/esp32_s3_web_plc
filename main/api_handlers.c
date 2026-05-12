#include "api_handlers.h"

#include "esp_log.h"
#include "plc_client.h"
#include "plc_gateway_state.h"

#include <stdio.h>

static const char *TAG = "API";

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t api_health_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t api_status_handler(httpd_req_t *req)
{
    (void)plc_client_get_status_ext();

    plc_gateway_state_t st;
    plc_gateway_state_get(&st);

    char json[4096];

    snprintf(
        json,
        sizeof(json),
        "{"
        "\"connection\":{"
        "\"ip\":\"192.168.4.1\","
        "\"port\":0,"
        "\"mode\":\"%s\","
        "\"uptime\":\"running\","
        "\"lastExchangeAgo\":\"online\","
        "\"linkStatus\":\"%s\""
        "},"
        "\"performance\":{"
        "\"scanAvgMs\":%lu,"
        "\"scanMaxMs\":%lu,"
        "\"scanAvgUs\":%lu,"
        "\"scanMaxUs\":%lu,"
        "\"crcErrors\":%lu,"
        "\"timeouts\":%lu"
        "},"
        "\"ioSummary\":{"
        "\"diUsed\":0,"
        "\"diTotal\":8,"
        "\"doUsed\":0,"
        "\"doTotal\":8,"
        "\"aiUsed\":0,"
        "\"aiTotal\":0"
        "},"
        "\"activeGraph\":{"
        "\"name\":\"active_graph\","
        "\"version\":\"%lu\","
        "\"nodes\":%lu,"
        "\"connections\":0,"
        "\"inputs\":0,"
        "\"outputs\":0,"
        "\"compileErrors\":0,"
        "\"activatedAt\":\"running\","
        "\"runState\":\"%s\""
        "},"
        "\"alarms\":[],"
        "\"nodes\":[],"
        "\"isLoading\":false,"
        "\"errorMessage\":null"
        "}",
        st.safe_or_fault ? "SAFE" : "RUN",
        st.connected ? "ONLINE" : "OFFLINE",
        (unsigned long)st.cycle_ms,
        (unsigned long)st.cycle_ms,
        (unsigned long)st.last_cycle_us,
        (unsigned long)st.max_cycle_us,
        (unsigned long)st.crc_errors,
        (unsigned long)st.timeouts,
        (unsigned long)st.active_graph_version,
        (unsigned long)st.node_count,
        st.safe_or_fault ? "SAFE" : "RUN"
    );

    return send_json(req, json);
}

static esp_err_t api_graph_active_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "404 Not Found");
    return send_json(req, "{\"ok\":false,\"error\":\"No active graph stored\"}");
}

static esp_err_t api_ok_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t api_mem_info_handler(httpd_req_t *req)
{
    uint8_t rsp[256];
    uint16_t rsp_len = 0u;

    if (plc_client_mem_info(rsp, sizeof(rsp), &rsp_len) != ESP_OK) {
        return send_json(req, "{\"ok\":false}");
    }

    if (rsp_len < 20u) {
        return send_json(req, "{\"ok\":false}");
    }

    uint16_t bool_count = (uint16_t)(rsp[12] | (rsp[13] << 8u));
    uint16_t int_count = (uint16_t)(rsp[14] | (rsp[15] << 8u));
    uint16_t real_count = (uint16_t)(rsp[16] | (rsp[17] << 8u));

    char json[256];

    snprintf(
        json,
        sizeof(json),
        "{\"ok\":true,\"values\":{\"boolCount\":%u,\"intCount\":%u,\"realCount\":%u}}",
        bool_count,
        int_count,
        real_count
    );

    return send_json(req, json);
}

static esp_err_t api_mem_reset_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true,\"values\":true}");
}

static esp_err_t api_mem_read_handler(httpd_req_t *req)
{
    uint8_t rsp[1024];
    uint16_t rsp_len = 0u;

    if (plc_client_mem_read(0u, 0u, 8u, rsp, sizeof(rsp), &rsp_len) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"values\":[]}");
    }

    return send_json(req, "{\"ok\":true,\"values\":[]}");
}

static esp_err_t api_mem_write_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true,\"written\":0}");
}

static esp_err_t api_log_dump_handler(httpd_req_t *req)
{
    return send_json(req,
                     "{"
                     "\"total\":0,"
                     "\"from\":0,"
                     "\"count\":0,"
                     "\"items\":[]"
                     "}"
    );
}

static esp_err_t api_graph_upload_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "graph upload requested");

    return send_json(req,
                     "{"
                     "\"ok\":true,"
                     "\"stagingNodeCount\":0,"
                     "\"stagingCycleMs\":10"
                     "}"
    );
}

static esp_err_t api_graph_activate_handler(httpd_req_t *req)
{
    return send_json(req,
                     "{"
                     "\"ok\":true,"
                     "\"active\":true,"
                     "\"cycleMs\":10,"
                     "\"nodesCount\":0"
                     "}"
    );
}

static void register_get(httpd_handle_t server, const char *uri, esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t route = {
            .uri = uri,
            .method = HTTP_GET,
            .handler = handler,
            .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
}

static void register_post(httpd_handle_t server, const char *uri, esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t route = {
            .uri = uri,
            .method = HTTP_POST,
            .handler = handler,
            .user_ctx = NULL,
    };

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

    register_post(server, "/api/plc/output/force", api_ok_handler);
    register_post(server, "/api/plc/output/release", api_ok_handler);

    register_post(server, "/api/plc/mem/reset", api_mem_reset_handler);
    register_post(server, "/api/plc/mem/info", api_mem_info_handler);
    register_post(server, "/api/plc/mem/read", api_mem_read_handler);
    register_post(server, "/api/plc/mem/write/bool", api_mem_write_handler);
    register_post(server, "/api/plc/mem/write/int", api_mem_write_handler);
    register_post(server, "/api/plc/mem/write/real", api_mem_write_handler);

    ESP_LOGI(TAG, "API handlers registered");

    return ESP_OK;
}
