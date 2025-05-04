#include <stdio.h>
#include "esp_event.h"
#include "esp_check.h"

#include "config.h"
#include "utils.h"
#include "wifi.h"
#include "ring_link.h"
#include "device.h"
#include "server.h"

static const char *TAG = "==> main";

void app_main(void) {

    config_setup();
    config_print();

    Device device;
    Device *device_ptr = &device;

    const char *device_uuid = "ESP_01";
    const uint8_t device_orientation = 0;

    const char *wifi_network_prefix = "AP_Test";
    const char *wifi_network_password = "test123456";

    const uint8_t ap_channel_to_emit = 6;
    const uint8_t ap_max_sta_connections = 4;

    const char *network_cidr = "10.0.0.1";
    const char *network_gateway = "10.0.0.1";
    const char *network_mask = "255.0.0.0";

    const uint8_t device_is_root = 1;
    Device_Mode mode = AP;

    device_init(device_ptr, device_uuid, device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);

    ESP_ERROR_CHECK(ring_link_init());

    device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(device_ptr);

    print_route_table();
    open_server();

}
