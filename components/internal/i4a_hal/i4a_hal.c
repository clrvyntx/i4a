#include "i4a_hal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "uart.h"

const char *TAG = "i4a_hal";

static SemaphoreHandle_t s_uart_lock = NULL;
static SemaphoreHandle_t s_uart_read_lock = NULL;
static SemaphoreHandle_t s_uart_write_lock = NULL;

static inline void uart_execute(uint32_t cmd, uint32_t payload_size, const void *payload) {
    while (!xSemaphoreTake(s_uart_write_lock, portMAX_DELAY));
    write_all(&cmd, sizeof(uint32_t));
    write_all(&payload_size, sizeof(uint32_t));
    if (payload_size > 0) {
        write_all(payload, payload_size);
    }
    xSemaphoreGive(s_uart_write_lock);
}

static inline uint32_t uart_query(uint32_t cmd) {
    uart_execute(cmd, 0, NULL);

    while (!xSemaphoreTake(s_uart_read_lock, portMAX_DELAY));
    uint32_t result = 0;
    read_exact(&result, sizeof(uint32_t));
    xSemaphoreGive(s_uart_read_lock);
    return result;
}

void i4a_hal_init() {
    s_uart_lock = xSemaphoreCreateMutex();
    s_uart_read_lock = xSemaphoreCreateMutex();
    s_uart_write_lock = xSemaphoreCreateMutex();
    if(!s_uart_lock || !s_uart_read_lock || !s_uart_write_lock) {
        ESP_LOGE(TAG, "cant create mutex");
        abort();
    }

    setup_uart();
}

uint32_t hal_get_config_bits() {
    while (!xSemaphoreTake(s_uart_lock, portMAX_DELAY));
    uint32_t ret = uart_query(0x03);
    xSemaphoreGive(s_uart_lock);
    return ret;
}

esp_err_t hal_spi_init() {
    return ESP_OK;
}

esp_err_t hal_spi_send(const void *p, size_t len) {
    while (!xSemaphoreTake(s_uart_lock, portMAX_DELAY));
    uart_execute(0x01, (uint32_t) len, p);
    xSemaphoreGive(s_uart_lock);
    return ESP_OK;
}

static esp_err_t _hal_spi_recv(void *p, size_t *len) {
    uint32_t payload_size = uart_query(0x02);
    if (!payload_size) {
        return ESP_ERR_NOT_FOUND;
    }

    if (payload_size > *len) {
        ESP_LOGE(TAG, "Got payload bigger than buffer size (%zu bytes) -- abort", payload_size);
        abort();
    }
    
    *len = payload_size;
    read_exact(p, payload_size);
    return ESP_OK;
}

esp_err_t hal_spi_recv(void *p, size_t *len) {
    // Enter long poll
    while (!xSemaphoreTake(s_uart_lock, portMAX_DELAY));  // -- cant use any other function
    while (!xSemaphoreTake(s_uart_read_lock, portMAX_DELAY));  // -- read exclusive for us
    uart_execute(0x04, 0, NULL); // enter long poll
    xSemaphoreGive(s_uart_lock); // allow execution of other commands, but still hold read lock

    uint32_t result = 0;
    read_exact(&result, sizeof(uint32_t));
    xSemaphoreGive(s_uart_read_lock); // -- long poll finished, return read lock

    while (!xSemaphoreTake(s_uart_lock, portMAX_DELAY)); // -- regrab uart lock and do regular spi read
    esp_err_t ret = _hal_spi_recv(p, len);
    xSemaphoreGive(s_uart_lock);
    return ret;
}
