#include "lwip_custom_hooks.h"
#include "esp_log.h"
#include "device.h"
#include "ring_link_netif_tx.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "lwip/etharp.h"
#include "esp_wifi.h"

static const char *TAG = "custom_ip4_route_src_hook";

static bool get_first_sta_mac(uint8_t mac[6]) {
    wifi_sta_list_t sta_list = {0};
    esp_err_t err = esp_wifi_ap_get_sta_list(&sta_list);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get STA list: %d", err);
        return false;
    }

    if (sta_list.num == 0) {
        ESP_LOGW(TAG, "No stations connected");
        return false;
    }

    // Get MAC of the first station
    memcpy(mac, sta_list.sta[0].mac, 6);
    ESP_LOGI(TAG, "First connected STA MAC: " MACSTR, MAC2STR(mac));
    return true;
}

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    ESP_LOGD(TAG, "Routing hook called for dest: %s", ip4addr_ntoa(dest));

    DevicePtr device = get_current_device();
    if (!device) {
        ESP_LOGW(TAG, "No device found, falling back to default");
        return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
    }

    uint8_t orientation = device->orientation;
    uint8_t is_root = device->device_is_root;

    ESP_LOGD(TAG, "Device orientation: %d, is_root: %d", orientation, is_root);

    ip4_addr_t my_subnet;
    ip4_addr_t mask;
    IP4_ADDR(&mask, 255, 255, 0, 0); // /16

    // === Case 1: Center (device_orientation == 4) ===
    if (orientation == 4) {

        if (is_root) {
            IP4_ADDR(&my_subnet, 10, 9, 0, 0);
            ESP_LOGD(TAG, "Device is center root, subnet 10.9.0.0/16");
        } else {
            IP4_ADDR(&my_subnet, 10, 8, 0, 0);
            ESP_LOGD(TAG, "Device is center non-root, subnet 10.8.0.0/16");
        }

        // Check if dest belongs to my subnet
        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is in my subnet -> route via AP");
            return (struct netif *)esp_netif_get_netif_impl(device_get_netif(device));
        } else {
            ESP_LOGI(TAG, "Dest is NOT in my subnet -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
        }
    }

    // === Case 2: West and Root (device_orientation == 2 && is_root) ===
    if (orientation == 2 && is_root) {
        IP4_ADDR(&my_subnet, 10, 8, 0, 0);
        ESP_LOGD(TAG, "Device is west root, checking for center non-root subnet 10.8.0.0/16");

        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is other node -> route via AP");

            uint8_t mac[6];
            if (get_first_sta_mac(mac)) {
                // Insert static ARP entry for dest IP -> MAC on AP netif
                struct netif *ap_netif = esp_netif_get_netif_impl(device_get_netif(device));
                err_t err = etharp_add_static_entry(dest, mac, ap_netif);
                if (err == ERR_OK) {
                    ESP_LOGI(TAG, "Added static ARP entry: %s -> " MACSTR,
                             ip4addr_ntoa(dest), MAC2STR(mac));
                } else {
                    ESP_LOGW(TAG, "Failed to add static ARP entry: %d", err);
                }
            } else {
                ESP_LOGW(TAG, "Cannot add ARP entry, no STA MAC found");
            }

            return (struct netif *)esp_netif_get_netif_impl(device_get_netif(device));
        } else {
            ESP_LOGI(TAG, "Dest is this node -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
        }
    }

    // === Case 3: East and Not Root (device_orientation == 3 && !is_root) ===
    if (orientation == 3 && !is_root) {
        IP4_ADDR(&my_subnet, 10, 9, 0, 0);
        ESP_LOGD(TAG, "Device is east non-root, checking for center root subnet 10.9.0.0/16");

        // Check if dest belongs to my subnet
        if (ip4_addr_netcmp(dest, &my_subnet, &mask)) {
            ESP_LOGI(TAG, "Dest is other node -> route via STA");
            return (struct netif *)esp_netif_get_netif_impl(device_get_netif(device));
        } else {
            ESP_LOGI(TAG, "Dest is this node -> route via SPI");
            return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
        }
    }

    // === Default: fallback to ring link TX interface ===
    ESP_LOGW(TAG, "Falling back to default routing interface");
    return (struct netif *)esp_netif_get_netif_impl(get_ring_link_tx_netif());
}
