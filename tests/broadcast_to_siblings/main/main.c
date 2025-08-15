#include "esp_log.h"
#include "lwip/esp_netif_net_stack.h"
#include "esp_netif_net_stack.h"
#include "os/os.h"
#include "node.h"

struct netif *custom_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest) {
    return (struct netif *)esp_netif_get_netif_impl(node_get_spi_netif());
}

void test_broadcast_to_siblings(void) {
    void *msg = "Hello siblings!";
    size_t len = strlen(msg);

    bool ok = node_broadcast_to_siblings(msg, len);

    if (ok) {
        log_info("TEST", "Broadcast to siblings succeeded");
    } else {
        log_error("TEST", "Broadcast to siblings failed");
    }
}

void app_main(void) {
    node_setup();
    node_device_orientation_t orientation = node_get_device_orientation();

    if(orientation == NODE_DEVICE_ORIENTATION_CENTER){
        while (true) {
            test_broadcast_to_siblings();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
