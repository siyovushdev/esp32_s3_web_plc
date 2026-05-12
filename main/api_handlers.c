#include "api_handlers.h"

#include "esp_log.h"

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
    const char *json =
            "{"
            "\"connection\":{"
            "\"ip\":\"192.168.4.1\","
            "\"port\":0,"
            "\"mode\":\"STOP\","
            "\"uptime\":\"00:00:00\","
            "\"lastExchangeAgo\":\"никогда\","
            "\"linkStatus\":\"NO_LINK\""
            "},"
            "\"performance\":{"
            "\"scanAvgMs\":0,"
            "\"scanMaxMs\":0,"
            "\"scanAvgUs\":0,"
            "\"scanMaxUs\":0,"
            "\"workAvgMs\":0,"
            "\"workMaxMs\":0,"
            "\"workAvgUs\":0,"
            "\"workMaxUs\":0,"
            "\"cycleRealAvgMs\":0,"
            "\"cycleRealMaxMs\":0,"
            "\"scanLimitMs\":10,"
            "\"cpuLoadPercent\":0,"
            "\"memoryUsagePercent\":0,"
            "\"crcErrors\":0,"
            "\"timeouts\":0,"
            "\"scanLongSteps\":0,"
            "\"workLongSteps\":0"
            "},"
            "\"ioSummary\":{"
            "\"diUsed\":0,"
            "\"diTotal\":8,"
            "\"doUsed\":0,"
            "\"doTotal\":8,"
            "\"aiUsed\":0,"
            "\"aiTotal\":0,"
            "\"pwmUsed\":0,"
            "\"pwmTotal\":0"
            "},"
            "\"activeGraph\":{"
            "\"name\":\"active_graph\","
            "\"version\":\"—\","
            "\"nodes\":0,"
            "\"connections\":0,"
            "\"inputs\":0,"
            "\"outputs\":0,"
            "\"compileErrors\":0,"
            "\"activatedAt\":\"—\","
            "\"runState\":\"STOP\""
            "},"
            "\"alarms\":[],"
            "\"nodes\":[],"
            "\"isLoading\":false,"
            "\"errorMessage\":null"
            "}";

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
    return send_json(req,
                     "{"
                     "\"ok\":true,"
                     "\"values\":{"
                     "\"boolCount\":64,"
                     "\"intCount\":64,"
                     "\"realCount\":64"
                     "}"
                     "}"
    );
}

static esp_err_t api_mem_reset_handler(httpd_req_t *req)
{
    return send_json(req, "{\"ok\":true,\"values\":true}");
}

static esp_err_t api_mem_read_handler(httpd_req_t *req)
{
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