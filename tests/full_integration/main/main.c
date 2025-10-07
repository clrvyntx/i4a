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

typedef struct netif *(*routing_hook_func_t)(uint32_t src_ip, uint32_t dst_ip);

struct netif *routing_hook_root(uint32_t src_ip, uint32_t dst_ip);
struct netif *routing_hook_forwarder(uint32_t src_ip, uint32_t dst_ip);
struct netif *routing_hook_home(uint32_t src_ip, uint32_t dst_ip);
struct netif *routing_hook_root_forwarder(uint32_t src_ip, uint32_t dst_ip);

typedef enum routing_hook_type {
    ROUTING_HOOK_ROOT,
    ROUTING_HOOK_FORWARDER,
    ROUTING_HOOK_HOME,
    ROUTING_HOOK_ROOT_FORWARDER,
    ROUTING_HOOK_COUNT
} routing_hook_type_t;

static routing_hook_func_t routing_hooks[ROUTING_HOOK_COUNT] = {
    [ROUTING_HOOK_ROOT] = routing_hook_root,
    [ROUTING_HOOK_FORWARDER] = routing_hook_forwarder,
    [ROUTING_HOOK_HOME] = routing_hook_home,
    [ROUTING_HOOK_ROOT_FORWARDER] = routing_hook_root_forwarder
};

// Root hook: route root subnet to SPI, everything else WiFi
struct netif *routing_hook_root(uint32_t src_ip, uint32_t dst_ip) {
    ESP_LOGI(TAG, "ROOT hook: dest_ip=0x%08" PRIx32, dst_ip);

    if(node_is_point_to_point_message(dst_ip)){
        ESP_LOGI(TAG, "Routing P2P message via WiFi");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    if(node_is_packet_for_this_subnet(dst_ip)){
        ESP_LOGI(TAG, "Routing via SPI (dst_ip matches root network)");
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    } else {
        ESP_LOGI(TAG, "Routing via WiFi (dst_ip is outside root network)");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }
}

// Root forwarder hook: route own subnet to WiFi, everything else SPI
struct netif *routing_hook_root_forwarder(uint32_t src_ip, uint32_t dst_ip) {
    ESP_LOGI(TAG, "ROOT FORWARDER hook: dest_ip=0x%08" PRIx32, dst_ip);

    if(node_is_point_to_point_message(dst_ip)){
        ESP_LOGI(TAG, "Routing P2P message via WiFi");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    if(node_is_packet_for_this_subnet(dst_ip)){
        ESP_LOGI(TAG, "Routing via WiFi (dst_ip matches my own network)");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    } else {
        ESP_LOGI(TAG, "Routing via SPI (dst_ip does NOT match my own network)");
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }
}

// Forwarder hook: route point-to-point messages via WiFi, else use routing table
struct netif *routing_hook_forwarder(uint32_t src_ip, uint32_t dst_ip) {
    ESP_LOGI(TAG, "FORWARDER hook: src_ip=0x%08" PRIx32 ", dest_ip=0x%08" PRIx32, src_ip, dst_ip);

    if(node_is_point_to_point_message(dst_ip)){
        ESP_LOGI(TAG, "Routing P2P message via WiFi");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    rt_routing_result_t routing_result = rt_do_route(&rt, src_ip, dst_ip);

    switch (routing_result) {
        case ROUTE_WIFI:
            ESP_LOGI(TAG, "Routing result: WIFI");
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());

        case ROUTE_SPI:
            ESP_LOGI(TAG, "Routing result: SPI");
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());

        default:
            ESP_LOGW(TAG, "Routing result: UNKNOWN");
            return NULL;
    }
}

// Home hook: if message to home subnet, route via WiFi, else SPI
struct netif *routing_hook_home(uint32_t src_ip, uint32_t dst_ip) {
    ESP_LOGI(TAG, "HOME hook: dest_ip=0x%08" PRIx32, dst_ip);

    if(node_is_packet_for_this_subnet(dst_ip)){
        ESP_LOGI(TAG, "Routing message to home via WiFi");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    } else {
        ESP_LOGI(TAG, "Routing non-home message via SPI");
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }
}

struct netif *routing_hook_default(uint32_t src_ip, uint32_t dst_ip) {
    ESP_LOGW(TAG, "Default routing hook called");
    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}

static routing_hook_func_t selected_routing_hook = routing_hook_default;

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t src_ip = lwip_ntohl(ip4_addr_get_u32(src));
    uint32_t dst_ip = lwip_ntohl(ip4_addr_get_u32(dest));
    return selected_routing_hook(src_ip, dst_ip);
}

void app_main(void) {
    node_setup();

    siblings_t *sb = node_get_siblings_instance();
    wireless_t *wl = node_get_wireless_instance();
    node_device_orientation_t orientation = node_get_device_orientation();
    bool is_center_root = node_is_device_center_root();

    rs_init(&rs, sb);
    sync_init(&_sync, &rs, orientation);
    ss_init(&ss, &_sync, &rs, orientation);
    cm_init(&rs, orientation);
    rt_create(&rt, &rs, wl, &_sync, &ss, orientation);

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER){
        if(is_center_root){
            node_set_as_ap(ROOT_NETWORK, ROOT_MASK);
            selected_routing_hook = routing_hooks[ROUTING_HOOK_ROOT];
            rt_init_root(&rt, ROOT_NETWORK, ROOT_MASK);
        } else {
            selected_routing_hook = routing_hooks[ROUTING_HOOK_HOME];
            rt_init_home(&rt);
        }
    } else {
        if(is_center_root){
            selected_routing_hook = routing_hooks[ROUTING_HOOK_ROOT_FORWARDER];
        } else {
            selected_routing_hook = routing_hooks[ROUTING_HOOK_FORWARDER];
        }
        rt_init_forwarder(&rt);
    }
    
    rt_on_start(&rt);
    rt_on_tick(&rt, 1);

    if(orientation == NODE_DEVICE_ORIENTATION_NORTH && !is_center_root){ // For testing, have a single one, in reality all non centers should be stations
        node_set_as_sta();
    }
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        rt_on_tick(&rt, 1000);
    }
}
