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

static const char *LOGGING_TAG = "device";
static const char *dev_orientation[5] = {"_N_", "_S_", "_E_", "_W_", "_C_"};
static bool is_on_connect_loop = false;
static DevicePtr current_device_ptr = NULL;

// Function to initialize NVS (non-volatile storage)
static esp_err_t init_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  return ret;
}

// Function to initialize the Wi-Fi interface
esp_err_t wifi_init() {
  // Initialize NVS
  esp_err_t ret = init_nvs();
  if (ret != ESP_OK) {
    ESP_LOGE(LOGGING_TAG, "NVS initialization failed");
    return ret;
  }

  // Initialize the ESP-NETIF library (for network interface management)
  ESP_ERROR_CHECK(esp_netif_init());

  // Create default event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize Wi-Fi configuration structure
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  return ESP_OK;
}

void device_init(DevicePtr device_ptr, const char *device_uuid, uint8_t device_orientation, const char *wifi_network_prefix, const char *wifi_network_password, uint8_t ap_channel_to_emit, uint8_t ap_max_sta_connections, uint8_t device_is_root, Device_Mode mode) {
  device_ptr->mode = mode;
  device_ptr->state = d_inactive;
  device_ptr->device_is_root = device_is_root;

  AccessPoint ap = {};
  device_ptr->access_point = ap;
  device_ptr->access_point_ptr = &device_ptr->access_point;

  Station station = {};
  device_ptr->station = station;
  device_ptr->station_ptr = &device_ptr->station;

  current_device_ptr = device_ptr;

  if (mode == AP) {
    device_init_ap(device_ptr, ap_channel_to_emit, wifi_network_prefix, device_uuid, wifi_network_password, ap_max_sta_connections, device_orientation);
  } else if (mode == STATION) {
    device_init_station(device_ptr, wifi_network_prefix, device_orientation, device_uuid, wifi_network_password);
  }

}

void device_init_ap(DevicePtr device_ptr, uint8_t channel, const char *wifi_network_prefix ,const char *device_uuid, const char *password, uint8_t max_sta_connections, uint16_t orientation) {
  // Generate the wifi ssid
  char wifi_ssid[32];
  memset(wifi_ssid, 0, sizeof(wifi_ssid));
  strcpy(wifi_ssid, wifi_network_prefix);
  strcat(wifi_ssid, dev_orientation[orientation]);
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
      device_stop_station(device_ptr);
    } 
    // else if (device_ptr->mode == ap_station) {
    //   device_stop_ap_station(device_ptr);
    // }
    device_ptr->state = d_inactive;
  }
  device_destroy_netif(device_ptr);
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

void device_destroy_netif(DevicePtr device_ptr){
  if (device_ptr->mode == AP) {
    ap_destroy_netif(device_ptr->access_point_ptr);
  } else if (device_ptr->mode == STATION) {
    station_destroy_netif(device_ptr->station_ptr);
  }
}

// AP

void device_start_ap(DevicePtr device_ptr) {
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

static void device_connect_station_task(void* arg) {
  DevicePtr device_ptr = (DevicePtr)arg;  // Get the device pointer from the task argument

  while (is_on_connect_loop) {

    // If station is disconnected, start scanning for APs
    if (!station_is_active(device_ptr->station_ptr)) {
      ESP_LOGI(LOGGING_TAG, "Wi-Fi not connected. Scanning for available networks...");
      station_find_ap(device_ptr->station_ptr);

      if (station_found_ap(device_ptr->station_ptr)) {
        ESP_LOGI(LOGGING_TAG, "Wi-Fi found! Attempting to connect.");
        station_connect(device_ptr->station_ptr);
      } else {
        ESP_LOGE(LOGGING_TAG, "No Wi-Fi found. Re-scanning in 10 seconds.");
      }

      // Wait before trying again
      vTaskDelay(pdMS_TO_TICKS(10000));  // Scan every 10 seconds
    }

    vTaskDelay(pdMS_TO_TICKS(10000)); // Wait 10 seconds before checking if station is not currently trying to connect
  }

  ESP_LOGW(LOGGING_TAG, "Station not on connect loop, killing the task.");
  vTaskDelete(NULL);  // Delete the task

}

void device_connect_station(DevicePtr device_ptr) {
  is_on_connect_loop = true;
  xTaskCreate(device_connect_station_task, "device_connect_station_task", 4096, device_ptr, 5, NULL);
}

void device_disconnect_station(DevicePtr device_ptr) {
  is_on_connect_loop = false;
  station_disconnect(device_ptr->station_ptr);
};

void device_restart_station(DevicePtr device_ptr) {
  station_restart(device_ptr->station_ptr);
};

void device_stop_station(DevicePtr device_ptr) {
  station_stop(device_ptr->station_ptr);
};

DevicePtr get_current_device() {
  return current_device_ptr;
}

esp_netif_t *device_get_netif(DevicePtr device_ptr) {
  if (!device_ptr) {
    return NULL;
  }

  if (device_ptr->mode == AP) {
    return device_ptr->access_point_ptr->netif;
  } else if (device_ptr->mode == STATION) {
    return device_ptr->station_ptr->netif;
  }

  return NULL;
}
