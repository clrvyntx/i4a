#pragma once


// ESP32 libraries
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ring_link_payload.h"
#include "ring_link_lowlevel_impl.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif


#define RING_LINK_READ_MEM_TASK 16384 * 2
#define RING_LINK_READ_QUEUE_SIZE 40

esp_err_t ring_link_lowlevel_init(QueueHandle_t **queue);

esp_err_t ring_link_lowlevel_transmit_payload(ring_link_payload_t *p);

esp_err_t ring_link_lowlevel_forward_payload(ring_link_payload_t *p);

esp_err_t ring_link_lowlevel_free_rx_buffer(void *p);

#ifdef __cplusplus
}
#endif

