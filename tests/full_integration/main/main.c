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
#include "routing_hooks.h"
#include "callbacks.h"
#include "node.h"

#define ROOT_NETWORK 0x0A000000  // 10.0.0.0
#define ROOT_MASK 0xFF000000 // 255.0.0.0

static const char *TAG = "main";

static sync_t _sync = { 0 };
static shared_state_t ss = { 0 };

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t src_ip = ip4_addr_get_u32(src);
    uint32_t dst_ip = ip4_addr_get_u32(dest);
    return node_do_routing(src_ip, dst_ip);
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

    wireless_t *wl = node_get_wireless_instance();
    ring_share_t *rs = node_get_rs_instance();
    routing_t *rt = node_get_rt_instance();
    node_device_orientation_t orientation = node_get_device_orientation();
    bool is_center_root = node_is_device_center_root();

    sync_init(&_sync, rs, orientation);
    ss_init(&ss, &_sync, rs, orientation);
    rt_create(rt, rs, wl, &_sync, &ss, orientation);

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER){
        if(is_center_root){
            node_set_routing_hook(ROUTING_HOOK_ROOT_CENTER);
            rt_init_root(rt, ROOT_NETWORK, ROOT_MASK);
        } else {
            node_set_routing_hook(ROUTING_HOOK_HOME);
            rt_init_home(rt);
        }
    } else {
        if(is_center_root){
            node_set_routing_hook(ROUTING_HOOK_ROOT_FORWARDER);
        } else {
            node_set_routing_hook(ROUTING_HOOK_FORWARDER);
        }
        rt_init_forwarder(rt);
    }
    
    rt_on_start(rt);
    rt_on_tick(rt, 1);

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER && is_center_root){
        node_set_as_ap(ROOT_NETWORK, ROOT_MASK);
    }

    if(orientation == NODE_DEVICE_ORIENTATION_NORTH && !is_center_root){ // For testing, have a single one, in reality all non centers should be stations
        node_set_as_sta();
    }
    
    xTaskCreatePinnedToCore(
        routing_task,
        "routing_task",
        4096,
        rt,
        tskIDLE_PRIORITY + 2,
        NULL,
        0
    );

}

