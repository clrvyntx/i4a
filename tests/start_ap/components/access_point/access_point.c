#include <lwip/netdb.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#include "access_point.h"
// #include "../server/server.h"

#include <string.h>

static const char *LOGGING_TAG = "AP";

void ap_init(AccessPointPtr ap, uint8_t wifi_channel, const char *wifi_ssid, const char *wifi_password, uint8_t wifi_max_sta_conn) {
  // Populate the Access Point wifi_config_t 
  strcpy((char *)ap->wifi_config.ap.ssid, wifi_ssid);
  ap->wifi_config.ap.ssid_len = strlen(wifi_ssid);
  strcpy((char *)ap->wifi_config.ap.password, wifi_password);
  ap->wifi_config.ap.channel = wifi_channel;
  ap->wifi_config.ap.max_connection = wifi_max_sta_conn;
  if (strlen(wifi_password) == 0) {
    ap->wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  } else {
    ap->wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  }
  ap->wifi_config.ap.pmf_cfg.required = true;
  // strcpy((char *)ap->network_cidr, network_cidr);
  esp_netif_t *netif = esp_netif_create_default_wifi_ap();
  ap->netif = netif;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap->wifi_config));
  // Register the event handler
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, NULL));
  ap->initialized = true;
}

bool ap_is_initialized(AccessPointPtr ap) {
  return ap->initialized;
}

bool ap_is_active(AccessPointPtr ap) {
  return ap->state == active;
}

void ap_print_info(AccessPointPtr ap) {
  ESP_LOGI(LOGGING_TAG, "wifi_config.ap.channel: %d", ap->wifi_config.ap.channel);
  ESP_LOGI(LOGGING_TAG, "wifi_config.ap.max_connection: %d", ap->wifi_config.ap.max_connection);
  ESP_LOGI(LOGGING_TAG, "wifi_config.ap.authmode: %d", ap->wifi_config.ap.authmode);
  ESP_LOGI(LOGGING_TAG, "wifi_config.ap.ssid: %s", ap->wifi_config.ap.ssid);
  ESP_LOGI(LOGGING_TAG, "wifi_config.ap.ssid_len: %d", ap->wifi_config.ap.ssid_len);
  ESP_LOGI(LOGGING_TAG, "wifi_config.ap.password: %s", ap->wifi_config.ap.password);
}

void ap_set_channel(AccessPointPtr ap, uint8_t channel) {
  ap->wifi_config.ap.channel = channel;
  ESP_LOGI(LOGGING_TAG, "Channel set on: %u", ap->channel);
};

void ap_set_ssid(AccessPointPtr ap, const char *ssid) {
  strcpy((char *)ap->wifi_config.ap.ssid, ssid);
  ap->wifi_config.ap.ssid_len = strlen(ssid);
};

void ap_set_password(AccessPointPtr ap, const char *password){
  strcpy((char *)ap->wifi_config.ap.password, password);
};

void ap_update(AccessPointPtr ap) {
  if (ap->state == active) {
    ap_stop(ap);
  }
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap->wifi_config));
  ap_start(ap);
};

void ap_set_network(AccessPointPtr ap, const char *network_cidr, const char *network_gateway, const char *network_mask) {
  ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap->netif));
  esp_netif_ip_info_t ip;
  memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
  ip.ip.addr = ipaddr_addr(network_cidr);
  ip.netmask.addr = ipaddr_addr(network_mask);
  ip.gw.addr = ipaddr_addr(network_gateway);
  esp_err_t result = esp_netif_set_ip_info(ap->netif, &ip);
  ESP_LOGI(LOGGING_TAG, "Setting ip info");
  ESP_ERROR_CHECK(result);
  if (result != ESP_OK) {
      ESP_LOGE(LOGGING_TAG, "Failed to set ip info");
      return;
  }
  esp_netif_dhcps_start(ap->netif);
};

void ap_start(AccessPointPtr ap) {
  ESP_LOGI(LOGGING_TAG, "Stoping AP");
  ap->state = active;
  ESP_ERROR_CHECK(esp_wifi_start());
};

void ap_stop(AccessPointPtr ap){
  ESP_LOGI(LOGGING_TAG, "Stoping AP");
  ap->state = inactive;
  ESP_ERROR_CHECK(esp_wifi_stop());
};

void ap_restart(AccessPointPtr ap) {
  ap_stop(ap);
  ap_start(ap);
};

void ap_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ESP_LOGI(LOGGING_TAG, "Event received: %s %ld", event_base, event_id);
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(LOGGING_TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(LOGGING_TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
    ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
    ESP_LOGI(LOGGING_TAG, "station ip:" IPSTR ", mac:" MACSTR "", IP2STR(&event->ip), MAC2STR(event->mac));
  }
}