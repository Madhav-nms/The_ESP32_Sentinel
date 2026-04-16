#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void pir_get_stats(uint32_t *total, uint32_t *dropped);

typedef struct {
    int64_t ts_us; // hardware timestamp in us
    int level; // GPIO level, 1: rising, 0:falling
} pir_event_t;

QueueHandle_t pir_init_and_get_queue(void);

void pir_get_status(uint32_t *total, uint32_t *dropped);