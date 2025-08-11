#include <stdio.h>
#include "esp_event.h"

#include "node.h"

#define IS_ROOT 1

static const char *TAG = "==> main";

static node_t *current_node;

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    ESP_LOGI(TAG, "Routing hook called for dest: %s", ip4addr_ntoa(dest));

    if (!current_node->node_device_ptr) {
        ESP_LOGW(TAG, "No device found, falling back to default");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    uint8_t orientation = current_node->node_device_ptr->device_orientation;
    uint8_t is_root = current_node->node_device_ptr->device_is_root;

    ESP_LOGI(TAG, "Device orientation: %d, is_root: %d", orientation, is_root);

    ip4_addr_t my_subnet;
    ip4_addr_t mask;
    IP4_ADDR(&mask, 255, 255, 0, 0); // /16

    // === Case 1: Center ===
    if (orientation == CONFIG_ORIENTATION_CENTER) {

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
            return (struct netif *)esp_netif_get_netif_impl(device_get_netif(current_node->node_device_ptr));
        } else {
            ESP_LOGI(TAG, "Dest is NOT in my subnet -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
        }
    }

    // === Case 2: East and Root ===
    if (orientation == CONFIG_ORIENTATION_EAST && is_root) {
        IP4_ADDR(&my_subnet, 10, 9, 0, 0);
        ESP_LOGI(TAG, "Device is east root, checking for center non-root subnet 10.9.0.0/16");

        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is this node -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
        } else {
            ESP_LOGI(TAG, "Dest is other node -> route via AP");
            return (struct netif *)esp_netif_get_netif_impl(device_get_netif(current_node->node_device_ptr));

        }
    }

    // === Case 3: West and Not Root ===
    if (orientation == CONFIG_ORIENTATION_WEST && !is_root) {
        IP4_ADDR(&my_subnet, 10, 8, 0, 0);
        ESP_LOGI(TAG, "Device is west non-root, checking for center non-root subnet 10.8.0.0/16");

        // Check if dest belongs to my subnet
        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is this node -> route via SPI");
	    return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());

        } else {
            ESP_LOGI(TAG, "Dest is other node -> route via STA");
	    return (struct netif *)esp_netif_get_netif_impl(device_get_netif(current_node->node_device_ptr));
        }
    }

    // === Default: fallback to ring link TX interface ===
    ESP_LOGW(TAG, "Falling back to SPI interface");
    return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
}

void app_main(void) {
    current_node = node_setup();
    uint8_t device_is_root = IS_ROOT;

    if(device_orientation == CONFIG_ORIENTATION_CENTER){
        if(device_is_root){
            uint32_t net = 0x0A090000; // 10.9.0.0
            uint32_t mask = 0xFFFF0000; // 255.255.0.0
            node_set_as_ap(net, mask);
        } else {
            uint32_t net = 0x0A080000; // 10.8.0.0
            uint32_t mask = 0xFFFF0000; // 255.255.0.0
            node_set_as_ap(net, mask);
        }

    }

    if(current_node->node_device_orientation == CONFIG_ORIENTATION_EAST && device_is_root){
        uint32_t net = 0x0A0A0000; // 10.10.0.0
        uint32_t mask = 0xFFFF0000; // 255.255.0.0
        node_set_as_ap(net, mask);
    }

    if(device_orientation == CONFIG_ORIENTATION_WEST && !device_is_root){
        node_set_as_sta();
    }
}
