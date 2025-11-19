#include "node.h"
#include "routing_hooks.h"

static routing_t routing = { 0 };
static routing_t *rt = &routing;

typedef struct netif *(*routing_hook_func_t)(uint32_t src_ip, uint32_t dst_ip);

static struct netif *routing_hook_root_center(uint32_t src_ip, uint32_t dst_ip);
static struct netif *routing_hook_forwarder(uint32_t src_ip, uint32_t dst_ip);
static struct netif *routing_hook_home(uint32_t src_ip, uint32_t dst_ip);
static struct netif *routing_hook_root_forwarder(uint32_t src_ip, uint32_t dst_ip);
static struct netif *routing_hook_default(uint32_t src_ip, uint32_t dst_ip);

static routing_hook_func_t selected_routing_hook = routing_hook_default;

static routing_hook_func_t routing_hooks[ROUTING_HOOK_COUNT] = {
    [ROUTING_HOOK_ROOT_CENTER] = routing_hook_root_center,
    [ROUTING_HOOK_FORWARDER] = routing_hook_forwarder,
    [ROUTING_HOOK_HOME] = routing_hook_home,
    [ROUTING_HOOK_ROOT_FORWARDER] = routing_hook_root_forwarder
};

static struct netif *routing_hook_root_center(uint32_t src_ip, uint32_t dst_ip) {
    if(node_is_point_to_point_message(dst_ip)){
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    if(node_is_packet_for_this_subnet(dst_ip)){
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    } else {
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }
}

static struct netif *routing_hook_root_forwarder(uint32_t src_ip, uint32_t dst_ip) {
    if(node_is_point_to_point_message(dst_ip)){
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    if(node_is_packet_for_this_subnet(dst_ip)){
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    } else {
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }
}

static struct netif *routing_hook_forwarder(uint32_t src_ip, uint32_t dst_ip) {
    if(node_is_point_to_point_message(dst_ip)){
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    rt_routing_result_t routing_result = rt_do_route(rt, src_ip, dst_ip);

    if(routing_result == ROUTE_WIFI) {
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    if(routing_result == ROUTE_SPI) {
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }

    return NULL;
}

static struct netif *routing_hook_home(uint32_t src_ip, uint32_t dst_ip) {
    if(node_is_packet_for_this_subnet(dst_ip)){
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    } else {
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }
}

static struct netif *routing_hook_default(uint32_t src_ip, uint32_t dst_ip) {
    return NULL;
}

void node_set_routing_hook(routing_hook_type_t hook) {
    if (hook < ROUTING_HOOK_COUNT) {
        selected_routing_hook = routing_hooks[hook];
    } else {
        selected_routing_hook = routing_hook_default;
    }
}

struct netif *node_do_routing(uint32_t src, uint32_t dst){
    return selected_routing_hook(src, dst);
}

routing_t *node_get_rt_instance(void){
    return rt;
}
