#include "esp_log.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "node.h"

static uint32_t r_subnet = 0x0A000000;
static uint32_t r_mask   = 0xFF000000;

static const char *TAG = "NODE_APP";
static void *message = "Hello from center node!";

// Custom routing hook
struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    uint32_t dst_ip = lwip_ntohl(ip4_addr_get_u32(dest));

    if (node_is_point_to_point_message(dst_ip)) {
        return (struct netif *)esp_netif_get_netif_impl(node_get_wifi_netif());
    }

    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}


void app_main(void) {
    size_t len = strlen(message);
    node_setup();

    uint8_t orientation = node_get_device_orientation();

    if(orientation == NODE_DEVICE_ORIENTATION_NORTH){
        node_set_as_ap(r_subnet, r_mask);
    } 
    
    if(orientation == NODE_DEVICE_ORIENTATION_CENTER){
        node_set_as_sta();
        int msgs = 0;
        while (msgs < 5) {
            bool success = node_send_wireless_message(message, len);
    
            if (!success) {
                ESP_LOGE(TAG, "Error sending message, retrying in 10 seconds...");
                vTaskDelay(pdMS_TO_TICKS(10000)); // Retry after 10 seconds on failure
            } else {
                ESP_LOGI(TAG, "Message successfully sent");
                vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds between successful sends
                msgs++;
            }
        }
    } 

}
