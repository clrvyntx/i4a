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

bool get_higher_priority_mask(ip4_addr_t* mask1, ip4_addr_t* mask2)
{
    return ntohl(mask1->addr) > ntohl(mask2->addr);
}

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest)
{
    struct netif *netif = NULL;
    struct netif *higher_priority_netif = NULL;

    if ((dest != NULL) && !ip4_addr_isany(dest)) {
        ESP_LOGD(TAG, "Evaluando rutas para destino: %s", ip4addr_ntoa(dest));

        for (netif = netif_list; netif != NULL; netif = netif->next) {
            if (!netif_is_up(netif) || !netif_is_link_up(netif)) continue;
            if (!ip4_addr_netcmp(dest, netif_ip4_addr(netif), netif_ip4_netmask(netif))) continue;

            ESP_LOGD(TAG, "Candidato: %s, mascara: %s",
                     ip4addr_ntoa(netif_ip4_addr(netif)),
                     ip4addr_ntoa(netif_ip4_netmask(netif)));

            if (higher_priority_netif == NULL ||
                get_higher_priority_mask(netif_ip4_netmask(netif), netif_ip4_netmask(higher_priority_netif))) {
                ESP_LOGD(TAG, " -> Nueva mejor interfaz encontrada");
            higher_priority_netif = netif;
                }
        }
    }

    if (higher_priority_netif != NULL) {
        ESP_LOGD(TAG, "Buscando interfaz con gateway coincidente: %s",
                 ip4addr_ntoa(netif_ip4_gw(higher_priority_netif)));

        for (netif = netif_list; netif != NULL; netif = netif->next) {
            if (netif_is_up(netif) && netif_is_link_up(netif) &&
                !ip4_addr_isany_val(*netif_ip4_addr(netif)) &&
                ip4_addr_cmp(netif_ip4_gw(higher_priority_netif), netif_ip4_addr(netif))) {
                ESP_LOGD(TAG, "Interfaz seleccionada (por gateway): %s", ip4addr_ntoa(netif_ip4_addr(netif)));
            return netif;
                }
        }
    } else {
        ESP_LOGW(TAG, "No se encontró interfaz con red coincidente.");
    }

    if ((src != NULL) && !ip4_addr_isany(src)) {
        ESP_LOGD(TAG, "Buscando interfaz por IP de origen: %s", ip4addr_ntoa(src));

        for (netif = netif_list; netif != NULL; netif = netif->next) {
            if (netif_is_up(netif) && netif_is_link_up(netif) &&
                !ip4_addr_isany_val(*netif_ip4_addr(netif)) &&
                ip4_addr_cmp(src, netif_ip4_addr(netif))) {
                ESP_LOGD(TAG, "Interfaz seleccionada (por origen): %s", ip4addr_ntoa(netif_ip4_addr(netif)));
            return netif;
                }
        }
    }

    ESP_LOGW(TAG, "No se encontró ninguna interfaz para enrutar el paquete.");
    return NULL;
}

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

    ESP_ERROR_CHECK(device_wifi_init());

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
    char *network_cidr = "10.5.6.1";
    char *network_gateway = "10.5.6.1";
    char *network_mask = "255.255.255.0";

    device_reset(device_ptr);

    vTaskDelay(pdMS_TO_TICKS(10000));

    device_init(device_ptr, device_uuid, device_orientation, wifi_ssid_prefix, wifi_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
    device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(device_ptr);

    vTaskDelay(pdMS_TO_TICKS(10000));

    mode = STATION;

    device_reset(device_ptr);

    vTaskDelay(pdMS_TO_TICKS(10000));

    device_init(device_ptr, device_uuid, device_orientation, wifi_ssid_prefix, wifi_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
    device_start_station(device_ptr);
    device_connect_station(device_ptr);

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

    mode = AP;
    network_cidr = "10.8.22.1";
    network_gateway = "10.8.22.1";
    network_mask = "255.255.255.0";

    device_reset(device_ptr);

    vTaskDelay(pdMS_TO_TICKS(10000));

    device_init(device_ptr, device_uuid, device_orientation, wifi_ssid_prefix, wifi_password, ap_channel_to_emit, ap_max_sta_connections, device_is_root, mode);
    device_set_network_ap(device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(device_ptr);

}
