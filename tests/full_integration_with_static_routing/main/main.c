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
#include "internal_messages.h"
#include "callbacks.h"
#include "node.h"

#define ROOT_NETWORK 0x0A000000  // 10.0.0.0
#define ROOT_MASK    0xFF000000  // 255.0.0.0 (/8)

static const char *TAG = "routing_hook";

static ring_share_t rs = { 0 };
static sync_t _sync = { 0 };
static shared_state_t ss = { 0 };
static routing_t rt = { 0 };

static uint32_t r_subnet = 0x0A000000; // 10.0.0.0
static uint32_t r_mask   = 0xFF000000; // /8

static node_device_orientation_t orientation;
static bool is_center_root;

// ---------------- Default routing hook ----------------
struct netif *routing_hook_default(uint32_t src_ip, uint32_t dst_ip) {
    return NULL;
}

// ---------------- Static routing hook ----------------
struct netif *routing_hook_static(uint32_t src_ip, uint32_t dst_ip) {
    // Center nodes
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

    // North nodes
    if (orientation == NODE_DEVICE_ORIENTATION_NORTH) {
        if (node_is_point_to_point_message(dst_ip)) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
        if (is_center_root) {
            if ((dst_ip & r_mask) != r_subnet) {
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
            } else {
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
            }
        } else {
            if ((dst_ip & r_mask) == r_subnet) {
                return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
            } else {
                return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
            }
        }
    }

    // Fallback
    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}

// ---------------- Selected hook ----------------
static struct netif *(*selected_routing_hook)(uint32_t src_ip, uint32_t dst_ip) = routing_hook_default;

// ---------------- IP4 hook called by LWIP ----------------
struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t src_ip = lwip_ntohl(ip4_addr_get_u32(src));
    uint32_t dst_ip = lwip_ntohl(ip4_addr_get_u32(dest));
    return selected_routing_hook(src_ip, dst_ip);
}

// ---------------- Routing task ----------------
void routing_task(void *pvParameters) {
    routing_t *rt = (routing_t *)pvParameters;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        rt_on_tick(rt, 1000);
    }
}

// ---------------- Main ----------------
void app_main(void) {
    node_setup();

    wireless_t *wl = node_get_wireless_instance();
    ring_share_t *rs = node_get_rs_instance();
    node_device_orientation_t orientation = node_get_device_orientation();
    bool is_center_root = node_is_device_center_root();

    sync_init(&_sync, rs, orientation);
    ss_init(&ss, &_sync, rs, orientation);
    rt_create(&rt, rs, wl, &_sync, &ss, orientation);

    // Initialize routing tables
    if (orientation == NODE_DEVICE_ORIENTATION_CENTER) {
        if (is_center_root) {
            rt_init_root(&rt, r_subnet, r_mask);
        } else {
            rt_init_home(&rt);
        }
    } else {
        rt_init_forwarder(&rt);
    }

    rt_on_start(&rt);
    rt_on_tick(&rt, 1);

    // Set AP/STA as needed
    if (orientation == NODE_DEVICE_ORIENTATION_CENTER && is_center_root) {
        node_set_as_ap(r_subnet, r_mask);
    }
    if (orientation == NODE_DEVICE_ORIENTATION_NORTH && !is_center_root) {
        node_set_as_sta();
    }

    // Start routing task
    xTaskCreatePinnedToCore(
        routing_task,
        "routing_task",
        4096,
        &rt,
        tskIDLE_PRIORITY + 2,
        NULL,
        0
    );

    // After initialization, switch to static routing hook
    selected_routing_hook = routing_hook_static;
}




