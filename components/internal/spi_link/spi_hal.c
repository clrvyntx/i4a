#include "config.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/spi_slave.h"

#define SPI_QUEUE_SIZE 40

#define SPI_SENDER_GPIO_MOSI 23
#define SPI_SENDER_GPIO_SCLK 18
#define SPI_SENDER_GPIO_CS 5

#define SPI_RECEIVER_GPIO_MOSI 13
#define SPI_RECEIVER_GPIO_SCLK 14
#define SPI_RECEIVER_GPIO_CS 15

#define SPI_SENDER_HOST VSPI_HOST
#define SPI_RECEIVER_HOST HSPI_HOST

#if CONFIG_SPI_FREQ_8M
#define SPI_FREQ SPI_MASTER_FREQ_8M
#elif CONFIG_SPI_FREQ_9M
#define SPI_FREQ SPI_MASTER_FREQ_9M
#elif CONFIG_SPI_FREQ_10M
#define SPI_FREQ SPI_MASTER_FREQ_10M
#elif CONFIG_SPI_FREQ_11M
#define SPI_FREQ SPI_MASTER_FREQ_11M
#elif CONFIG_SPI_FREQ_13M
#define SPI_FREQ SPI_MASTER_FREQ_13M
#elif CONFIG_SPI_FREQ_16M
#define SPI_FREQ SPI_MASTER_FREQ_16M
#elif CONFIG_SPI_FREQ_20M
#define SPI_FREQ SPI_MASTER_FREQ_20M
#elif CONFIG_SPI_FREQ_26M
#define SPI_FREQ SPI_MASTER_FREQ_26M
#elif CONFIG_SPI_FREQ_40M
#define SPI_FREQ SPI_MASTER_FREQ_40M
#elif CONFIG_SPI_FREQ_80M
#define SPI_FREQ SPI_MASTER_FREQ_80M
#else
#define SPI_FREQ SPI_MASTER_FREQ_8M
#endif

#ifndef PYSIM

static spi_device_handle_t s_spi_device_handle = {0};

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

esp_err_t hal_spi_init() {
    ESP_ERROR_CHECK(spi_rx_init());
    ESP_ERROR_CHECK(spi_tx_init());
    return ESP_OK;
}

esp_err_t hal_spi_send(const void *p, size_t len) {
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = p,
    };
    
    return spi_device_transmit(s_spi_device_handle, &t);
}

esp_err_t hal_spi_recv(void *p, size_t *len) {
    spi_slave_transaction_t t = {
        .length = *len * 8,
        .rx_buffer = p,
    };
    
    return spi_slave_transmit(SPI_RECEIVER_HOST, &t, portMAX_DELAY);
}


#else // PYSIM

#include "i4a_pysim.h"

esp_err_t hal_spi_init() {
    return ps_spi_init();
}

esp_err_t hal_spi_send(const void *p, size_t len) {
    return ps_spi_send(p, len);
}

esp_err_t hal_spi_recv(void *p, size_t *len) {
    return ps_spi_recv(p, len);
}

#endif // PYSIM
