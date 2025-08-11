#include <stdio.h>
#include "esp_event.h"

#include "client.h"
#include "node.h"

static const char *TAG = "==> main";

static DevicePtr device_ptr;

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    ESP_LOGI(TAG, "Routing hook called for dest: %s", ip4addr_ntoa(dest));
    ESP_LOGI(TAG, "Test, route via STA");
    return (struct netif *)esp_netif_get_netif_impl(device_get_netif(device_ptr));
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
