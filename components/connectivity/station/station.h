#include "esp_wifi.h"
#include "esp_mac.h"

struct wifi_ap_record_t_owned {
  wifi_ap_record_t ap_info;
  bool found;
};

typedef struct wifi_ap_record_t_owned wifi_ap_record_t_owned;

typedef enum {
  s_active,
  s_inactive
} Station_State;

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
  wifi_config_t wifi_config;
  wifi_ap_record_t wifi_ap_found;
};

typedef struct Station Station;
typedef struct Station* StationPtr;

void station_init(StationPtr stationPtr, const char* wifi_ssid_like, uint16_t orientation, char* device_uuid, const char* password);
void station_start(StationPtr stationPtr);
void station_connect(StationPtr stationPtr);
void station_disconnect(StationPtr stationPtr);
void station_restart(StationPtr stationPtr);
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
* @brief Event handler for WiFi events for station mode
* @param arg The argument passed during handler registration
* @param event_base The event base
* @param event_id The event id
* @param event_data The event data
*/
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data);

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

