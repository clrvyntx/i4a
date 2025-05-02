#include "esp_event.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>
#include <unistd.h>

#include "device.h"

#define LOGGING_TAG "DEVICE"

void device_init(DevicePtr device_ptr, const char *device_uuid, uint8_t device_orientation, const char *wifi_network_prefix, const char *wifi_network_password, uint8_t ap_channel_to_emit, uint8_t ap_max_sta_connections, uint8_t device_is_root, Device_Mode mode) {
  device_ptr->mode = NAN;
  device_ptr->state = d_inactive;
  device_ptr->device_is_root = device_is_root;

  AccessPoint ap = {};
  device_ptr->access_point = ap;
  device_ptr->access_point_ptr = &device_ptr->access_point;

  Station station = {};
  device_ptr->station = station;
  device_ptr->station_ptr = &device_ptr->station; 

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  if (mode == AP) {
    device_init_ap(device_ptr, ap_channel_to_emit, wifi_network_prefix, device_uuid, wifi_network_password, ap_max_sta_connections);
  } else if (mode == STATION) {
    device_init_station(device_ptr, wifi_network_prefix, device_orientation, device_uuid, wifi_network_password);
  }

}

void device_init_ap(DevicePtr device_ptr, uint8_t channel, const char *wifi_network_prefix ,const char *device_uuid, const char *password, uint8_t max_sta_connections) {
  // Generate the wifi ssid
  char wifi_ssid[32];
  memset(wifi_ssid, 0, sizeof(wifi_ssid));
  strcpy(wifi_ssid, wifi_network_prefix);
  strcat(wifi_ssid, "_");
  strcat(wifi_ssid, device_uuid);

  ESP_LOGI(LOGGING_TAG, "Initializing AP with SSID: %s", wifi_ssid);
  
  ap_init(device_ptr->access_point_ptr, channel, wifi_ssid, password, max_sta_connections);
};

void device_init_station(DevicePtr device_ptr, const char* wifi_ssid_like, uint16_t orientation, char* device_uuid, const char* password) {
  station_init(device_ptr->station_ptr, wifi_ssid_like, orientation, device_uuid, password);
};

void device_set_network_ap(DevicePtr device_ptr, const char *network_cidr, const char *network_gateway, const char *network_mask) {
  ap_set_network(device_ptr->access_point_ptr, network_cidr, network_gateway, network_mask);
};

void device_reset(DevicePtr device_ptr) {
  if (device_ptr->state == d_active) {
    if (device_ptr->mode == AP) {
      device_stop_ap(device_ptr);
    } else if (device_ptr->mode == STATION) {
      device_disconnect_station(device_ptr);
    } 
    // else if (device_ptr->mode == ap_station) {
    //   device_stop_ap_station(device_ptr);
    // }
    device_ptr->state = d_inactive;
  }
  device_ptr->mode = NAN;
}

void device_set_mode(DevicePtr device_ptr, Device_Mode mode) {
  if (device_ptr->mode == mode) {
    ESP_LOGI(LOGGING_TAG, "Device mode is already setted");
    return;
  }
  device_reset(device_ptr);
  device_ptr->mode = mode;
  if (mode == AP) {
    device_start_ap(device_ptr);
  } else if (mode == STATION) {
    device_start_station(device_ptr);
    device_connect_station(device_ptr);
  }
  // else if (mode == ap_station) {
  //   device_start_ap_station(device_ptr);
  // }
}

// AP

void device_start_ap(DevicePtr device_ptr) {
  ESP_LOGI(LOGGING_TAG, "Starting AP");
  if (ap_is_initialized(device_ptr->access_point_ptr)) {
    ap_start(device_ptr->access_point_ptr);
    device_ptr->state = d_active;
  } else {
    ESP_LOGE(LOGGING_TAG, "Access Point is not initialized");
  }
};

void device_stop_ap(DevicePtr device_ptr) {
  // if (device_ptr->state == d_active && device_ptr->mode == ap && ap_is_active(&device_ptr->access_point_ptr)) {
  // }
  ap_stop(device_ptr->access_point_ptr);
};

void device_restart_ap(DevicePtr device_ptr) {
  ap_restart(device_ptr->access_point_ptr);
};

// Station

void device_start_station(DevicePtr device_ptr) {
  if (station_is_initialized(device_ptr->station_ptr)) {
    station_start(device_ptr->station_ptr);
    device_ptr->state = d_active;
  } else {
    ESP_LOGE(LOGGING_TAG, "Station is not initialized");
  }
};

void device_connect_station(DevicePtr device_ptr) {
  station_find_ap(device_ptr->station_ptr);
  if (station_found_ap(device_ptr->station_ptr)) {
    ESP_LOGI(LOGGING_TAG, "WIFI FOUND! Initiating connection.");
    station_connect(device_ptr->station_ptr);
  } else {
    ESP_LOGE(LOGGING_TAG, "No wifi found, trying to reconnect in 10 seconds");
    sleep(10);
    // Retry to connect
    device_connect_station(device_ptr);
  }
};

void device_disconnect_station(DevicePtr device_ptr) {
  station_disconnect(device_ptr->station_ptr);
};

void device_restart_station(DevicePtr device_ptr) {
  station_restart(device_ptr->station_ptr);
};
