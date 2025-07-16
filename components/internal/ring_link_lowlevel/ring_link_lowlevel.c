#include "ring_link_lowlevel.h"

static const char* TAG = "==> ring_link_lowlevel";

static SemaphoreHandle_t s_tx_semaphore_handle = NULL;

esp_err_t ring_link_lowlevel_init(QueueHandle_t **rx_queue) {
    ESP_ERROR_CHECK(RING_LINK_LOWLEVEL_IMPL_INIT(rx_queue));

    s_tx_semaphore_handle = xSemaphoreCreateMutex();

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

esp_err_t ring_link_lowlevel_free_rx_buffer(void *p)
{
    return RING_LINK_LOWLEVEL_IMPL_FREE_RX_BUFFER(p);
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


