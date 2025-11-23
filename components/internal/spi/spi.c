#include "spi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "i4a_hal.h"
#include "ring_link_payload.h"


static const char* TAG = "==> SPI";

#define NUM_BUFFERS  8

DMA_ATTR static ring_link_payload_t buffer_pool[NUM_BUFFERS] __attribute__((aligned(4)));  // Buffers preasignados
static QueueHandle_t free_buf_queue = NULL;       // Buffers libres
static QueueHandle_t spi_rx_queue = NULL;         // Mensajes recibidos

static void spi_polling_task(void *pvParameters) {
    ring_link_payload_t *payload;

    while (1) {
        // Esperar un buffer libre del pool
        if (xQueueReceive(free_buf_queue, &payload, portMAX_DELAY) != pdTRUE) continue;

        size_t len = SPI_BUFFER_SIZE;
        esp_err_t ret = hal_spi_recv(payload, &len);
        if (ret == ESP_OK) {
            if (xQueueSend(spi_rx_queue, &payload, 0) != pdTRUE) {
                // Cola llena, descartar el mensaje (¡devolver buffer!)
                xQueueSend(free_buf_queue, &payload, 0);
            }
        } else if (ret == ESP_ERR_NOT_FOUND) {
            // Empty frame - wait a few ms and retry
            xQueueSend(free_buf_queue, &payload, 0);
            vTaskDelay(pdMS_TO_TICKS(10)); 
        } else {
            // Si hubo error, devolver el buffer al pool
            ESP_LOGE(TAG, "SPI recv failed: %u", ret);
            xQueueSend(free_buf_queue, &payload, 0);
        }
        taskYIELD();
    }
}

esp_err_t spi_init(QueueHandle_t **rx_queue) {

    ESP_ERROR_CHECK(hal_spi_init());

    // inicializo punteros a queues
    free_buf_queue = xQueueCreate(NUM_BUFFERS, sizeof(ring_link_payload_t *));
    spi_rx_queue = xQueueCreate(NUM_BUFFERS, sizeof(ring_link_payload_t *));
    if (free_buf_queue == NULL || spi_rx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return ESP_FAIL;
    }
    *rx_queue = &spi_rx_queue;

    // Inicializar pool de buffers
    for (int i = 0; i < NUM_BUFFERS; i++) {
        ring_link_payload_t *ptr = &buffer_pool[i];
        xQueueSend(free_buf_queue, &ptr, 0);
    }
    // inicializo tarea de polling
    xTaskCreatePinnedToCore(spi_polling_task, "spi_polling_task", 4096, NULL, 10, NULL, 1);

    return ESP_OK;
}


esp_err_t spi_transmit(void *p, size_t len) {
    return hal_spi_send(p, len);
}

esp_err_t spi_free_rx_buffer(void *p)
{
    xQueueSend(free_buf_queue, &p, 0);
    return ESP_OK;
}