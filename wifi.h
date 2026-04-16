#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

typedef void (*wifi_on_got_ip_cb_t)(void);

void wifi_mgr_start(EventGroupHandle_t evbits, wifi_on_got_ip_cb_t on_ip);