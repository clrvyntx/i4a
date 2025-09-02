#ifndef __WIFI_H
#define __WIFI_H

#include <stdint.h>
#include <string.h>

#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"

#include "config.h"
#include "utils.h"


#define WIFI_AP_SSID CONFIG_WIFI_SSID
#define WIFI_AP_PASS CONFIG_WIFI_PASSWORD
#define WIFI_AP_CHANNEL CONFIG_WIFI_CHANNEL
#define WIFI_AP_MAX_STA_CONN CONFIG_WIFI_MAX_STA_CONN

#ifndef CONFIG_WIFI_AP_IP_FIRST_OCTET
#define CONFIG_WIFI_AP_IP_FIRST_OCTET 192
#endif

#ifndef CONFIG_WIFI_AP_IP_SECOND_OCTET
#define CONFIG_WIFI_AP_IP_SECOND_OCTET 170
#endif

#ifndef CONFIG_WIFI_AP_NETMASK_FIRST_OCTET
#define CONFIG_WIFI_AP_NETMASK_FIRST_OCTET 255
#endif

#ifndef CONFIG_WIFI_AP_NETMASK_SECOND_OCTET
#define CONFIG_WIFI_AP_NETMASK_SECOND_OCTET 255
#endif

#ifndef CONFIG_WIFI_AP_NETMASK_THIRD_OCTET
#define CONFIG_WIFI_AP_NETMASK_THIRD_OCTET 255
#endif

#ifndef CONFIG_WIFI_AP_NETMASK_FOURTH_OCTET
#define CONFIG_WIFI_AP_NETMASK_FOURTH_OCTET 0
#endif

esp_err_t wifi_init(void);
void wifi_ap_netif_init(void);
void wifi_sta_netif_init(void);
esp_netif_t * get_wifi_netif(void);
esp_err_t wifi_netif_init(void);
esp_ip4_addr_t get_wifi_ip_interface_address(void);


#endif
