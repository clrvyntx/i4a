#include "esp_log.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "node.h"

static const char *TAG = "==> main";

static uint32_t r_subnet = 0x0A000000; // 10.0.0.0
static uint32_t r_mask   = 0xFF000000; // 255.0.0.0

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t dst_ip = lwip_ntohl(ip4_addr_get_u32(dest));
    uint8_t orientation = node_get_device_orientation();

    if (orientation == NODE_DEVICE_ORIENTATION_NORTH) {
		if ((dst_ip & r_mask) != r_subnet) {
			return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
		} else {
			return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
		}
    }

    if (orientation == NODE_DEVICE_ORIENTATION_CENTER) {
        if ((dst_ip & r_mask) == r_subnet) {
            return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
        } else {
            return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
        }
    }

    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}

void app_main(void) {
    node_setup();

    uint8_t orientation = node_get_device_orientation();

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER){
		node_set_as_ap(r_subnet, r_mask);
    }

    if(orientation == NODE_DEVICE_ORIENTATION_NORTH){
		node_set_as_ap(r_subnet, r_mask);
    }

}
