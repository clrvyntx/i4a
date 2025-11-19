#include "node.h"
#include "routing_hooks.h"
#include "esp_log.h"

static const char *TAG = "ROUTING";

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
    ESP_LOGI(TAG, "Routing Hook: ROOT_CENTER called");

    if(node_is_point_to_point_message(dst_ip)){
        ESP_LOGI(TAG, "Decision: point-to-point message -> use WIFI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    if(node_is_packet_for_this_subnet(dst_ip)){
        ESP_LOGI(TAG, "Decision: packet for this subnet -> use SPI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    } else {
        ESP_LOGI(TAG, "Decision: packet not for this subnet -> use WIFI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }
}

static struct netif *routing_hook_root_forwarder(uint32_t src_ip, uint32_t dst_ip) {
    ESP_LOGI(TAG, "Routing Hook: ROOT_FORWARDER called");

    if(node_is_point_to_point_message(dst_ip)){
        ESP_LOGI(TAG, "Decision: point-to-point message -> use WIFI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    if(node_is_packet_for_this_subnet(dst_ip)){
        ESP_LOGI(TAG, "Decision: packet for this subnet -> use WIFI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    } else {
        ESP_LOGI(TAG, "Decision: packet not for this subnet -> use SPI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }
}

static struct netif *routing_hook_forwarder(uint32_t src_ip, uint32_t dst_ip) {
    ESP_LOGI(TAG, "Routing Hook: FORWARDER called");

    if(node_is_point_to_point_message(dst_ip)){
        ESP_LOGI(TAG, "Decision: point-to-point message -> use WIFI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    rt_routing_result_t routing_result = rt_do_route(rt, src_ip, dst_ip);

    if(routing_result == ROUTE_WIFI) {
        ESP_LOGI(TAG, "Decision: routing result -> ROUTE_WIFI -> use WIFI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    if(routing_result == ROUTE_SPI) {
        ESP_LOGI(TAG, "Decision: routing result -> ROUTE_SPI -> use SPI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }

    ESP_LOGI(TAG, "Decision: routing result -> unknown -> returning NULL");
    return NULL;
}

static struct netif *routing_hook_home(uint32_t src_ip, uint32_t dst_ip) {
    ESP_LOGI(TAG, "Routing Hook: HOME called");

    if(node_is_packet_for_this_subnet(dst_ip)){
        ESP_LOGI(TAG, "Decision: packet for this subnet -> use WIFI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    } else {
        ESP_LOGI(TAG, "Decision: packet not for this subnet -> use SPI netif");
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }
}

static struct netif *routing_hook_default(uint32_t src_ip, uint32_t dst_ip) {
    ESP_LOGI(TAG, "Routing Hook: DEFAULT called -> returning NULL");
    return NULL;
}

void node_set_routing_hook(routing_hook_type_t hook) {
    if (hook < ROUTING_HOOK_COUNT) {
        ESP_LOGI(TAG, "Setting routing hook -> valid index, updating selected hook");
        selected_routing_hook = routing_hooks[hook];
    } else {
        ESP_LOGI(TAG, "Setting routing hook -> invalid index, using default hook");
        selected_routing_hook = routing_hook_default;
    }
}

struct netif *node_do_routing(uint32_t src, uint32_t dst){
    ESP_LOGI(TAG, "node_do_routing called");
    return selected_routing_hook(src, dst);
}

routing_t *node_get_rt_instance(void){
    ESP_LOGI(TAG, "node_get_rt_instance called");
    return rt;
}
