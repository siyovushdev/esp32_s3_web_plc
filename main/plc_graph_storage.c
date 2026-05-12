#include "plc_graph_storage.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static esp_err_t write_text_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return ESP_FAIL;
    }

    fwrite(text, 1u, strlen(text), f);
    fclose(f);
    return ESP_OK;
}

static esp_err_t read_text_file(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return ESP_FAIL;
    }

    size_t rd = fread(out, 1u, cap - 1u, f);
    fclose(f);

    out[rd] = '\0';
    return ESP_OK;
}

esp_err_t plc_graph_storage_save(const char *json,
                                 const char *sha256_hex)
{
    if (json == NULL || sha256_hex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t r = write_text_file(PLC_GRAPH_JSON_PATH, json);
    if (r != ESP_OK) {
        return r;
    }

    return write_text_file(PLC_GRAPH_SHA_PATH, sha256_hex);
}

esp_err_t plc_graph_storage_load(char *json,
                                 size_t json_cap,
                                 char *sha256_hex,
                                 size_t sha_cap)
{
    esp_err_t r = read_text_file(PLC_GRAPH_JSON_PATH, json, json_cap);
    if (r != ESP_OK) {
        return r;
    }

    return read_text_file(PLC_GRAPH_SHA_PATH, sha256_hex, sha_cap);
}

bool plc_graph_storage_exists(void)
{
    struct stat st;
    return stat(PLC_GRAPH_JSON_PATH, &st) == 0;
}
