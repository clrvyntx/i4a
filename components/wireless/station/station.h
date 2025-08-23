#ifndef _STA_H_
#define _STA_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

struct wifi_ap_record_t_owned {
  wifi_ap_record_t ap_info;
  bool found;
};

typedef struct wifi_ap_record_t_owned wifi_ap_record_t_owned;

typedef enum {
  s_active,
  s_inactive
} Station_State;

typedef enum station_type {
  PEER = 0,
  ROOT = 1
} station_type_t;

struct Station {
  // Members
  char device_uuid[32];
  uint8_t device_orientation;
  bool device_is_root;
  char ssid_like[32];
  char password[64];
  bool initialized;
  bool active;
  bool ap_found;
  Station_State state;
  station_type_t station_type;
  wifi_config_t wifi_config;
  wifi_ap_record_t wifi_ap_found;
  esp_netif_t *netif;
  uint32_t subnet;
  uint32_t mask;
};

typedef struct Station Station;
typedef struct Station* StationPtr;

void station_init(StationPtr stationPtr, const char* wifi_ssid_like, uint16_t orientation, char* device_uuid, const char* password, station_type_t station_type);
void station_start(StationPtr stationPtr);
void station_connect(StationPtr stationPtr);
void station_disconnect(StationPtr stationPtr);
void station_restart(StationPtr stationPtr);
void station_stop(StationPtr stationPtr);
void station_destroy_netif(StationPtr stationPtr);
bool station_is_initialized(StationPtr stationPtr);
bool station_is_active(StationPtr stationPtr);
void station_find_ap(StationPtr stationPtr);
bool station_found_ap(StationPtr station_ptr);
void transform_wifi_ap_record_to_config(StationPtr stationPtr);

/*
 * @brief Discover the AP with the name like 'ESP_' and return the AP
 * information to connect
 * @param wifi_ssid_like The name of the AP to discover
 * @param orientation is where the node is looking at
 * @return uint_8_t The SSID of the AP to connect
 */
struct wifi_ap_record_t_owned discover_wifi_ap(const char* wifi_ssid_like, uint16_t orientation, char* device_uuid);

/*
 * @brief Initialize the WiFi stack in station mode
 */
void init_station_mode();

/*
 * @brief Connect to the AP with the given SSID and password
 * @param ssid The SSID of the AP to connect
 * @param password The password of the AP to connect
 */
void connect_to_wifi(wifi_config_t wifi_config);

/*
 * @brief Wait until the connection is established
 */
void wait_connection_established();

#ifdef __cplusplus
}
#endif

#endif // _STA_H_
