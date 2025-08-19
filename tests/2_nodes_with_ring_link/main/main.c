#include "esp_log.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "node.h"

#define NETWORK_NAME "test"
#define NETWORK_PASSWORD "123456"

static const char *TAG = "==> main";

static uint32_t r_subnet = 0x0A000000; // 10.0.0.0
static uint32_t r_mask   = 0xFF000000; // 255.0.0.0

static uint32_t e_subnet = 0x0A010000; // 10.1.0.0
static uint32_t e_mask = 0xFFFF0000; // 255.255.0.0

static uint32_t c_subnet = 0x0A010100; // 10.1.1.0
static uint32_t c_mask = 0xFFFFFF00; // 255.255.255.0

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t dst_ip = ip4_addr_get_u32(dest);
    uint8_t orientation = node_get_device_orientation();
    uint8_t is_root = node_is_device_center_root();

    // === Case 1: Center ===
    if (orientation == NODE_DEVICE_ORIENTATION_CENTER) {
        if (is_root) {
            if ((dst_ip & r_mask) == r_subnet) {
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif()); // Destination is in 10.0.0.0/8, use SPI
            } else {
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif()); // Destination is NOT in 10.0.0.0/8, use Wi-Fi
            }
        } else {
            if ((dst_ip & c_mask) != c_subnet) {
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif()); // Destination is NOT in 10.1.1.0/24, use SPI
            } else {
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif()); // Destination is in 10.1.1.0/24, use Wi-Fi
            }
        }

    }

    // === Case 2: East and Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_EAST && is_root) {
        if ((dst_ip & e_mask) != e_subnet) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif()); // Destination is NOT in 10.1.0.0/16, use SPI
        } else {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif()); // Destination is in 10.1.0.0/16, use Wi-Fi
        }
    }

    // === Case 3: West and Not Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_WEST && !is_root) {
        if ((dst_ip & e_mask) == e_subnet) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif()); // Destination is in 10.1.0.0/16, use SPI
        } else {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif()); // Destination is NOT in 10.1.0.0/16, use Wi-Fi
        }
    }

    // === Default: fallback to ring link TX interface ===
    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}

void app_main(void) {
    node_setup();

    uint8_t orientation = node_get_device_orientation();
    uint8_t device_is_root = node_is_device_center_root();

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER){
        if(device_is_root){
            node_set_as_root_sta(NETWORK_NAME, NETWORK_PASSWORD);
        } else {
            node_set_as_ap(c_subnet, c_mask);
        }

    }

    if(orientation == NODE_DEVICE_ORIENTATION_EAST && device_is_root){
        node_set_as_ap(e_subnet, e_mask);
    }

    if(orientation == NODE_DEVICE_ORIENTATION_WEST && !device_is_root){
        node_set_as_peer_sta();
    }
}
