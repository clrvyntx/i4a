#ifndef _I4A_PYSIM_H_
#define _I4A_PYSIM_H_

#include <stdint.h>

#include "pysim.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

void i4a_pysim_init();

/** -- config -- */
uint8_t ps_get_config_bits();
/** -- config -- */

/** -- spi -- */
esp_err_t ps_spi_init();
esp_err_t ps_spi_send(const void *p, size_t len);
esp_err_t ps_spi_recv(void *p, size_t *len);
/** -- spi -- */

/** -- wifi -- */
esp_err_t ps_wifi_init(const wifi_init_config_t *config);
esp_err_t ps_wifi_start(void);
esp_err_t ps_wifi_stop(void);
esp_err_t ps_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf);
esp_err_t ps_wifi_set_mode(wifi_mode_t mode);
esp_err_t ps_wifi_connect(void);
esp_err_t ps_wifi_disconnect(void);
esp_err_t ps_wifi_deauth_sta(uint16_t aid);
esp_err_t ps_wifi_scan_get_ap_num(uint16_t *number);
esp_err_t ps_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records);
esp_err_t ps_wifi_scan_start(const wifi_scan_config_t *config, bool block);
esp_err_t ps_wifi_ap_get_sta_list(wifi_sta_list_t *sta);
esp_err_t ps_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info);
esp_err_t ps_wifi_set_max_tx_power(int8_t power);
esp_netif_t* ps_netif_create_default_wifi_ap();
esp_netif_t* ps_netif_create_default_wifi_sta();
esp_err_t ps_netif_destroy_default_wifi(esp_netif_t*);
/** -- wifi -- */

// Replace esp_wifi_* functions with ps_wifi_*
#define esp_wifi_init ps_wifi_init
#define esp_wifi_start ps_wifi_start
#define esp_wifi_stop ps_wifi_stop
#define esp_wifi_set_config ps_wifi_set_config
#define esp_wifi_set_mode ps_wifi_set_mode
#define esp_wifi_connect ps_wifi_connect
#define esp_wifi_disconnect ps_wifi_disconnect
#define esp_wifi_deauth_sta ps_wifi_deauth_sta
#define esp_wifi_scan_get_ap_num ps_wifi_scan_get_ap_num
#define esp_wifi_scan_get_ap_records ps_wifi_scan_get_ap_records
#define esp_wifi_scan_start ps_wifi_scan_start
#define esp_wifi_ap_get_sta_list ps_wifi_ap_get_sta_list
#define esp_wifi_sta_get_ap_info ps_wifi_sta_get_ap_info
#define esp_wifi_set_max_tx_power ps_wifi_set_max_tx_power
#define esp_netif_create_default_wifi_ap ps_netif_create_default_wifi_ap
#define esp_netif_create_default_wifi_sta ps_netif_create_default_wifi_sta
#define esp_netif_destroy_default_wifi ps_netif_destroy_default_wifi

#endif // _I4A_PYSIM_H_
