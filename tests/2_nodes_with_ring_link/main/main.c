#include "esp_log.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "node.h"

static const char *TAG = "==> main";

static uint32_t r_subnet = 0x0A000000; // 10.0.0.0
static uint32_t r_mask   = 0xFF000000; // 255.0.0.0

static uint32_t e_subnet = 0x0A010000; // 10.1.0.0
static uint32_t e_mask = 0xFFFF0000; // 255.255.0.0

static uint32_t c_subnet = 0x0A010100; // 10.1.1.0
static uint32_t c_mask = 0xFFFFFF00; // 255.255.255.0

static uint32_t o_subnet = 0x0B000000; // 11.0.0.0
static uint32_t o_mask = 0xFF000000; // 255.0.0.0

// Make sure to comment or remove all the logs when doing a real speed test, they're slow and crash the watchdog timers
struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t dst_ip = lwip_ntohl(ip4_addr_get_u32(dest));
    uint8_t orientation = node_get_device_orientation();
    uint8_t is_root = node_is_device_center_root();

    ESP_LOGI(TAG, "Routing hook called: src_ip=%s, dst_ip=%s, orientation=%d, is_root=%d", ip4addr_ntoa(src), ip4addr_ntoa(dest), orientation, is_root);

    // === Case 1: Center ===
    if (orientation == NODE_DEVICE_ORIENTATION_CENTER) {
        if (is_root) {
            if ((dst_ip & r_mask) == r_subnet) {
                ESP_LOGI(TAG, "Center Root: dst in 10.0.0.0/8 -> Use SPI");
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
            } else {
                ESP_LOGI(TAG, "Center Root: dst outside 10.0.0.0/8 -> Use Wi-Fi");
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
            }
        } else {
            if ((dst_ip & c_mask) != c_subnet) {
                ESP_LOGI(TAG, "Center Leaf: dst outside 10.1.1.0/24 -> Use SPI");
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
            } else {
                ESP_LOGI(TAG, "Center Leaf: dst in 10.1.1.0/24 -> Use Wi-Fi");
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
            }
        }
    }

    // === Case 2: East and Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_EAST && is_root) {
        if (dst_ip == (e_subnet + 1) || dst_ip == (e_subnet + 2)) {
            ESP_LOGI(TAG, "East Root: point-to-point -> Use Wi-Fi");
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
        if ((dst_ip & e_mask) != e_subnet) {
            ESP_LOGI(TAG, "East Root: dst outside 10.1.0.0/16 -> Use SPI");
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            ESP_LOGI(TAG, "East Root: dst in 10.1.0.0/16 -> Use Wi-Fi");
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
    }

    // === Case 3: West and Not Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_WEST && !is_root) {
        if (dst_ip == (e_subnet + 1) || dst_ip == (e_subnet + 2)) {
            ESP_LOGI(TAG, "West Leaf: point-to-point -> Use Wi-Fi");
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
        if ((dst_ip & e_mask) == e_subnet) {
            ESP_LOGI(TAG, "West Leaf: dst in 10.1.0.0/16 -> Use SPI");
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            ESP_LOGI(TAG, "West Leaf: dst outside 10.1.0.0/16 -> Use Wi-Fi");
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
    }

    // === Default ===
    ESP_LOGI(TAG, "Default case: fallback to SPI");
    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}


void app_main(void) {
    node_setup();

    uint8_t orientation = node_get_device_orientation();
    uint8_t device_is_root = node_is_device_center_root();

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER){
        if(device_is_root){
            node_set_as_ap(o_subnet, o_mask); // Uncomment this to test with an AP that simulates an outside connection
            //node_set_as_sta(); // Uncomment this to test with real outside connection
        } else {
            node_set_as_ap(c_subnet, c_mask);
        }
    }

    if(orientation == NODE_DEVICE_ORIENTATION_EAST && device_is_root){
        node_set_as_ap(e_subnet, e_mask);
    }

    if(orientation == NODE_DEVICE_ORIENTATION_WEST && !device_is_root){
        node_set_as_sta();
    }
}
