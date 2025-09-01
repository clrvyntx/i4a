#include <stdio.h>
#include "esp_log.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "node.h"

#define NUM_ORIENTATIONS 5

const uint32_t subnets[NUM_ORIENTATIONS] = {
    0x0A070000, // orientation 0: 10.7.0.0
    0x0A080000, // orientation 1: 10.8.0.0
    0x0A090000, // orientation 2: 10.9.0.0
    0x0A0A0000, // orientation 3: 10.10.0.0
    0x0A0B0000, // orientation 4: 10.11.0.0
};

static const char *TAG = "==> main";

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t dst_ip = lwip_ntohl(ip4_addr_get_u32(dest));

    if(node_is_message_to_home(dst_ip)){
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    } else {
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }
}

void app_main(void) {
    node_setup();
    node_device_orientation_t orientation = node_get_device_orientation();
    uint32_t net = subnets[orientation];
    uint32_t mask = 0xFFFF0000;
    node_set_as_ap(net, mask);
}
