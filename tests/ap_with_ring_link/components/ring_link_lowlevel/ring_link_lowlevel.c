#include "ring_link_lowlevel.h"

static const char* TAG = "==> ring_link_lowlevel";

static SemaphoreHandle_t s_tx_semaphore_handle = NULL;
static QueueHandle_t ring_link_queue = NULL;


static void ring_link_receive_task(void *pvParameters)
{
    esp_err_t rc;
    ring_link_payload_t *payload;
    
    while (true) {
        payload = heap_caps_malloc(sizeof(ring_link_payload_t), MALLOC_CAP_DMA);
        if (payload == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for payload");
            continue;
        }
        
        memset(payload, 0, sizeof(ring_link_payload_t));
        rc = ring_link_lowlevel_receive_payload(payload);
        ESP_ERROR_CHECK_WITHOUT_ABORT(rc);
        if (rc == ESP_OK) {
            if (xQueueSend(ring_link_queue, &payload, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Queue full, payload dropped");
                free(payload);
            }
        } else {
            free(payload);
        }
    }
}

esp_err_t ring_link_lowlevel_init(QueueHandle_t **queue) {
    ESP_ERROR_CHECK(RING_LINK_LOWLEVEL_IMPL_INIT());
    s_tx_semaphore_handle = xSemaphoreCreateMutex();
    BaseType_t ret;
    
    ret = xTaskCreate(
        ring_link_receive_task,
        "ring_link_receive",
        RING_LINK_READ_MEM_TASK,
        NULL,
        (tskIDLE_PRIORITY + 4),
        NULL
    );
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create receive task");
        return ESP_FAIL;
    }

    ring_link_queue = xQueueCreate(RING_LINK_READ_QUEUE_SIZE, sizeof(ring_link_payload_t*));
    *queue = &ring_link_queue;
    if (ring_link_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ring_link_lowlevel_transmit_payload(ring_link_payload_t *p)
{
    esp_err_t rc;
    ESP_LOGD(TAG, "Payload puesto en queue:");
    ESP_LOGD(TAG, "  id: %d", p->id);
    ESP_LOGD(TAG, "  ttl: %d", p->ttl);
    ESP_LOGD(TAG, "  buffer_type: %d", p->buffer_type);
    ESP_LOGD(TAG, "  len: %d", p->len);
    ESP_LOGD(TAG, "  src_id: %d", p->src_id);
    ESP_LOGD(TAG, "  dst_id: %d\n", p->dst_id);
    if( xSemaphoreTake( s_tx_semaphore_handle, ( TickType_t ) 10 ) == pdTRUE )
    {
        rc = RING_LINK_LOWLEVEL_IMPL_TRANSMIT(p, sizeof(ring_link_payload_t));
        xSemaphoreGive( s_tx_semaphore_handle );
        return rc;
    }
    ESP_LOGE(TAG, "Could not adquire Mutex...");
    return ESP_FAIL;
}

esp_err_t ring_link_lowlevel_receive_payload(ring_link_payload_t *p)
{
    return RING_LINK_LOWLEVEL_IMPL_RECEIVE(p, sizeof(ring_link_payload_t));
}

esp_err_t ring_link_lowlevel_forward_payload(ring_link_payload_t *p)
{
    if (p->ttl <= 0) {
        ESP_LOGW(TAG, "Discarding packet (src=%i,dest=%i,id=%i,ttl=%i) due to TTL. Won't forward.",p->src_id, p->dst_id, p->id, p->ttl);
        return ESP_OK;
    }
    p->ttl--;
    ESP_LOGD(TAG, "Forwarding packet (src=%i,dest=%i,id=%i,ttl=%i).", p->src_id, p->dst_id, p->id, p->ttl);
    return ring_link_lowlevel_transmit_payload(p);
}


