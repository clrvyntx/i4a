#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "lwip/ip_addr.h"
#include "client.h"
#include "station.h"

#define SCAN_LIST_SIZE 10
#define MAX_RETRIES 10
#define RSSI_THRESHOLD -128 // Minimum RSSI (in dBm) required to consider an AP as available

static const char* LOGGING_TAG = "station";

static int s_retry_num = 0;

static bool is_network_allowed(char* device_uuid, char* network_prefix, char* network_name) {
  // Must contain the prefix
  if (strstr(network_name, network_prefix) == NULL) {
    return false;
  }

  // Must NOT contain the device UUID
  if (strstr(network_name, device_uuid) != NULL) {
    return false;
  }

  return true;
}

void station_init(StationPtr stationPtr, const char* wifi_ssid_like, uint16_t orientation, char* device_uuid, const char* password) {

  strcpy(stationPtr->ssid_like, wifi_ssid_like);
  strcpy(stationPtr->device_uuid, device_uuid);
  strcpy(stationPtr->password, password);
  stationPtr->device_orientation = orientation;
  stationPtr->initialized = true;
  stationPtr->state = s_inactive;
  stationPtr->ap_found = false;
  stationPtr->is_fully_connected = false;

  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);
  stationPtr->netif = sta_netif;

}

void station_find_ap(StationPtr stationPtr) {
  uint16_t number = SCAN_LIST_SIZE;
  wifi_ap_record_t ap_info[SCAN_LIST_SIZE];
  uint16_t ap_count = 0;
  memset(ap_info, 0, sizeof(ap_info));

  esp_wifi_scan_start(NULL, true);
  ESP_LOGI(LOGGING_TAG, "Max AP number ap_info can hold = %u", number);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  ESP_LOGI(LOGGING_TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);

  uint16_t networks_to_scan = (ap_count > number) ? number : ap_count;

  wifi_ap_record_t *best_ap = NULL;
  int best_rssi = RSSI_THRESHOLD;

  for (int i = 0; i < networks_to_scan; i++) {
    if (is_network_allowed(stationPtr->device_uuid, stationPtr->ssid_like, (char*)ap_info[i].ssid)) {
      ESP_LOGI(LOGGING_TAG, "Allowed SSID: %s | RSSI: %d | Channel: %d", ap_info[i].ssid, ap_info[i].rssi, ap_info[i].primary);
      if (ap_info[i].rssi > best_rssi) {
        best_ap = &ap_info[i];
        best_rssi = ap_info[i].rssi;
      }
    }
  }

  if (best_ap) {
    memcpy(&stationPtr->wifi_ap_found, best_ap, sizeof(*best_ap));
    stationPtr->ap_found = true;
    ESP_LOGI(LOGGING_TAG, "Best AP found: SSID: %s | RSSI: %d | Channel: %d", best_ap->ssid, best_ap->rssi, best_ap->primary);
    transform_wifi_ap_record_to_config(stationPtr);
  } else {
    ESP_LOGW(LOGGING_TAG, "No allowed APs found");
    stationPtr->ap_found = false;
  }

}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  StationPtr stationPtr = (StationPtr)arg;

  if (event_base == WIFI_EVENT) {
    switch (event_id) {
      case WIFI_EVENT_STA_DISCONNECTED:
        if(stationPtr->is_fully_connected){
          client_close();
        }
        if (s_retry_num < MAX_RETRIES) {
          esp_wifi_connect();
          s_retry_num++;
          ESP_LOGI(LOGGING_TAG, "Connection failed, retrying to connect to the AP");
        } else {
          ESP_LOGE(LOGGING_TAG, "Failed to connect, disconnecting");
          s_retry_num = 0;
          stationPtr->ap_found = false;
          stationPtr->state = s_inactive;
          stationPtr->is_fully_connected = false;
        }
        break;
    }
  }

  if (event_base == IP_EVENT) {
    switch (event_id) {
      case IP_EVENT_STA_GOT_IP:
        if(stationPtr->is_fully_connected){
          client_open();
          s_retry_num = 0;
        } else {
          ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
          esp_netif_ip_info_t s_learned_ip_info = event->ip_info;
          esp_netif_ip_info_t static_ip;

          uint32_t subnet_base_host = ntohl(s_learned_ip_info.ip.addr & s_learned_ip_info.netmask.addr);

          static_ip.gw.addr = htonl(subnet_base_host + 1);
          static_ip.ip.addr = htonl(subnet_base_host + 2);
          static_ip.netmask = s_learned_ip_info.netmask;

          stationPtr->is_fully_connected = true;
          esp_netif_dhcpc_stop(stationPtr->netif);
          ESP_ERROR_CHECK(esp_netif_set_ip_info(stationPtr->netif, &static_ip));
        }

        break;
    }
  }

}

void station_start(StationPtr stationPtr) {
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &(wifi_config_t){ 0 }));
  ESP_ERROR_CHECK(esp_wifi_start());
}

void station_connect(StationPtr stationPtr) {
  s_retry_num = 0;
  stationPtr->state = s_active;
  ESP_LOGI(LOGGING_TAG, "Connecting to %s...", stationPtr->wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_netif_dhcpc_start(stationPtr->netif));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &stationPtr->wifi_config));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, stationPtr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, stationPtr));
  ESP_ERROR_CHECK(esp_wifi_connect());
}

void station_disconnect(StationPtr stationPtr) {
  s_retry_num = MAX_RETRIES;
  ESP_ERROR_CHECK(esp_wifi_disconnect());
}

void station_stop(StationPtr stationPtr) {
  ESP_ERROR_CHECK(esp_wifi_stop());
}

void station_restart(StationPtr stationPtr) {
  station_disconnect(stationPtr);
  station_stop(stationPtr);
  station_start(stationPtr);
}

void station_destroy_netif(StationPtr stationPtr) {
  if (stationPtr->netif) {
    ESP_LOGW(LOGGING_TAG, "Destroying STA netif...");
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler);
    esp_netif_destroy_default_wifi(stationPtr->netif);
    stationPtr->netif = NULL;  // Prevent reuse or double free
  } else {
    ESP_LOGI(LOGGING_TAG, "AP netif already destroyed or not initialized.");
  }
}

bool station_is_initialized(StationPtr stationPtr) {
  return stationPtr->initialized;
}

bool station_is_active(StationPtr stationPtr) {
  return stationPtr->state == s_active;
}

bool station_found_ap(StationPtr stationPtr) {
  return stationPtr->ap_found;
};

void transform_wifi_ap_record_to_config(StationPtr stationPtr) {
  memcpy(stationPtr->wifi_config.sta.ssid, stationPtr->wifi_ap_found.ssid, sizeof(stationPtr->wifi_ap_found.ssid));
  memcpy(stationPtr->wifi_config.sta.bssid, stationPtr->wifi_ap_found.bssid, sizeof(stationPtr->wifi_ap_found.bssid));
  memcpy(stationPtr->wifi_config.sta.password, stationPtr->password, sizeof(stationPtr->password));
  stationPtr->wifi_config.sta.bssid_set = true;
}
