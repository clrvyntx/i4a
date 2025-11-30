#ifndef _I4A_HAL_
#define _I4A_HAL_

#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

void i4a_hal_init();

/** -- config -- */
uint8_t hal_get_config_bits();
/** -- config -- */

/** -- spi -- */
esp_err_t hal_spi_init();
esp_err_t hal_spi_send(const void *p, size_t len);
esp_err_t hal_spi_recv(void *p, size_t *len);
/** -- spi -- */

/** -- wifi -- */
esp_err_t hal_wifi_init(const wifi_init_config_t *config);
esp_err_t hal_wifi_start(void);
esp_err_t hal_wifi_stop(void);
esp_err_t hal_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf);
esp_err_t hal_wifi_set_mode(wifi_mode_t mode);
esp_err_t hal_wifi_connect(void);
esp_err_t hal_wifi_disconnect(void);
esp_err_t hal_wifi_deauth_sta(uint16_t aid);
esp_err_t hal_wifi_scan_get_ap_num(uint16_t *number);
esp_err_t hal_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records);
esp_err_t hal_wifi_scan_start(const wifi_scan_config_t *config, bool block);
esp_err_t hal_wifi_ap_get_sta_list(wifi_sta_list_t *sta);
esp_err_t hal_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info);
esp_netif_t* hal_netif_create_default_wifi_ap();
esp_netif_t* hal_netif_create_default_wifi_sta();
esp_err_t hal_netif_destroy_default_wifi(esp_netif_t*);
esp_err_t hal_netif_destroy_default_wifi(esp_netif_t*);
/** -- wifi -- */

#endif // _I4A_HAL_
