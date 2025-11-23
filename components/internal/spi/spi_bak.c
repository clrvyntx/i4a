#include "spi.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"

#include "i4a_hal.h"


static const char* TAG = "==> SPI";
static QueueHandle_t spi_rx_queue = NULL;

static uint8_t spi_buffer[1600];
static bool buffer_free = true;

static void spi_polling_task(void *pvParameters) {
    void *payload = (void *)spi_buffer;

    while (1) {
        if (!buffer_free) {
            vTaskDelay(pdMS_TO_TICKS(10));
            taskYIELD();
            continue;
        }

        size_t len = 1600;
        esp_err_t err = hal_spi_recv(payload, &len);

        if (err == ESP_ERR_NOT_FOUND) {
            // No data
            vTaskDelay(pdMS_TO_TICKS(50));
            taskYIELD();
            continue;
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "hal_spi_recv failed: %u", err);
            vTaskDelay(pdMS_TO_TICKS(10));
            taskYIELD();
            continue;
        }

        buffer_free = false;
        xQueueSend(spi_rx_queue, &payload, portMAX_DELAY);
        taskYIELD();
    }
}

esp_err_t spi_init(QueueHandle_t **rx_queue) {
    ESP_LOGI(TAG, "spi_init called");
    esp_err_t err = hal_spi_init();
    if (err != ESP_OK) {
        return err;
    }
    
    spi_rx_queue = xQueueCreate(1, sizeof(void *));
    *rx_queue = &spi_rx_queue;
    xTaskCreatePinnedToCore(spi_polling_task, "spi_polling_task", 4096, NULL, 10, NULL, 1);
    return ESP_OK;
}

esp_err_t spi_transmit(void *p, size_t len) {
    ESP_LOGI(TAG, "spi_transmit(%p, %zu) called", p, len);
    hal_spi_send(p, len);
    return ESP_OK;
}

esp_err_t spi_free_rx_buffer(void *p)
{
    ESP_LOGI(TAG, "spi_free_rx_buffer(%p) called", p);
    if (buffer_free) {
        ESP_LOGE(TAG, "double free of spi buffer");
        abort();
    }
    buffer_free = true;
    return ESP_OK;
}
