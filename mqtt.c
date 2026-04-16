#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include <string.h>

static const char *TAG = "mqtt";
static EventGroupHandle_t s_ev;
static esp_mqtt_client_handle_t s_mqtt;
static mqtt_rx_cb_t s_rx_cb;

// TLS certificates
extern const uint8_t client_cert[]     asm(" ");
extern const uint8_t client_key[]      asm(" ");
extern const uint8_t root_ca[]         asm(" ");

// Event Handler 
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args; (void)base;

    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        xEventGroupSetBits(s_ev, MQTT_CONNECTED_BIT); // Signal comm_task to resume publishing
        esp_mqtt_client_subscribe(s_mqtt, MQTT_CMD_TOPIC, 1); // QoS = 1
        ESP_LOGI(TAG, "Subscribed: %s", MQTT_CMD_TOPIC);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        xEventGroupClearBits(s_ev, MQTT_CONNECTED_BIT); // clear bit so comm_task stops publishing until reconnected
        break;

    case MQTT_EVENT_DATA: {
        // topic and data are NOT null-terminated
        if (s_rx_cb && e->topic && e->data) {
            s_rx_cb(e->topic, e->topic_len, e->data, e->data_len);
        }
        break;

    }

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;

        default:
            break;
        }
}

void mqtt_mgr_start(EventGroupHandle_t evbits, mqtt_rx_cb_t rx_cb)
{
    s_ev = evbits;
    s_rx_cb = rx_cb;

    esp_mqtt_client_config_t cfg = {
        .broker = {
            .address.uri = MQTT_URI,
            .verification.certificate = (const char *)root_ca, //Root CA verfires if it is AWS IoT core
        },
        .credentials = {
            .client_id = "",
            .authentication = {
                .certificate = (const char *)client_cert,
                .key = (const char *)client_key,
            },
        },
    };
    s_mqtt = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt));

    ESP_LOGI(TAG, "MQTT starting (URI=%s)", MQTT_URI);

}

esp_mqtt_client_handle_t mqtt_mgr_get_client(void)
{
    return s_mqtt;
}