#include "web_server.h"
#include "api_handlers.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_littlefs.h"

static const char *TAG = "WEB_SERVER";

#define WEB_BASE_PATH "/littlefs"
#define FILE_PATH_MAX 256
#define SCRATCH_BUFSIZE 4096

static httpd_handle_t server = NULL;
static char scratch[SCRATCH_BUFSIZE];

static const char *get_content_type(const char *filename)
{
    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".css"))  return "text/css";
    if (strstr(filename, ".js"))   return "application/javascript";
    if (strstr(filename, ".json")) return "application/json";
    if (strstr(filename, ".png"))  return "image/png";
    if (strstr(filename, ".jpg"))  return "image/jpeg";
    if (strstr(filename, ".svg"))  return "image/svg+xml";
    if (strstr(filename, ".ico"))  return "image/x-icon";
    return "text/plain";
}

static esp_err_t static_file_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    const char *uri = req->uri;

    if (strncmp(uri, "/api/", 5) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "API route not found");
        return ESP_FAIL;
    }

    if (strstr(uri, "..") != NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
        return ESP_FAIL;
    }

    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    int written = snprintf(filepath, sizeof(filepath), "%s%s", WEB_BASE_PATH, uri);
    if (written < 0 || written >= sizeof(filepath)) {
        httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "URI too long");
        return ESP_FAIL;
    }

    struct stat file_stat;
    bool is_spa_fallback = false;

    if (stat(filepath, &file_stat) != 0) {
        written = snprintf(filepath, sizeof(filepath), "%s/index.html", WEB_BASE_PATH);
        if (written < 0 || written >= sizeof(filepath)) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Path error");
            return ESP_FAIL;
        }

        if (stat(filepath, &file_stat) != 0) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "index.html not found");
            return ESP_FAIL;
        }

        is_spa_fallback = true;
    }

    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        return ESP_FAIL;
    }

    if (is_spa_fallback) {
        httpd_resp_set_type(req, "text/html");
    } else {
        httpd_resp_set_type(req, get_content_type(filepath));
    }

    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    size_t read_bytes;
    while ((read_bytes = fread(scratch, 1, SCRATCH_BUFSIZE, file)) > 0) {
        if (httpd_resp_send_chunk(req, scratch, read_bytes) != ESP_OK) {
            fclose(file);
            return ESP_FAIL;
        }
    }

    fclose(file);

    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t mount_littlefs(void)
{
    esp_vfs_littlefs_conf_t conf = {
            .base_path = WEB_BASE_PATH,
            .partition_label = "storage",
            .format_if_mount_failed = true,
            .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_littlefs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted. total=%u, used=%u", (unsigned)total, (unsigned)used);
    }

    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    ESP_ERROR_CHECK(mount_littlefs());

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 32;
    config.stack_size = 16384;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(api_register_handlers(server));

    httpd_uri_t static_files = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = static_file_handler,
            .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &static_files));

    ESP_LOGI(TAG, "HTTP server started on port 80");

    return ESP_OK;
}
