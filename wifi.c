#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "wifi.h"
#include "app_config.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <string.h>
#include "esp_wifi.h"

static const char *TAG = "wifi";

// Module State 
static EventGroupHandle_t s_ev;
static wifi_on_got_ip_cb_t s_on_ip = NULL;

// Event Handler 
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; 

    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            //WiFi driver to initiate connection 
            ESP_LOGI(TAG, "WiFi started -> connect");
            esp_wifi_connect();
        } 
        else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d =
        (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG,
             "WiFi disconnected -> reconnect, reason=%d",
             d->reason);
             //clear bits to prevent comm_task from publishing 
             xEventGroupClearBits(s_ev, WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT);
              esp_wifi_connect(); //Retry connection automatically 
            }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_ev, WIFI_CONNECTED_BIT); // Signal Connectivity 

        if (s_on_ip) s_on_ip(); // Time sync + MQTT
    }
}

void wifi_mgr_start(EventGroupHandle_t evbits, wifi_on_got_ip_cb_t on_ip)
{
    s_ev = evbits;
    s_on_ip = on_ip;

    // Intialize TCP/IP stack and default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    //WiFi driver with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //Register event handler for WiFi and IP events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    //configure SSID and password from app_config.h
    wifi_config_t wifi_cfg = {0};
    strncpy((char*)wifi_cfg.sta.ssid, WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
    strncpy((char*)wifi_cfg.sta.password, WIFI_PASS, sizeof(wifi_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); // power save for low latency


    ESP_LOGI(TAG, "WiFi init done (SSID='%s')", WIFI_SSID);
}