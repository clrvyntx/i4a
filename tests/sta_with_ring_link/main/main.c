#include <stdio.h>
#include "esp_event.h"

#include "client.h"
#include "node.h"

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

void app_main(void) {

    device_ptr = node_setup();
    node_set_as_sta();

    const uint8_t message[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xAA, 0xBB, 0xCC };
    uint16_t len = sizeof(message) / sizeof(message[0]);
    int msgs = 0;
    while (msgs < 5) {
        bool success = client_send_message(message, len);

        if (!success) {
            ESP_LOGE("CLIENT", "Error sending message, retrying in 10 seconds...");
            vTaskDelay(pdMS_TO_TICKS(10000)); // Retry after 10 seconds on failure
        } else {
            vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds between successful sends
            msgs++;
        }
    }

    uint32_t net = 0x0A090000; // 10.9.0.0
    uint32_t mask = 0xFFFF0000; // 255.255.0.0
    node_set_as_ap(net, mask);

    vTaskDelay(pdMS_TO_TICKS(10000));

    node_set_as_sta();

    msgs = 0;
    while (msgs < 5) {
        bool success = client_send_message(message, len);

        if (!success) {
            ESP_LOGE("CLIENT", "Error sending message, retrying in 10 seconds...");
            vTaskDelay(pdMS_TO_TICKS(10000)); // Retry after 10 seconds on failure
        } else {
            vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds between successful sends
            msgs++;
        }
    }

    net = 0x0A080000; // 10.8.0.0
    mask = 0xFFFF0000; // 255.255.0.0
    node_set_as_ap(net, mask);

}
