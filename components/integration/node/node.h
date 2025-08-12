#ifndef _NODE_H_
#define _NODE_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_netif.h"

typedef enum {
    NODE_DEVICE_ORIENTATION_NORTH = 0,
    NODE_DEVICE_ORIENTATION_SOUTH,
    NODE_DEVICE_ORIENTATION_EAST,
    NODE_DEVICE_ORIENTATION_WEST,
    NODE_DEVICE_ORIENTATION_CENTER,
} node_device_orientation_t;

void node_setup(void);
void node_set_as_ap(uint32_t network, uint32_t mask);
void node_set_as_sta(void);
node_device_orientation_t node_get_device_orientation(void);
bool node_is_device_center_root(void);
bool node_send_wireless_message(const uint8_t *msg, uint16_t len);
esp_netif_t *node_get_wifi_netif(void);
esp_netif_t *node_get_spi_netif(void);

#endif // _NODE_H_
