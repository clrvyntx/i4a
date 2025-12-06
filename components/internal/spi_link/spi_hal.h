#ifndef _SPI_LINK_HAL_H_
#define _SPI_LINK_HAL_H_

esp_err_t hal_spi_init();
esp_err_t hal_spi_send(const void *p, size_t len);
esp_err_t hal_spi_recv(void *p, size_t *len);

#endif // _SPI_LINK_HAL_H_
