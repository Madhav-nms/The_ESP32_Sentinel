#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "pir.h"
#include "app_config.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

static volatile uint32_t s_isr_total   = 0;  // every interrupt fired
static volatile uint32_t s_isr_dropped = 0;  // queue full = dropped

static const char *TAG = "pir";

static QueueHandle_t s_pir_queue;

static void IRAM_ATTR pir_isr_handler(void *arg)
{
    BaseType_t task_woken = pdFALSE;

    s_isr_total++;  // count every interrupt

    pir_event_t evt = {
        .ts_us = esp_timer_get_time(),                  //us hardware timestamp
        .level = gpio_get_level((gpio_num_t)PIR_GPIO),  //current GPIO level
    };

    if (xQueueSendFromISR(s_pir_queue, &evt, &hpw) != pdTRUE) {
        s_isr_dropped++;  // queue was full, event lost
    }

    if (task_woken) portYIELD_FROM_ISR();
}

QueueHandle_t pir_init_and_get_queue(void)
{
    // COnfiguring LED output
    gpio_config_t led_io = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&led_io));
    gpio_set_level((gpio_num_t)LED_GPIO, 0); // starts with LED off

    // Queue
    s_pir_queue = xQueueCreate(32, sizeof(pir_event_t));
    if (!s_pir_queue) {
        ESP_LOGE(TAG, "Failed to create PIR queue");
        return NULL;
    }

    // PIR input
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE, // trigger on both rising and falling edge
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    // ISR service & handler
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)PIR_GPIO, pir_isr_handler, NULL));

    ESP_LOGI(TAG, "PIR ISR installed on GPIO%d", PIR_GPIO);
    return s_pir_queue;
}

void pir_get_stats(uint32_t *total, uint32_t *dropped)
{
    *total   = s_isr_total;
    *dropped = s_isr_dropped;
}
