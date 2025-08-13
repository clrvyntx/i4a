#include "esp_log.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "node.h"

#define IS_ROOT 1

static const char *TAG = "==> main";

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    ESP_LOGI(TAG, "Routing hook called for dest: %s", ip4addr_ntoa(dest));

    uint8_t orientation = node_get_device_orientation();
    uint8_t is_root = IS_ROOT;

    ESP_LOGI(TAG, "Device orientation: %d, is_root: %d", orientation, is_root);

    ip4_addr_t my_subnet;
    ip4_addr_t mask;
    IP4_ADDR(&mask, 255, 255, 0, 0); // /16

    // === Case 1: Center ===
    if (orientation == NODE_DEVICE_ORIENTATION_CENTER) {

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
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        } else {
            ESP_LOGI(TAG, "Dest is NOT in my subnet -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        }
    }

    // === Case 2: East and Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_EAST && is_root) {
        IP4_ADDR(&my_subnet, 10, 9, 0, 0);
        ESP_LOGI(TAG, "Device is east root, checking for center non-root subnet 10.9.0.0/16");

        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is this node -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            ESP_LOGI(TAG, "Dest is other node -> route via AP");
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());

        }
    }

    // === Case 3: West and Not Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_WEST && !is_root) {
        IP4_ADDR(&my_subnet, 10, 8, 0, 0);
        ESP_LOGI(TAG, "Device is west non-root, checking for center non-root subnet 10.8.0.0/16");

        // Check if dest belongs to my subnet
        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is this node -> route via SPI");
	    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());

        } else {
            ESP_LOGI(TAG, "Dest is other node -> route via STA");
	    return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
    }

    // === Default: fallback to ring link TX interface ===
    ESP_LOGW(TAG, "Falling back to SPI interface");
    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}

void app_main(void) {
    node_setup();

    uint8_t orientation = node_get_device_orientation();
    uint8_t device_is_root = IS_ROOT;

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER){
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

    if(orientation == NODE_DEVICE_ORIENTATION_EAST && device_is_root){
        uint32_t net = 0x0A0A0000; // 10.10.0.0
        uint32_t mask = 0xFFFF0000; // 255.255.0.0
        node_set_as_ap(net, mask);
    }

    if(orientation == NODE_DEVICE_ORIENTATION_WEST && !device_is_root){
        node_set_as_sta();
    }
}
