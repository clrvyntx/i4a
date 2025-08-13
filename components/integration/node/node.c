#include "device.h"
#include "config.h"
#include "ring_link.h"
#include "esp_check.h"
#include "client.h"
#include "server.h"
#include "node.h"

#define NODE_NAME_PREFIX "I4A"
#define NODE_LINK_PASSWORD "zWfAc2wXq5"
#define MAX_DEVICES_PER_HOUSE 4

typedef struct node {
  DevicePtr node_device_ptr;
  char node_device_uuid[7];
  config_orientation_t node_device_orientation;
  bool node_device_is_center_root;
} node_t;

static node_t node = { 0 };
static Device node_device = { 0 };

static node_t *node_ptr = &node;
static bool network_is_setup = false;

static void generate_uuid_from_mac(char *uuid_out, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(uuid_out, len, "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

void node_setup(void){
    config_setup();
    config_print();

    ESP_ERROR_CHECK(device_wifi_init());
    ESP_ERROR_CHECK(ring_link_init());

    node_ptr->node_device_ptr = &node_device;
    node_ptr->node_device_orientation = config_get_orientation();
    node_ptr->node_device_is_center_root = config_mode_is(CONFIG_MODE_ROOT);
    generate_uuid_from_mac(node_ptr->node_device_uuid, sizeof(node_ptr->node_device_uuid));

}

void node_set_as_sta(void){
    if(network_is_setup){
        device_reset(node_ptr->node_device_ptr);
    }

    char *wifi_network_prefix = NODE_NAME_PREFIX;
    char *wifi_network_password = NODE_LINK_PASSWORD;

    device_init(node_ptr->node_device_ptr, node_ptr->node_device_uuid, node_ptr->node_device_orientation, wifi_network_prefix, wifi_network_password, 6, 4, 0, STATION);
    device_start_station(node_ptr->node_device_ptr);
    device_connect_station(node_ptr->node_device_ptr);
    network_is_setup = true;
}

void node_set_as_ap(uint32_t network, uint32_t mask){
    if(network_is_setup){
        device_reset(node_ptr->node_device_ptr);
    }

    uint32_t node_gateway;
    uint8_t ap_channel_to_emit = (rand() % 11) + 1;
    uint8_t ap_max_sta_connections;
    char *wifi_network_prefix = NODE_NAME_PREFIX;
    char *wifi_network_password;

    if (node_ptr->node_device_orientation == CONFIG_ORIENTATION_CENTER) {
        node_gateway = network + 1;
        wifi_network_password = "";
        ap_max_sta_connections = MAX_DEVICES_PER_HOUSE;
    } else {
        node_gateway = network + 2;
        wifi_network_password = NODE_LINK_PASSWORD;
        ap_max_sta_connections = 1;
    }

    ip4_addr_t net_addr, gateway_addr, mask_addr;
    net_addr.addr = htonl(network + 1);
    gateway_addr.addr = htonl(node_gateway);
    mask_addr.addr = htonl(mask);

    char network_cidr[IP4ADDR_STRLEN_MAX];
    char network_gateway[IP4ADDR_STRLEN_MAX];
    char network_mask[IP4ADDR_STRLEN_MAX];

    ip4addr_ntoa_r(&net_addr, network_cidr, sizeof(network_cidr));
    ip4addr_ntoa_r(&gateway_addr, network_gateway, sizeof(network_gateway));
    ip4addr_ntoa_r(&mask_addr, network_mask, sizeof(network_mask));

    device_init(node_ptr->node_device_ptr, node_ptr->node_device_uuid, node_ptr->node_device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, 0, AP);
    device_set_network_ap(node_ptr->node_device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(node_ptr->node_device_ptr);
    network_is_setup = true;
}

node_device_orientation_t node_get_device_orientation(void){
  return node_ptr->node_device_orientation;
}

bool node_is_device_center_root(void){
  return node_ptr->node_device_is_center_root;
}

bool node_broadcast_to_siblings(const uint8_t *msg, uint16_t len){
  return broadcast_to_siblings(msg,len);
}

bool node_send_wireless_message(const uint8_t *msg, uint16_t len){
  if(node_ptr->node_device_ptr->mode == AP){
    return server_send_message(msg, len);
  }

  if(node_ptr->node_device_ptr->mode == STATION){
    return client_send_message(msg, len);
  }

  return false;
}

esp_netif_t *node_get_wifi_netif(void) {
    return device_get_netif(node_ptr->node_device_ptr);
}

esp_netif_t *node_get_spi_netif(void) {
    return get_ring_link_tx_netif();
}

void node_on_peer_connected(uint32_t network, uint32_t mask) {
  // Placeholder
}

void node_on_peer_lost(uint32_t network, uint32_t mask) {
  // Placeholder
}

void node_on_peer_message(const uint8_t *msg, uint16_t len) {
  // Placeholder
}

void node_on_sibling_message(const uint8_t *msg, uint16_t len) {
  // Placeholder
}
