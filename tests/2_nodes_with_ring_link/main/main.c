#include "esp_log.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "ring_share/ring_share.h"
#include "siblings/siblings.h"
#include "channel_manager/channel_manager.h"
#include "callbacks.h"
#include "node.h"

const char *TAG = "main";

static ring_share_t rs = { 0 };

static uint32_t r_subnet = 0x0A000000; // 10.0.0.0
static uint32_t r_mask   = 0xFF000000; // 255.0.0.0 (/8)

static uint32_t l_subnet = 0x0A200000; // 10.32.0.0
static uint32_t l_mask   = 0xFFE00000; // 255.224.0.0 (/11)

static uint32_t c_subnet = 0x0A600000; // 10.96.0.0
static uint32_t c_mask   = 0xFFFC0000; // 255.252.0.0 (/14)

static uint32_t o_subnet = 0x0B000000; // 11.0.0.0
static uint32_t o_mask = 0xFF000000; // 255.0.0.0

// Custom routing hook
struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t src_ip = lwip_ntohl(ip4_addr_get_u32(src));
    uint32_t dst_ip = lwip_ntohl(ip4_addr_get_u32(dest));
    uint8_t orientation = node_get_device_orientation();
    uint8_t is_root = node_is_device_center_root();

    // === Case 1: Center ===
    if (orientation == NODE_DEVICE_ORIENTATION_CENTER) {
        if (is_root) {
            if ((dst_ip & r_mask) == r_subnet) {
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
            } else {
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
            }
        } else {
            if ((dst_ip & c_mask) != c_subnet) {
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
            } else {
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
            }
        }
    }

    // === Case 2: North and Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_NORTH && is_root) {
        if (node_is_point_to_point_message(dst_ip)) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
        if ((dst_ip & c_mask) != c_subnet) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
    }

    // === Case 3: West and Not Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_WEST && !is_root) {
        if (node_is_point_to_point_message(dst_ip)) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
        if ((dst_ip & c_mask) == c_subnet) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
    }

    // === Default ===
    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}


void app_main(void) {
    node_setup();

    node_device_orientation_t orientation = node_get_device_orientation();
    bool device_is_root = node_is_device_center_root();

    siblings_t *sb = node_get_siblings_instance();
    rs_init(&rs, sb);
    cm_init(&rs, orientation);

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

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        uint8_t my_channel = cm_get_suggested_channel();
        ESP_LOGI(TAG, "Got suggested channel: %u", my_channel);
    }
}
