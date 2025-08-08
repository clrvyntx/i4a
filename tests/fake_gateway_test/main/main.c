#include <stdio.h>
#include "esp_event.h"
#include "esp_check.h"

#include "config.h"
#include "utils.h"
#include "wifi.h"
#include "ring_link.h"
#include "device.h"
#include "server.h"

static const char *TAG = "==> main";

Device device;
Device *device_ptr = &device;
Device_Mode mode = AP;

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    ESP_LOGI(TAG, "Routing hook called for dest: %s", ip4addr_ntoa(dest));

    if (!device_ptr) {
        ESP_LOGW(TAG, "No device found, falling back to SPI");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    esp_netif_t *ap_netif = device_get_netif(device_ptr);
    esp_netif_ip_info_t ip_info;

    if (esp_netif_get_ip_info(ap_netif, &ip_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP info from AP netif");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    // Cast to ip4_addr_t* for logging
    ESP_LOGI(TAG, "Device AP IP: %s, Netmask: %s",
             ip4addr_ntoa((const ip4_addr_t *)&ip_info.ip),
             ip4addr_ntoa((const ip4_addr_t *)&ip_info.netmask));

    // Compare destination with our subnet
    if (ip4_addr_netcmp(dest, (const ip4_addr_t *)&ip_info.ip, (const ip4_addr_t *)&ip_info.netmask)) {
        ESP_LOGI(TAG, "Dest is in my subnet -> route via AP");
        return (struct netif *)esp_netif_get_netif_impl(ap_netif);
    } else {
        ESP_LOGI(TAG, "Dest is NOT in my subnet -> route via AP GW");
        return (struct netif *)esp_netif_get_netif_impl(ap_netif);
    }
}

void app_main(void) {
    config_setup();
    config_print();

    char *device_uuid = "12345";

    uint8_t device_orientation = 1;
    uint8_t device_is_root = 0;

    char *wifi_network_prefix = "I4A";
    char *wifi_network_password = "test123456";

    uint8_t ap_channel_to_emit = (rand() % 11) + 1;
    uint8_t ap_max_sta_connections = 4;

    char *network_cidr = "10.0.0.1";
    char *network_gateway = "10.0.0.2";
    char *network_mask = "255.255.0.0";

    ESP_ERROR_CHECK(device_wifi_init());
    ESP_ERROR_CHECK(ring_link_init());

    device_init(device_ptr, device_uuid, device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
    device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(device_ptr);
}
