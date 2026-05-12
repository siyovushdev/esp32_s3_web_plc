#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "wifi_ap.h"
#include "web_server.h"
#include "plc_client.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "ESP32-S3 Gateway starting...");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(plc_client_init());

    plc_client_start_polling();

    ESP_ERROR_CHECK(wifi_ap_start());
    ESP_ERROR_CHECK(web_server_start());
}
