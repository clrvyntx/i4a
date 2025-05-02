#include "ring_link_internal.h"

static const char* TAG = "==> ring_link_internal";
static QueueHandle_t ring_link_internal_queue = NULL;


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

static void ring_link_internal_process_task(void *pvParameters)
{
    ring_link_payload_t *payload;
    esp_err_t rc;
    
    while (true) {
        if (xQueueReceive(ring_link_internal_queue, &payload, portMAX_DELAY) == pdTRUE) {
            rc = ring_link_internal_handler(payload);
            ESP_ERROR_CHECK_WITHOUT_ABORT(rc);
            free(payload);
        }
    }
}

esp_err_t ring_link_internal_init(QueueHandle_t **queue)
{
    ESP_ERROR_CHECK(broadcast_init());

    ring_link_internal_queue = xQueueCreate(RING_LINK_INTERNAL_QUEUE_SIZE, sizeof(ring_link_payload_t*));
    
    if (ring_link_internal_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queues");
        return ESP_FAIL;
    }
    *queue = &ring_link_internal_queue;

    BaseType_t ret = xTaskCreate(
        ring_link_internal_process_task,
        "ring_link_internal_process",
        RING_LINK_INTERNAL_MEM_TASK,
        NULL,
        (tskIDLE_PRIORITY + 2),
        NULL
    );
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create internal process task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ring_link_internal_process(ring_link_payload_t *p)
{
    ESP_LOGW(TAG, "call on_sibling_message(%s, %i)\n", p->buffer, p->len);
    return ESP_OK;
}