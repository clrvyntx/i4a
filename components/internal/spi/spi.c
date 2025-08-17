#include "spi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "ring_link_payload.h"

static const char* TAG = "==> SPI";

#define NUM_BUFFERS  8
#define TX_QUEUE_LEN NUM_BUFFERS

static spi_device_handle_t s_spi_device_handle = {0};

// Buffer pool for RX
static ring_link_payload_t buffer_pool[NUM_BUFFERS];
static QueueHandle_t free_buf_queue = NULL;
static QueueHandle_t spi_rx_queue = NULL;

// TX transaction pool
static spi_transaction_t tx_transaction_pool[NUM_BUFFERS];
static QueueHandle_t free_tx_queue = NULL;

// =======================
// Polling task for SPI RX
// =======================
static void spi_polling_task(void *pvParameters) {
    ring_link_payload_t *payload;

    while (1) {
        if (xQueueReceive(free_buf_queue, &payload, portMAX_DELAY) != pdTRUE) continue;

        spi_slave_transaction_t t = { 0 };
        t.length = SPI_BUFFER_SIZE * 8;
        t.rx_buffer = payload;

        esp_err_t ret = spi_slave_transmit(SPI_RECEIVER_HOST, &t, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to spi_slave_transmit");
            xQueueSend(free_buf_queue, &payload, 0);
        }

        taskYIELD();
    }
}

// ==================================
// Handle completed SPI RX transfers
// ==================================
void IRAM_ATTR spi_post_trans_cb(spi_slave_transaction_t *trans) {
    ring_link_payload_t *payload = (ring_link_payload_t *) trans->rx_buffer;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR(spi_rx_queue, &payload, &xHigherPriorityTaskWoken) != pdTRUE) {
        xQueueSendFromISR(free_buf_queue, &payload, &xHigherPriorityTaskWoken);
    }

    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

// ===============================
// Task to handle TX completions
// ===============================
static void spi_tx_task(void *arg) {
    while (1) {
        spi_transaction_t *trans;
        if (spi_device_get_trans_result(s_spi_device_handle, &trans, portMAX_DELAY) == ESP_OK) {
            // Recycle TX transaction structure
            xQueueSend(free_tx_queue, &trans, 0);
        }
    }
}

// ======================
// SPI RX Initialization
// ======================
static esp_err_t spi_rx_init() {
    spi_bus_config_t buscfg = {
        .mosi_io_num = SPI_RECEIVER_GPIO_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = SPI_RECEIVER_GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = SPI_RECEIVER_GPIO_CS,
        .queue_size = SPI_QUEUE_SIZE,
        .flags = 0,
        .post_trans_cb = spi_post_trans_cb
    };

    ESP_ERROR_CHECK(spi_slave_initialize(SPI_RECEIVER_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO));
    return ESP_OK;
}

// ======================
// SPI TX Initialization
// ======================
static esp_err_t spi_tx_init() {
    spi_bus_config_t buscfg = {
        .mosi_io_num = SPI_SENDER_GPIO_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = SPI_SENDER_GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = SPI_FREQ,
        .duty_cycle_pos = 128,
        .mode = 0,
        .spics_io_num = SPI_SENDER_GPIO_CS,
        .cs_ena_pretrans = 3,
        .cs_ena_posttrans = 10,
        .queue_size = TX_QUEUE_LEN
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI_SENDER_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_SENDER_HOST, &devcfg, &s_spi_device_handle));
    return ESP_OK;
}

// ====================
// SPI Module Init
// ====================
esp_err_t spi_init(QueueHandle_t **rx_queue) {
    ESP_ERROR_CHECK(spi_rx_init());
    ESP_ERROR_CHECK(spi_tx_init());

    free_buf_queue = xQueueCreate(NUM_BUFFERS, sizeof(ring_link_payload_t *));
    spi_rx_queue = xQueueCreate(NUM_BUFFERS, sizeof(ring_link_payload_t *));
    free_tx_queue = xQueueCreate(NUM_BUFFERS, sizeof(spi_transaction_t *));

    if (!free_buf_queue || !spi_rx_queue || !free_tx_queue) {
        ESP_LOGE(TAG, "Failed to create queues");
        return ESP_FAIL;
    }

    *rx_queue = &spi_rx_queue;

    // Initialize RX buffer pool
    for (int i = 0; i < NUM_BUFFERS; i++) {
        ring_link_payload_t *ptr = &buffer_pool[i];
        xQueueSend(free_buf_queue, &ptr, 0);

        // Init TX transaction pool
        spi_transaction_t *tx = &tx_transaction_pool[i];
        memset(tx, 0, sizeof(spi_transaction_t));
        xQueueSend(free_tx_queue, &tx, 0);
    }

    // Start tasks
    xTaskCreatePinnedToCore(spi_polling_task, "spi_polling_task", 4096, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(spi_tx_task, "spi_tx_task", 2048, NULL, 10, NULL, 1);

    return ESP_OK;
}

// =============================
// Non-blocking SPI Transmit
// =============================
esp_err_t spi_transmit(void *p, size_t len) {
    ring_link_payload_t *payload = (ring_link_payload_t *)p;
    spi_transaction_t *trans = NULL;

    if (xQueueReceive(free_tx_queue, &trans, 0) != pdTRUE) {
        ESP_LOGW(TAG, "No free TX transaction");
        return ESP_ERR_TIMEOUT;
    }

    memset(trans, 0, sizeof(spi_transaction_t));
    trans->length = len * 8;
    trans->tx_buffer = p;
    trans->user = p;  // Optionally keep track of buffer if needed

    ESP_LOGD(TAG, "Queuing TX - Type: 0x%02x, ID: %d, TTL: %d", payload->buffer_type, payload->id, payload->ttl);

    esp_err_t err = spi_device_queue_trans(s_spi_device_handle, trans, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue SPI TX");
        xQueueSend(free_tx_queue, &trans, 0); // Recycle
    }

    return err;
}

// ==================================
// Recycle RX Buffer Back to Pool
// ==================================
esp_err_t spi_free_rx_buffer(void *p) {
    return xQueueSend(free_buf_queue, &p, 0);
}
