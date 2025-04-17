#include "esp_wifi.h"

#include "../access_point/access_point.h"
#include "../station/station.h"
#include "../token_ring/token_ring.h"

/*
* State enum to manage the state of the Access Point
*/
typedef enum {
  AP = 0,  // Access Point is active
  STATION, // Access Point is inactive
  AP_STATION,
  NAN
} Device_Mode;

typedef enum {
  d_active,  // Access Point is active
  d_inactive // Access Point is inactive
} Device_State;


/*
* Struct to manage the Access Point
*/
struct Device {
  // Members
  char device_uuid[32];
  uint8_t device_orientation;
  uint8_t device_is_root;
  char ssid[32];
  char password[64];
  uint8_t channel;
  Device_Mode mode;
  Device_State state;
  AccessPoint access_point;
  AccessPointPtr access_point_ptr;
  Station station;
  StationPtr station_ptr;
};

typedef struct Device Device;
typedef struct Device* DevicePtr;

// Methods
void device_init(DevicePtr device_ptr, const char *device_uuid, uint8_t device_orientation, const char *wifi_network_prefix, const char *wifi_network_password, uint8_t ap_channel_to_emit, uint8_t softap_max_sta_connections, uint8_t device_is_root, Device_Mode mode);
void device_init_ap(DevicePtr device_ptr, uint8_t channel, const char *wifi_network_prefix ,const char *device_uuid, const char *password, uint8_t max_sta_connections);
void device_init_station(DevicePtr device_ptr, const char* wifi_ssid_like, uint16_t orientation, char* device_uuid, const char* password);
void device_set_mode(DevicePtr device_ptr, Device_Mode mode);
void device_reset(DevicePtr device_ptr);
void device_start_ap(DevicePtr device_ptr);
void device_stop_ap(DevicePtr device_ptr);
void device_restart_ap(DevicePtr device_ptr);
void device_start_station(DevicePtr device_ptr);
void device_connect_station(DevicePtr device_ptr);
void device_disconnect_station(DevicePtr device_ptr);
void device_restart_station(DevicePtr device_ptr);
void device_set_network_ap(DevicePtr device_ptr, const char *network_cidr, const char *network_gateway, const char *network_mask);
// void device_start_ap_station(DevicePtr device_ptr);
// void device_stop_ap_station(DevicePtr device_ptr);
// void device_restart_ap_station(DevicePtr device_ptr);
// void device_start_tcp_server(DevicePtr device_ptr);
// void device_stop_tcp_server(DevicePtr device_ptr);
// void device_restart_tcp_server(DevicePtr device_ptr);
// void device_handle_message(DevicePtr device_ptr);

// Quiza podemos agregar un flag para saber a quien va el mensaje
// Aca tendriamos el handle message y llamamos al station station_find_ap
// lo mejor seria que ellos chequeen si es para nosotros 