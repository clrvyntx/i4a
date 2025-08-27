#include "esp_log.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "node.h"

static const char *TAG = "==> main";

static uint32_t r_subnet = 0x0A000000; // 10.0.0.0
static uint32_t r_mask   = 0xFF000000; // 255.0.0.0 (/8)

static uint32_t l_subnet = 0x0A200000; // 10.32.0.0
static uint32_t l_mask   = 0xFFE00000; // 255.224.0.0 (/11)

static uint32_t c_subnet = 0x0A600000; // 10.96.0.0
static uint32_t c_mask   = 0xFFFC0000; // 255.252.0.0 (/14)

static uint32_t o_subnet = 0x0B000000; // 11.0.0.0
static uint32_t o_mask = 0xFF000000; // 255.0.0.0

// Make sure to comment or remove all the logs when doing a real speed test, they're slow and crash the watchdog timers
struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t src_ip = lwip_ntohl(ip4_addr_get_u32(src));
    uint32_t dst_ip = lwip_ntohl(ip4_addr_get_u32(dest));
    uint8_t orientation = node_get_device_orientation();
    uint8_t is_root = node_is_device_center_root();

    char src_ip_str[16], dst_ip_str[16];
    strncpy(src_ip_str, ip4addr_ntoa(src), sizeof(src_ip_str));
    src_ip_str[sizeof(src_ip_str) - 1] = '\0';

    strncpy(dst_ip_str, ip4addr_ntoa(dest), sizeof(dst_ip_str));
    dst_ip_str[sizeof(dst_ip_str) - 1] = '\0';

    ESP_LOGI(TAG, "Routing hook called: src_ip_str=%s (%" PRIu32 "), dst_ip_str=%s (%" PRIu32 "), orientation=%d, is_root=%d",
             src_ip_str, src_ip, dst_ip_str, dst_ip, node_get_device_orientation(), node_is_device_center_root());

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
                ESP_LOGI(TAG, "Center Leaf: dst outside 10.96.0.0/14 -> Use SPI");
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
            } else {
                ESP_LOGI(TAG, "Center Leaf: dst in 10.96.0.0/14 -> Use Wi-Fi");
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
            }
        }
    }

    // === Case 2: North and Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_NORTH && is_root) {
        if (node_is_point_to_point_message(dst_ip)) {
            ESP_LOGI(TAG, "North Root: point-to-point -> Use Wi-Fi");
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
        if ((dst_ip & c_mask) != c_subnet) {
            ESP_LOGI(TAG, "North Root: dst outside 10.96.0.0/14 -> Use SPI");
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            ESP_LOGI(TAG, "North Root: dst in 10.96.0.0/14 -> Use Wi-Fi");
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
    }

    // === Case 3: West and Not Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_WEST && !is_root) {
        if (node_is_point_to_point_message(dst_ip)) {
            ESP_LOGI(TAG, "West Leaf: point-to-point -> Use Wi-Fi");
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
        if ((dst_ip & c_mask) == c_subnet) {
            ESP_LOGI(TAG, "West Leaf: dst in 10.96.0.0/14 -> Use SPI");
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            ESP_LOGI(TAG, "West Leaf: dst outside 10.96.0.0/14 -> Use Wi-Fi");
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
            node_set_as_ap(o_subnet, o_mask);
        } else {
            node_set_as_ap(c_subnet, c_mask);
        }
    }

    if(orientation == NODE_DEVICE_ORIENTATION_NORTH && device_is_root){
        node_set_as_ap(l_subnet, l_mask);
    }

    if(orientation == NODE_DEVICE_ORIENTATION_WEST && !device_is_root){
        node_set_as_sta();
    }
}
