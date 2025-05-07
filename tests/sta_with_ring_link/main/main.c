#include <stdio.h>
#include "esp_event.h"
#include "esp_check.h"

#include "config.h"
#include "utils.h"
#include "wifi.h"
#include "ring_link.h"
#include "device.h"
#include "client.h"

static const char *TAG = "==> main";

void app_main(void) {

    config_setup();
    config_print();

    Device device;
    DevicePtr device_ptr = &device;

    const char *wifi_ssid_prefix = "AP_Test";
    const char *wifi_password = "test123456";
    const char *device_uuid = "ESP_02";
    uint16_t device_orientation = 3;
    uint8_t ap_channel_to_emit = 6;
    uint8_t ap_max_sta_connections = 4;
    uint8_t device_is_root = 0;
    Device_Mode mode = STATION;

    ESP_ERROR_CHECK(wifi_init());

    ESP_ERROR_CHECK(ring_link_init());

    device_init(device_ptr, device_uuid, device_orientation, wifi_ssid_prefix, wifi_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
    device_start_station(device_ptr);
    device_connect_station(device_ptr);

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

    mode = AP;
    const char *network_cidr = "10.5.6.1";
    const char *network_gateway = "10.5.6.1";
    const char *network_mask = "255.255.255.0";

    device_reset(device_ptr);
    device_init(device_ptr, device_uuid, device_orientation, wifi_ssid_prefix, wifi_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
    device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(device_ptr);

}
