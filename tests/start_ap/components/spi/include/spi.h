#pragma once

// C libraries
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// ESP32 libraries
#include "driver/spi_master.h"
#include "driver/spi_slave.h"


#ifdef __cplusplus
extern "C" {
#endif

#define SPI_BUFFER_SIZE CONFIG_SPI_BUFFER_SIZE
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
#endif

esp_err_t spi_init(void);
esp_err_t spi_transmit(void *p, size_t len);
esp_err_t spi_receive(void *p, size_t len);


#ifdef __cplusplus
}
#endif

