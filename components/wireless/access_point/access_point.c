#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "lwip/ip_addr.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "server.h"
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"
#include "access_point.h"

#define DEFAULT_DNS "8.8.8.8"

static const char *LOGGING_TAG = "AP";

void ap_init(AccessPointPtr ap, uint8_t wifi_channel, const char *wifi_ssid, const char *wifi_password, uint8_t wifi_max_sta_conn, bool is_center) {
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
  ap->wifi_config.ap.pmf_cfg.required = false;
  ap->wifi_config.ap.pmf_cfg.capable = true;
  // strcpy((char *)ap->network_cidr, network_cidr);
  esp_netif_t *netif = esp_netif_create_default_wifi_ap();
  ap->netif = netif;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap->wifi_config));
  // Register the event handler
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, ap));
  ap->initialized = true;
  ap->is_center = is_center;
  ap->server_is_up = false;
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
  ap->channel = channel;
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

  dhcps_offer_t dhcps_dns_value = OFFER_DNS;
  ESP_ERROR_CHECK(esp_netif_dhcps_option(ap->netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value)));

  esp_netif_dns_info_t dnsserver;
  dnsserver.ip.u_addr.ip4.addr = ipaddr_addr(DEFAULT_DNS);
  dnsserver.ip.type = ESP_IPADDR_TYPE_V4;
  ESP_ERROR_CHECK(esp_netif_set_dns_info(ap->netif, ESP_NETIF_DNS_MAIN, &dnsserver));

  esp_netif_dhcps_start(ap->netif);
};

void ap_start(AccessPointPtr ap) {
  ESP_LOGI(LOGGING_TAG, "Starting AP");
  ap->state = active;
  ESP_ERROR_CHECK(esp_wifi_start());
};

void ap_stop(AccessPointPtr ap){
  ESP_LOGI(LOGGING_TAG, "Stopping AP");
  ap->state = inactive;
  ESP_ERROR_CHECK(esp_wifi_stop());
};

void ap_restart(AccessPointPtr ap) {
  ap_stop(ap);
  ap_start(ap);
};

void ap_destroy_netif(AccessPointPtr ap) {
  if (ap->netif) {
    ESP_LOGW(LOGGING_TAG, "Destroying AP netif...");
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler);
    esp_netif_destroy_default_wifi(ap->netif);
    ap->netif = NULL;  // Prevent reuse or double free
  } else {
    ESP_LOGI(LOGGING_TAG, "AP netif already destroyed or not initialized.");
  }
}

void ap_lock(AccessPointPtr ap) {
  ESP_LOGI(LOGGING_TAG, "Locking down AP (hiding SSID)");
  ap->wifi_config.ap.ssid_hidden = 1;
  ap_update(ap);
}

void ap_unlock(AccessPointPtr ap) {
  ESP_LOGI("AP", "Unlocking AP (broadcasting SSID)");
  ap->wifi_config.ap.ssid_hidden = 0;
  ap_update(ap);
}

void ap_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  AccessPointPtr ap = (AccessPointPtr)arg;
  if (event_base == WIFI_EVENT) {
    switch (event_id) {

    case WIFI_EVENT_AP_STACONNECTED:
      if (!ap->is_center) {
        server_create();
        ap->server_is_up = true;
      }
      break;

    case WIFI_EVENT_AP_STADISCONNECTED:
      if (!ap->is_center) {
        server_close();
        ap->server_is_up = false;
      }
      break;

    }
  }
}
