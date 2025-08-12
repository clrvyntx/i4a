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
        ESP_LOGW(TAG, "No device found, falling back to default (SPI)");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    esp_netif_t *ap_esp_netif = device_get_netif(device_ptr);
    if (!ap_esp_netif) {
        ESP_LOGW(TAG, "AP netif is NULL, falling back to SPI");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    esp_netif_ip_info_t ap_ip_info;
    if (esp_netif_get_ip_info(ap_esp_netif, &ap_ip_info) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get IP info for AP netif, falling back to SPI");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    // Compare destination address with AP subnet
    if (ip4_addr_netcmp(dest, &ap_ip_info.ip, &ap_ip_info.netmask)) {
        ESP_LOGI(TAG, "Dest is in the same subnet as AP -> route via AP");
        return (struct netif *)esp_netif_get_netif_impl(ap_esp_netif);
    } else {
        ESP_LOGI(TAG, "Dest is NOT in subnet -> route via SPI");
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
