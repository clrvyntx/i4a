#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "ring_link.h"

static const char *TAG = "==> ring_link";

static QueueHandle_t *lowlevel_queue;
static QueueHandle_t *internal_queue;
static QueueHandle_t *esp_netif_queue;

static esp_err_t process_payload(ring_link_payload_t *p)
{
    QueueHandle_t *specific_queue;

    ESP_LOGD(TAG, "Received payload:");
    ESP_LOGD(TAG, "  buffer_type: 0x%02x", p->buffer_type);
    ESP_LOGD(TAG, "  id: %d", p->id);
    ESP_LOGD(TAG, "  src_id: %d", p->src_id);
    ESP_LOGD(TAG, "  dst_id: %d", p->dst_id);
    
    ESP_LOGD(TAG, "Checks:");
    ESP_LOGD(TAG, "  is_internal: %d (expect 0x%02x)", 
             ring_link_payload_is_internal(p), 
             RING_LINK_PAYLOAD_TYPE_INTERNAL);
    ESP_LOGD(TAG, "  is_esp_netif: %d (expect 0x%02x)", 
             ring_link_payload_is_esp_netif(p), 
             RING_LINK_PAYLOAD_TYPE_ESP_NETIF);

    if (ring_link_payload_is_internal(p))
    {
        ESP_LOGD(TAG, "Processing as internal payload");
        specific_queue = internal_queue;
    }
    else if (ring_link_payload_is_esp_netif(p))
    {
        ESP_LOGD(TAG, "Processing as esp_netif payload");
        specific_queue = esp_netif_queue;
    }
    else
    {
        ESP_LOGE(TAG, "Unknown payload type: '0x%02x'", p->buffer_type);
        ESP_LOGD(TAG, "Expected types: INTERNAL=0x%02x, ESP_NETIF=0x%02x",
                 RING_LINK_PAYLOAD_TYPE_INTERNAL,
                 RING_LINK_PAYLOAD_TYPE_ESP_NETIF);
        
        ESP_LOGW(TAG, "Received payload:");
        ESP_LOGW(TAG, "  id: %d", p->id);
        ESP_LOGW(TAG, "  ttl: %d", p->ttl);
        ESP_LOGW(TAG, "  src_id: %d", p->src_id);
        ESP_LOGW(TAG, "  dst_id: %d", p->dst_id);
        ESP_LOGW(TAG, "  len: %d", p->len);
        
        ring_link_lowlevel_free_rx_buffer(p);
        return ESP_FAIL;
    }
    if (xQueueSend(*specific_queue, &p, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Queue full, payload dropped");
        ring_link_lowlevel_free_rx_buffer(p);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void ring_link_process_task(void *pvParameters)
{
    ring_link_payload_t *payload;
    esp_err_t rc;
    
    while (true) {
        if (xQueueReceive(*lowlevel_queue, &payload, portMAX_DELAY) == pdTRUE) {
            rc = process_payload(payload);
            // ESP_ERROR_CHECK_WITHOUT_ABORT(rc);
            taskYIELD();
        }
    }
}


esp_err_t ring_link_init(void)
{
    #ifdef CONFIG_RING_LINK_LOWLEVEL_IMPL_SPI
    printf("CONFIG_RING_LINK_LOWLEVEL_IMPL_SPI\n");
    #endif

    #ifdef CONFIG_RING_LINK_LOWLEVEL_IMPL_UART
    printf("CONFIG_RING_LINK_LOWLEVEL_IMPL_UART\n");
    #endif


    ESP_ERROR_CHECK(ring_link_lowlevel_init(&lowlevel_queue));
    ESP_ERROR_CHECK(ring_link_internal_init(&internal_queue));
    ESP_ERROR_CHECK(ring_link_netif_init(&esp_netif_queue));

    BaseType_t ret = xTaskCreatePinnedToCore(
        ring_link_process_task,
        "ring_link_process",
        RING_LINK_NETIF_MEM_TASK,
        NULL,
        (tskIDLE_PRIORITY + 4),
        NULL,
        1
    );
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create process task");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}