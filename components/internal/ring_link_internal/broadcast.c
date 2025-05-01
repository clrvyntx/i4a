#include "broadcast.h"

static const char* TAG = "==> broadcast";
static SemaphoreHandle_t s_broadcast_mutex = NULL;
static TaskHandle_t s_broadcast_task = NULL;
static ring_link_payload_id_t s_id_counter = 0;

esp_err_t broadcast_init( void )
{
    s_broadcast_mutex = xSemaphoreCreateMutex();
    if( s_broadcast_mutex == NULL )
    {
        ESP_LOGE(TAG, "an error ocurred creating mutex.");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t send_broadcast(const void *buffer, uint16_t len){
    ring_link_payload_t p = {
        .id = s_id_counter ++,
        .ttl = RING_LINK_PAYLOAD_TTL,
        .src_id = config_get_id(),
        .dst_id = CONFIG_ID_ALL,
        .buffer_type = RING_LINK_PAYLOAD_TYPE_INTERNAL,
        .len = len,
    };
    if (len > RING_LINK_PAYLOAD_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Buffer length exceeds maximum allowed size.");
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(p.buffer, buffer, len);
    return ring_link_lowlevel_transmit_payload(&p);
}

bool broadcast_to_siblings(const void *msg, uint16_t len)
{
    if( xSemaphoreTake( s_broadcast_mutex, ( TickType_t ) 400 ) == pdTRUE )
    {
        bool result = false;
        s_broadcast_task = xTaskGetCurrentTaskHandle();
        if (send_broadcast(msg, len) == ESP_OK)
        {    
            result = ulTaskNotifyTake( pdTRUE, BROADCAST_TIMEOUT_MS / portTICK_PERIOD_MS ) == pdTRUE ? true : false;
        }
        s_broadcast_task = NULL;
        xSemaphoreGive( s_broadcast_mutex );
        return result;
    }
    ESP_LOGE(TAG, "Could not adquire Mutex...");
    return false;
}

esp_err_t broadcast_handler(ring_link_payload_t *p)
{
    // broadcast origin
    if (ring_link_payload_is_from_device(p))
    {
        ESP_LOGD(TAG, "Broadcast complete (src=%i,dest=%i,id=%i,ttl=%i).", p->src_id, p->dst_id, p->id, p->ttl);
        xTaskNotifyGive( s_broadcast_task );
        return ESP_OK;
    }
    else
    {
        ESP_ERROR_CHECK(ring_link_internal_process(p));
        return ring_link_lowlevel_forward_payload(p);
    }
}
