#include <stdio.h>
#include "esp_event.h"
#include "esp_check.h"

#include "config.h"
#include "utils.h"
#include "wifi.h"
#include "ring_link.h"
#include "device.h"
#include "server.h"

#define IS_ROOT 1

static const char *TAG = "==> main";

Device device;
Device *device_ptr = &device;

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    ESP_LOGI(TAG, "Routing hook called for dest: %s", ip4addr_ntoa(dest));

    if (!device_ptr) {
        ESP_LOGW(TAG, "No device found, falling back to default");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    uint8_t orientation = device_ptr->device_orientation;
    uint8_t is_root = device_ptr->device_is_root;

    ESP_LOGI(TAG, "Device orientation: %d, is_root: %d", orientation, is_root);

    ip4_addr_t my_subnet;
    ip4_addr_t mask;
    IP4_ADDR(&mask, 255, 255, 0, 0); // /16

    // === Case 1: Center (device_orientation == 4) ===
    if (orientation == 4) {

        if (is_root) {
            IP4_ADDR(&my_subnet, 10, 9, 0, 0);
            ESP_LOGI(TAG, "Device is center root, subnet 10.9.0.0/16");
        } else {
            IP4_ADDR(&my_subnet, 10, 8, 0, 0);
            ESP_LOGI(TAG, "Device is center non-root, subnet 10.8.0.0/16");
        }

        // Check if dest belongs to my subnet
        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is in my subnet -> route via AP");
            return (struct netif *)esp_netif_get_netif_impl(device_get_netif(device_ptr));
        } else {
            ESP_LOGI(TAG, "Dest is NOT in my subnet -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
        }
    }

    // === Case 2: West and Root (device_orientation == 2 && is_root) ===
    if (orientation == 2 && is_root) {
        IP4_ADDR(&my_subnet, 10, 8, 0, 0);
        ESP_LOGI(TAG, "Device is west root, checking for center non-root subnet 10.8.0.0/16");

        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is other node -> route via AP");
            return (struct netif *)esp_netif_get_netif_impl(device_get_netif(device_ptr));
        } else {
            ESP_LOGI(TAG, "Dest is this node -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
        }
    }

    // === Case 3: East and Not Root (device_orientation == 3 && !is_root) ===
    if (orientation == 3 && !is_root) {
        IP4_ADDR(&my_subnet, 10, 9, 0, 0);
        ESP_LOGI(TAG, "Device is east non-root, checking for center root subnet 10.9.0.0/16");

        // Check if dest belongs to my subnet
        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is other node -> route via STA");
            return (struct netif *)esp_netif_get_netif_impl(device_get_netif(device_ptr));
        } else {
            ESP_LOGI(TAG, "Dest is this node -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
        }
    }

    // === Default: fallback to ring link TX interface ===
    ESP_LOGW(TAG, "Falling back to SPI interface");
    return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
}

void generate_uuid_from_mac(char *uuid_out, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(uuid_out, len, "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

void app_main(void) {
    config_setup();
    config_print();

    ESP_ERROR_CHECK(device_wifi_init());
    ESP_ERROR_CHECK(ring_link_init());

    char device_uuid[7];
    generate_uuid_from_mac(device_uuid, sizeof(device_uuid));

    uint8_t device_orientation = 2;
    uint8_t device_is_root = IS_ROOT;

    char *wifi_network_prefix = "I4A";
    char *wifi_network_password = "test123456";

    uint8_t ap_channel_to_emit = (rand() % 11) + 1;
    uint8_t ap_max_sta_connections = 4;

    char *network_cidr;
    char *network_gateway;
    char *network_mask = "255.255.0.0";

    if(device_orientation == CONFIG_ORIENTATION_CENTER){
        Device_Mode mode = AP;

        if(device_is_root){
            network_cidr = "10.9.0.1";
            network_gateway = "10.9.0.1";
        } else {
            network_cidr = "10.8.0.1";
            network_gateway = "10.8.0.1";

        }

        device_init(device_ptr, device_uuid, device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
        device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
        device_start_ap(device_ptr);
    }

    if(device_orientation == CONFIG_ORIENTATION_EAST && device_is_root){
        Device_Mode mode = AP;

        network_cidr = "10.10.0.1";
        network_gateway = "10.10.0.2"; // Fake gateway for rerouting to STA

        device_init(device_ptr, device_uuid, device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
        device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
        device_start_ap(device_ptr);
    }

    if(device_orientation == CONFIG_ORIENTATION_WEST && !device_is_root){
        Device_Mode mode = STATION;

        device_init(device_ptr, device_uuid, device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
        device_start_station(device_ptr);
        device_connect_station(device_ptr);
    }
}
