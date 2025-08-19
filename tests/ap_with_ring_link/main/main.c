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
    esp_netif_t *ap_esp_netif = node_get_wifi_netif();
    if (!ap_esp_netif) {
        // Fallback if AP netif is unavailable
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }

    esp_netif_ip_info_t ap_ip_info;
    if (esp_netif_get_ip_info(ap_esp_netif, &ap_ip_info) != ESP_OK) {
        // Fallback if IP info is not ready
        return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
    }

    if (ip4_addr_netcmp(dest, &ap_ip_info.ip, &ap_ip_info.netmask)) {
        // Destination is in the same subnet as AP
        return (struct netif *)esp_netif_get_netif_impl(ap_esp_netif);
    }

    // Fallback: route via SPI interface
    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}

void app_main(void) {
    node_setup();
    node_device_orientation_t orientation = node_get_device_orientation();
    uint32_t net = subnets[orientation];
    uint32_t mask = 0xFFFF0000;
    node_set_as_ap(net, mask);
}
