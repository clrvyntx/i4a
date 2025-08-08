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

bool get_first_sta_mac(uint8_t mac[6]) {
    wifi_sta_list_t sta_list = {0};
    esp_err_t err = esp_wifi_ap_get_sta_list(&sta_list);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get STA list: %d", err);
        return false;
    }

    if (sta_list.num == 0) {
        ESP_LOGW(TAG, "No stations connected");
        return false;
    }

    // Get MAC of the first station
    memcpy(mac, sta_list.sta[0].mac, 6);
    ESP_LOGI(TAG, "First connected STA MAC: " MACSTR, MAC2STR(mac));
    return true;
}

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

            uint8_t mac[6];
            if (get_first_sta_mac(mac)) {
                // Insert static ARP entry for dest IP -> MAC
                struct eth_addr eth_mac;
                memcpy(eth_mac.addr, mac, ETH_HWADDR_LEN);

                err_t err = etharp_add_static_entry(dest, &eth_mac);
                if (err == ERR_OK) {
                    ESP_LOGI(TAG, "Added static ARP entry: %s -> " MACSTR,
                             ip4addr_ntoa(dest), MAC2STR(mac));
                } else {
                    ESP_LOGW(TAG, "Failed to add static ARP entry: %d", err);
                }
            } else {
                ESP_LOGW(TAG, "Cannot add ARP entry, no STA MAC found");
            }

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
    ESP_LOGW(TAG, "Falling back to default routing interface");
    return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
}

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

    ESP_ERROR_CHECK(device_wifi_init());
    ESP_ERROR_CHECK(ring_link_init());

    char device_uuid[7];
    generate_uuid_from_mac(device_uuid, sizeof(device_uuid));

    uint8_t device_orientation = config_get_orientation();
    uint8_t device_is_root = IS_ROOT;

    char *wifi_network_prefix = "I4A";
    char *wifi_network_password = "test123456";

    uint8_t ap_channel_to_emit = (rand() % 11) + 1;
    uint8_t ap_max_sta_connections = 4;

    char network_cidr[16];
    char network_gateway[16];
    char *network_mask = "255.255.0.0";

    if(device_orientation == 4){ // Center
        Device_Mode mode = AP;
        generate_subnet_from_id(device_orientation, network_cidr, network_gateway, sizeof(network_cidr), device_is_root);

        device_init(device_ptr, device_uuid, device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
        device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
        device_start_ap(device_ptr);
    }

    if(device_orientation == 2 && device_is_root){ // West and root
        Device_Mode mode = AP;
        generate_subnet_from_id(device_orientation, network_cidr, network_gateway, sizeof(network_cidr), device_is_root);

        device_init(device_ptr, device_uuid, device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
        device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
        device_start_ap(device_ptr);
    }

    if(device_orientation == 3 && !device_is_root){ // East and non-root
        Device_Mode mode = STATION;

        device_init(device_ptr, device_uuid, device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
        device_start_station(device_ptr);
        device_connect_station(device_ptr);
    }
}
