#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "ring_link.h"

static const char *TAG = "==> ring_link";

static QueueHandle_t *lowlevel_queue;

static esp_err_t ring_link_internal_process(ring_link_payload_t *p)
{
    on_sibling_message(p->buffer, p->len);
    return ESP_OK;
}

static esp_err_t ring_link_internal_handler(ring_link_payload_t *p)
{
    if (ring_link_payload_is_broadcast(p))  // broadcast
    {
        return broadcast_handler(p);
    }
    else if (ring_link_payload_is_for_device(p))  // payload for me
    {
        return ring_link_internal_process(p);
    }
    else  // not for me, forwarding
    {
        return ring_link_lowlevel_forward_payload(p);
    }
}

static esp_err_t process_payload(ring_link_payload_t *p)
{
    ESP_LOGI(TAG, "Received payload:");
    ESP_LOGI(TAG, "  id: %d", p->id);
    ESP_LOGI(TAG, "  src_id: %d", p->src_id);
    ESP_LOGI(TAG, "  dst_id: %d", p->dst_id);

    return ring_link_internal_handler(p);
}

static void ring_link_process_task(void *pvParameters)
{
    ring_link_payload_t *payload;
    esp_err_t rc;

    while (true) {
        if (xQueueReceive(*lowlevel_queue, &payload, portMAX_DELAY) == pdTRUE) {
            rc = process_payload(payload);
            ESP_ERROR_CHECK_WITHOUT_ABORT(rc);
        }
    }
}

esp_err_t ring_link_init(void)
{
	
    ESP_ERROR_CHECK(ring_link_lowlevel_init(&lowlevel_queue));
    ESP_ERROR_CHECK(broadcast_init());

    BaseType_t ret = xTaskCreate(
        ring_link_process_task,
        "ring_link_process",
        RING_LINK_MEM_TASK,
        NULL,
        (tskIDLE_PRIORITY + 4),
        NULL
    );
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create process task");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

void on_sibling_message(char *buffer, uint8_t len){
    // sibling callback
}
