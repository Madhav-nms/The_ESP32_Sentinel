#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"


#include "app_config.h"
#include "pir.h"
#include "wifi.h"
#include "mqtt.h"
#include "time_sync.h"
#include "ota.h"

static const char *TAG = "app";

// Module State 
static QueueHandle_t s_pir_queue;
static EventGroupHandle_t s_ev;
static bool s_mqtt_started = false; // to prevent double init on WiFi reconnect 

// OTA Task
static void ota_task(void *url)
{
    ota_mgr_handle_cmd_json((const char *)url, strlen((char *)url));
    free(url);  // free the allocated URL string
    vTaskDelete(NULL);  // delete task when done
}
// MQTT recieve callback
static void on_mqtt_rx(const char *topic, int topic_len,
                       const char *data,  int data_len)
{
    if (topic_len != (int)strlen(MQTT_CMD_TOPIC) &&
    memcmp(topic, MQTT_CMD_TOPIC, topic_len) != 0) {
        return;
    }
    // allocate a copy of payload for OTA task
    // Original data pointer is only valid during this callback
    char *payload = malloc(data_len + 1);
    if (!payload) {
        ESP_LOGE(TAG, "OTA: malloc failed — dropping command");
        return;
    }
    memcpy(payload, data, data_len);
    payload[data_len] = '\0';  // Null-terminate for string functions in ota.c

    // Spawn OTA task with 8KB stack — required for HTTPS + flash write buffers
    xTaskCreate(ota_task, "ota_task", 8192, payload, 5, NULL);
}
// WiFi IP Callback 
static void on_got_ip_start_tls_mqtt(void)
{
    if (s_mqtt_started) return;
    s_mqtt_started = true;

    time_sync(); // sync wall clock before publishing timestamps 
    mqtt_mgr_start(s_ev, on_mqtt_rx); // Connect to AWS IoT Core over mTLS
}
// Communication Task 
static void comm_task(void *arg)
{
    (void)arg;
    pir_event_t evt;
    TickType_t  last_stats = xTaskGetTickCount();

    while (1) {
        // 1s timeout instead of portMAX_DELAY so stats can print every 30s
        if (xQueueReceive(s_pir_queue, &evt, pdMS_TO_TICKS(1000)) == pdTRUE) {

            // Mirror PIR level on LED for visual feedback
            gpio_set_level((gpio_num_t)LED_GPIO, evt.level);
            if (evt.level != 1) continue;
            // Wait up to 10 seconds for both WiFi and MQTT to be ready 
            // If either is down, drop event 
            EventBits_t bits = xEventGroupWaitBits(
                s_ev,
                WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT,
                pdFALSE, pdTRUE,
                pdMS_TO_TICKS(10000)
            );

            if ((bits & (WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT)) !=
                        (WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT)) {
                ESP_LOGW(TAG, "Not connected — dropping event");
                continue;
            }
            // Format JSON payload with hardware timestamp 
            // ts_us was captured at ISR time 
            char payload[128];
            snprintf(payload, sizeof(payload),
                     "{\"event\":\"motion_detected\",\"ts_us\":%lld}",
                     (long long)evt.ts_us);
            // Publish at QoS 1
            int msg_id = esp_mqtt_client_publish(
                mqtt_mgr_get_client(), MQTT_TOPIC, payload, 0, 1, 0);
            ESP_LOGI(TAG, "Published msg_id=%d → %s", msg_id, payload);
        }

        // Print stats every 30 seconds
        if ((xTaskGetTickCount() - last_stats) >= pdMS_TO_TICKS(30000)) {
            uint32_t total, dropped;
            pir_get_stats(&total, &dropped);
            uint32_t captured = total - dropped;
            float    rate     = total ? (captured * 100.0f / total) : 100.0f;
            ESP_LOGI(TAG, "PIR Stats — Total: %lu  Captured: %lu  Dropped: %lu  Rate: %.1f%%",
                     total, captured, dropped, rate);
            last_stats = xTaskGetTickCount();
        }
    }
}


void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Starting.....");

    // NVS required for WiFi
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    
      // prevents roll back
    ota_mgr_mark_app_valid();

    s_ev = xEventGroupCreate();

    s_pir_queue = pir_init_and_get_queue();
    if (!s_pir_queue) {
        ESP_LOGE(TAG, "PIR init failed");
        return;
    }
    
    // Start WiFi; when IP is obtained, callback starts time sync + TLS MQTT
    wifi_mgr_start(s_ev, on_got_ip_start_tls_mqtt);

  
    // Reads PIR events from queue and publishes to AWS IoT Core 
    xTaskCreate(comm_task, "comm_task", 4096, NULL, 10, NULL);

}