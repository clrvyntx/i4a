#ifndef _NODE_H_
#define _NODE_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_netif.h"

// Node's device orientations, the values match the ones read in hardware
typedef enum {
    NODE_DEVICE_ORIENTATION_NORTH = 0,
    NODE_DEVICE_ORIENTATION_SOUTH,
    NODE_DEVICE_ORIENTATION_EAST,
    NODE_DEVICE_ORIENTATION_WEST,
    NODE_DEVICE_ORIENTATION_CENTER,
} node_device_orientation_t;

// Startup
void node_setup(void); // Always call this before doing anything with this module

// Mode setting
void node_set_as_ap(uint32_t network, uint32_t mask); // Sets device as AP with desired subnet/mask
void node_set_as_sta(void); // Sets device as Station, scanning for nearby connections

// Node parameters
node_device_orientation_t node_get_device_orientation(void); // Orientation of node's specific device
bool node_is_device_center_root(void); // Tells the device if they're center root or not

// Node communication functions
bool node_send_wireless_message(const uint8_t *msg, uint16_t len); // Send a wireless message to the other side of the wireless link between two devices of different nodes
bool node_broadcast_to_siblings(const uint8_t *msg, uint16_t len); // Broadcast a message to all devices of the same node

// Node network interfaces
esp_netif_t *node_get_wifi_netif(void); // Returns network interface for wireless link
esp_netif_t *node_get_spi_netif(void); // Returns network interface for local communication

#endif // _NODE_H_
