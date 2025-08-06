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

void generate_uuid_from_mac(char *uuid_out, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(uuid_out, len, "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

void generate_subnet_from_id(uint8_t device_id, char *cidr_out, char *gateway_out, size_t len, uint8_t is_root) {
    snprintf(cidr_out, len, "10.%d.0.1", device_id * 2 + is_root);
    snprintf(gateway_out, len, "10.%d.0.1", device_id * 2 + is_root);
}

void app_main(void) {
    config_setup();
    config_print();

    Device device;
    Device *device_ptr = &device;
    Device_Mode mode = AP;

    char device_uuid[7];
    generate_uuid_from_mac(device_uuid, sizeof(device_uuid));

    uint8_t device_orientation = config_get_orientation();
    uint8_t device_is_root = 0;

    char *wifi_network_prefix = "I4A";
    char *wifi_network_password = "test123456";

    uint8_t ap_channel_to_emit = (rand() % 11) + 1;
    uint8_t ap_max_sta_connections = 4;

    char network_cidr[16];
    char network_gateway[16];
    char *network_mask = "255.255.0.0";

    generate_subnet_from_id(device_orientation, network_cidr, network_gateway, sizeof(network_cidr), device_is_root);

    ESP_ERROR_CHECK(device_wifi_init());
    ESP_ERROR_CHECK(ring_link_init());

    device_init(device_ptr, device_uuid, device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
    device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(device_ptr);
}
