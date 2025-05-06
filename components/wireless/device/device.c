#include <lwip/netdb.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#include "station.h"
#include <string.h>
#include "utils.c"
#include "../client/client.h"

#define DEFAULT_SCAN_LIST_SIZE 10
#define EXAMPLE_ESP_MAXIMUM_RETRY 10

static const char* LOGGING_TAG = "station";

static int s_retry_num = 0;

/*
 * @brief Initialize the WiFi stack in station mode
 */
void station_init(StationPtr stationPtr, const char* wifi_ssid_like, uint16_t orientation, char* device_uuid, const char* password) {

  strcpy(stationPtr->ssid_like, wifi_ssid_like);
  strcpy(stationPtr->device_uuid, device_uuid);
  strcpy(stationPtr->password, password);
  stationPtr->device_orientation = orientation;
  stationPtr->initialized = true;
  stationPtr->state = s_inactive;

  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

}

void station_find_ap(StationPtr stationPtr) {
  uint16_t number = DEFAULT_SCAN_LIST_SIZE;
  wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
  uint16_t ap_count = 0;
  memset(ap_info, 0, sizeof(ap_info));

  esp_wifi_scan_start(NULL, true);
  ESP_LOGI(LOGGING_TAG, "Max AP number ap_info can hold = %u", number);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  ESP_LOGI(LOGGING_TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
  
  uint16_t networks_to_scan;
  if (ap_count > number) {
    networks_to_scan = number;
  } else {
    networks_to_scan = ap_count;
  }

  for (int i = 0; i < networks_to_scan; i++) {
    // Verificamos el prefix de la red
    if (is_network_allowed(stationPtr->device_uuid, stationPtr->ssid_like, (char*)ap_info[i].ssid)) {
      uint16_t channel = ap_info[i].primary;
      if (is_channel_allowed(stationPtr->device_orientation, channel)) {
        ESP_LOGI(LOGGING_TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(LOGGING_TAG, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(LOGGING_TAG, "Channel \t\t%d", ap_info[i].primary);
        ESP_LOGI(LOGGING_TAG, "Index \t\t%u", i);
        memcpy(&stationPtr->wifi_ap_found, &ap_info[i], sizeof(ap_info[i]));
        stationPtr->ap_found = true;
        break;
      }
    }
  }

  if (!stationPtr->ap_found) {
    ESP_LOGI(LOGGING_TAG, "No AP found");
  } else {
    ESP_LOGI(LOGGING_TAG, "AP found");
    transform_wifi_ap_record_to_config(stationPtr);
  }
}

/*
 * @brief Event handler for WiFi events for station mode
 * @param arg The argument passed during handler registration
 * @param event_base The event base
 * @param event_id The event id
 * @param event_data The event data
 */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  StationPtr stationPtr = (StationPtr)arg;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
              event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(LOGGING_TAG, "Connection failed, retrying to connect to the AP");
    } else {
      ESP_LOGE(LOGGING_TAG, "Failed to connect after %d attempts", s_retry_num);
      s_retry_num = 0;
      stationPtr->ap_found = false;
      stationPtr->state = s_inactive;
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(LOGGING_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

    // Retrieve and print IP address, netmask, and gateway
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    ESP_LOGI(LOGGING_TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(LOGGING_TAG, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
    ESP_LOGI(LOGGING_TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    s_retry_num = 0;
    client_create();
  }
}

void station_start(StationPtr stationPtr) {
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
}

void station_connect(StationPtr stationPtr) {
  ESP_LOGI(LOGGING_TAG, "Connecting to %s...", stationPtr->wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &stationPtr->wifi_config));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, stationPtr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, stationPtr));
  ESP_ERROR_CHECK(esp_wifi_connect());
  stationPtr->state = s_active;
}

void station_disconnect(StationPtr stationPtr) {
  s_retry_num = EXAMPLE_ESP_MAXIMUM_RETRY + 1;
  stationPtr->state = s_inactive;
  ESP_ERROR_CHECK(esp_wifi_disconnect());
}

void station_restart(StationPtr stationPtr) {
  station_disconnect(stationPtr);
  station_start(stationPtr);
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
