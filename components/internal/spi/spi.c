#include "spi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "ring_link_payload.h"


static spi_device_handle_t s_spi_device_handle = {0};
static const char* TAG = "==> SPI";

#define NUM_BUFFERS  8

static ring_link_payload_t buffer_pool[NUM_BUFFERS];        // Buffers preasignados
static QueueHandle_t free_buf_queue = NULL;       // Buffers libres
static QueueHandle_t spi_rx_queue = NULL;         // Mensajes recibidos

static void spi_polling_task(void *pvParameters) {
    ring_link_payload_t *payload;

    while (1) {
        // Esperar un buffer libre del pool
        if (xQueueReceive(free_buf_queue, &payload, portMAX_DELAY) != pdTRUE) continue;

        spi_slave_transaction_t t = { 0 };
        t.length = SPI_BUFFER_SIZE * 8;
        t.rx_buffer = payload;

        esp_err_t ret = spi_slave_transmit(SPI_RECEIVER_HOST, &t, portMAX_DELAY);
        if (ret != ESP_OK) {
            // Si hubo error, devolver el buffer al pool
            ESP_LOGE(TAG, "Failed to spi_slave_queue_trans");
            xQueueSend(free_buf_queue, &payload, 0);
        }
        taskYIELD();
    }
}

void IRAM_ATTR spi_post_trans_cb(spi_slave_transaction_t *trans) {
    ring_link_payload_t *payload = (ring_link_payload_t *) trans->rx_buffer;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR(spi_rx_queue, &payload, &xHigherPriorityTaskWoken) != pdTRUE) {
        // Cola llena, descartar el mensaje (¡devolver buffer!)
        xQueueSendFromISR(free_buf_queue, &payload, &xHigherPriorityTaskWoken);
    }

    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

static esp_err_t spi_rx_init() {
    //Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=SPI_RECEIVER_GPIO_MOSI,
        .miso_io_num=-1,
        .sclk_io_num=SPI_RECEIVER_GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    //Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg={
        .mode=0,
        .spics_io_num=SPI_RECEIVER_GPIO_CS,
        .queue_size=SPI_QUEUE_SIZE,
        .flags=0,
        //.post_setup_cb=spi_post_setup_cb,
        .post_trans_cb=spi_post_trans_cb
    };

    //Initialize SPI slave interface
    ESP_ERROR_CHECK(spi_slave_initialize(SPI_RECEIVER_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO));
    return ESP_OK;
}

static esp_err_t spi_tx_init() {
    //Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=SPI_SENDER_GPIO_MOSI,
        .miso_io_num=-1,
        .sclk_io_num=SPI_SENDER_GPIO_SCLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };

    //Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg={
        .command_bits=0,
        .address_bits=0,
        .dummy_bits=0,
        .clock_speed_hz=SPI_FREQ,
        .duty_cycle_pos=128,        //50% duty cycle
        .mode=0,
        .spics_io_num=SPI_SENDER_GPIO_CS,
        .cs_ena_pretrans = 3,   
        .cs_ena_posttrans = 10,        //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size=SPI_QUEUE_SIZE
    };

    //Initialize the SPI bus and add the device we want to send stuff to.
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_SENDER_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_SENDER_HOST, &devcfg, &s_spi_device_handle));
    return ESP_OK;
}

esp_err_t spi_init(QueueHandle_t **rx_queue) {

    ESP_ERROR_CHECK(spi_rx_init());
    ESP_ERROR_CHECK(spi_tx_init());

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
    ring_link_payload_t* payload = (ring_link_payload_t*)p;
    ESP_LOGD(TAG, "Pre-transmit payload - Type: 0x%02x, ID: %d, TTL: %d", 
             payload->buffer_type, payload->id, payload->ttl);
             
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = p,
    };
    return spi_device_transmit(s_spi_device_handle, &t);
}

esp_err_t spi_free_rx_buffer(void *p)
{
    xQueueSend(free_buf_queue, &p, 0);
    return ESP_OK;
}