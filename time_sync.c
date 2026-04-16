#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time_sync.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include <time.h>

static const char *TAG = "time";

void time_sync(void)
{
    ESP_LOGI(TAG, "Starting SNTP time sync...");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));

    time_t now = 0;
    struct tm timeinfo = {0};

    for (int i = 0; i < 30; i++) {
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year >= (2020 - 1900)) {
            ESP_LOGI(TAG, "Time synced: %04d-%02d-%02d %02d:%02d:%02d",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGW(TAG, "Time not confirmed yet (continuing anyway)");
}