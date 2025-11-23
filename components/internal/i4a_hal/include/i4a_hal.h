#ifndef _I4A_HAL_
#define _I4A_HAL_

#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

void i4a_hal_init();

/** -- config -- */
uint32_t hal_get_config_bits();
/** -- config -- */

/** -- spi -- */
esp_err_t hal_spi_init();
esp_err_t hal_spi_send(const void *p, size_t len);
esp_err_t hal_spi_recv(void *p, size_t *len);
/** -- spi -- */

/** -- wifi -- */
void hal_wifi_init();
esp_err_t hal_wifi_set_mode(wifi_mode_t mode);
/** -- wifi -- */

#endif // _I4A_HAL_
