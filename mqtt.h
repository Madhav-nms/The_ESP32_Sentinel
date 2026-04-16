#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"

typedef void (*mqtt_rx_cb_t)(const char *topic, int topic_len,
                            const char *data, int data_len);
                            
void mqtt_mgr_start(EventGroupHandle_t evbits, mqtt_rx_cb_t rx_cb);
esp_mqtt_client_handle_t mqtt_mgr_get_client(void);