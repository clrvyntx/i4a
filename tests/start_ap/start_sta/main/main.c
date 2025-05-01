#include <stdio.h>
#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"

#include "device.h"

void app_main(void) {

    Device device;
    DevicePtr device_ptr = &device;

    const char *wifi_ssid_prefix = "red_a_conectar";
    const char *wifi_password = "pw_de_la_red";
    const char *device_uuid = "Test_STA";
    uint16_t device_orientation = 2;
    uint8_t ap_channel_to_emit = 6;
    uint8_t ap_max_sta_connections = 4;
    uint8_t device_is_root = 0;

    device_init(device_ptr, device_uuid, device_orientation, wifi_ssid_prefix, wifi_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, STATION);
    device_start_station(device_ptr);
    device_connect_station(device_ptr);

}
