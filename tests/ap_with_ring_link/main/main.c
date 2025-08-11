#include <stdio.h>
#include "esp_event.h"

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

static DevicePtr device_ptr;

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    ESP_LOGI(TAG, "Routing hook called for dest: %s", ip4addr_ntoa(dest));

    if (!device_ptr) {
        ESP_LOGW(TAG, "No device found, falling back to SPI");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    esp_netif_t *esp_netif = device_get_netif(device_ptr);
    struct netif *netif = (struct netif *)esp_netif_get_netif_impl(esp_netif);

    if (!netif) {
        ESP_LOGE(TAG, "Failed to get netif from esp_netif");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    const ip4_addr_t *my_ip = ip_2_ip4(&netif->ip_addr);
    const ip4_addr_t *my_netmask = ip_2_ip4(&netif->netmask);

    ESP_LOGI(TAG, "Device IP: %s, Netmask: %s", ip4addr_ntoa(my_ip), ip4addr_ntoa(my_netmask));

    if (ip4_addr_netcmp(dest, my_ip, my_netmask)) {
        ESP_LOGI(TAG, "Dest is in same subnet → route via AP");
        return netif;
    } else {
        ESP_LOGI(TAG, "Dest is NOT in same subnet → route via SPI");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }
}

uint32_t get_subnet_for_orientation(uint8_t orientation) {
    if (orientation >= NUM_ORIENTATIONS) {
        return 0x0A000000; // fallback subnet 10.0.0.0
    }
    return subnets[orientation];
}

void app_main(void) {
    device_ptr = node_setup();
    uint32_t net = get_subnet_for_orientation(device_ptr->device_orientation);
    uint32_t mask = 0xFFFF0000;
    node_set_as_ap(net, mask);
}
