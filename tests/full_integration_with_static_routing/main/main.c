#include "esp_log.h"
#include <inttypes.h>
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "wireless/wireless.h"
#include "siblings/siblings.h"
#include "ring_share/ring_share.h"
#include "shared_state/shared_state.h"
#include "sync/sync.h"
#include "routing_config/routing_config.h"
#include "routing/routing.h"
#include "channel_manager/channel_manager.h"
#include "callbacks.h"
#include "node.h"

#define ROOT_NETWORK 0x0A000000  // 10.0.0.0
#define ROOT_MASK 0xFF000000 // 255.0.0.0

static const char *TAG = "routing_hook";

static ring_share_t rs = { 0 };
static sync_t _sync = { 0 };
static shared_state_t ss = { 0 };
static routing_t rt = { 0 };

static uint32_t r_subnet = 0x0A000000; // 10.0.0.0
static uint32_t r_mask   = 0xFF000000; // 255.0.0.0 (/8)

static uint32_t l_subnet = 0x0A200000; // 10.32.0.0
static uint32_t l_mask   = 0xFFE00000; // 255.224.0.0 (/11)

static uint32_t c_subnet = 0x0A600000; // 10.96.0.0
static uint32_t c_mask   = 0xFFFC0000; // 255.252.0.0 (/14)

static node_device_orientation_t orientation;
static bool is_center_root;

// Custom routing hook
struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t src_ip = lwip_ntohl(ip4_addr_get_u32(src));
    uint32_t dst_ip = lwip_ntohl(ip4_addr_get_u32(dest));

    // === Case 1: Center ===
    if (orientation == NODE_DEVICE_ORIENTATION_CENTER) {
        if (is_center_root) {
            if ((dst_ip & r_mask) == r_subnet) {
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
            } else {
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
            }
        } else {
            if ((dst_ip & r_mask) != r_subnet) {
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
            } else {
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
            }
        }
    }

    // === Case 2: North and Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_NORTH && is_center_root) {
        if (node_is_point_to_point_message(dst_ip)) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
        if ((dst_ip & r_mask) != r_subnet) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
    }

    // === Case 3: North and Not Root ===
    if (orientation == NODE_DEVICE_ORIENTATION_NORTH && !is_center_root) {
        if (node_is_point_to_point_message(dst_ip)) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
        if ((dst_ip & r_mask) == r_subnet) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
    }

    // === Default ===
    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}

void routing_task(void *pvParameters) {
    routing_t *rt = (routing_t *)pvParameters;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        rt_on_tick(rt, 1000);
    }
}

void app_main(void) {
    node_setup();

    siblings_t *sb = node_get_siblings_instance();
    wireless_t *wl = node_get_wireless_instance();
    orientation = node_get_device_orientation();
    is_center_root = node_is_device_center_root();

    rs_init(&rs, sb);
    sync_init(&_sync, &rs, orientation);
    ss_init(&ss, &_sync, &rs, orientation);
    cm_init(&rs, orientation);
    rt_create(&rt, &rs, wl, &_sync, &ss, orientation);

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER){
        if(is_center_root){
            rt_init_root(&rt, r_subnet, r_mask);
        } else {
            rt_init_home(&rt);
        }
    } else {
        rt_init_forwarder(&rt);
    }
    
    rt_on_start(&rt);
    rt_on_tick(&rt, 1);

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER && is_center_root){
        node_set_as_ap(r_subnet, r_mask);
    }

    if(orientation == NODE_DEVICE_ORIENTATION_NORTH && !is_center_root){ // For testing, have a single one, in reality all non centers should be stations
        node_set_as_sta();
    }
    
    xTaskCreatePinnedToCore(
        routing_task,
        "routing_task",
        4096,
        &rt,
        tskIDLE_PRIORITY + 2,
        NULL,
        0
    );

}
