#include <arpa/inet.h>
#include "ring_link.h"
#include "esp_check.h"

#define NODE_NAME_PREFIX "I4A"
#define NODE_LINK_PASSWORD "zWfAc2wXq5"
#define MAX_DEVICES_PER_HOUSE 4

#include "node.h"

static node_t current_node;
static bool network_is_setup = false;

static void generate_uuid_from_mac(char *uuid_out, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(uuid_out, len, "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

node_t *node_setup(){
    config_setup();
    config_print();

    ESP_ERROR_CHECK(device_wifi_init());
    ESP_ERROR_CHECK(ring_link_init());

    node_t *current_node_ptr = &current_node;

    current_node_ptr->node_device_orientation = config_get_orientation();
    generate_uuid_from_mac(current_node_ptr->node_uuid, sizeof(current_node_ptr->node_uuid));
    current_node_ptr->node_center_is_root = config_mode_is(CONFIG_MODE_ROOT);

    return current_node_ptr;
}

void node_set_as_sta(node_t *current_node_ptr){
    if(network_is_setup){
        device_reset(current_node_ptr->node_device_ptr);
    }

    char *wifi_network_prefix = NODE_NAME_PREFIX;
    char *wifi_network_password = "";

    device_init(current_node_ptr->node_device_ptr, current_node_ptr->node_uuid, current_node_ptr->node_device_orientation, wifi_network_prefix, wifi_network_password, 6, 4, 0, STATION);
    device_start_station(current_node_ptr->node_device_ptr);
    device_connect_station(current_node_ptr->node_device_ptr);
    network_is_setup = true;
}

void node_set_as_ap(node_t *current_node_ptr, uint32_t network, uint32_t mask){
    if(network_is_setup){
        device_reset(current_node_ptr->node_device_ptr);
    }

    uint32_t node_gateway;
    uint8_t ap_channel_to_emit = (rand() % 11) + 1;
    uint8_t ap_max_sta_connections;
    char *wifi_network_prefix = NODE_NAME_PREFIX;
    char *wifi_network_password;

    if (current_node_ptr->node_device_orientation == CONFIG_ORIENTATION_CENTER) {
        node_gateway = network + 1;
        wifi_network_password = "";
        ap_max_sta_connections = MAX_DEVICES_PER_HOUSE;
    } else {
        node_gateway = network + 2;
        wifi_network_password = NODE_LINK_PASSWORD;
        ap_max_sta_connections = 1;
    }

    struct in_addr net_addr = { .s_addr = htonl(network) };
    struct in_addr gateway_addr = { .s_addr = htonl(node_gateway) };
    struct in_addr mask_addr = { .s_addr = htonl(mask) };

    char network_cidr[INET_ADDRSTRLEN];
    char network_gateway[INET_ADDRSTRLEN];
    char network_mask[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &net_addr, network_cidr, sizeof(network_cidr));
    inet_ntop(AF_INET, &gateway_addr, network_gateway, sizeof(network_gateway));
    inet_ntop(AF_INET, &mask_addr, network_mask, sizeof(network_mask));

    device_init(current_node_ptr->node_device_ptr, current_node_ptr->node_uuid, current_node_ptr->node_device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, 0, AP);
    device_set_network_ap(current_node_ptr->node_device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(current_node_ptr->node_device_ptr);
    network_is_setup = true;
}
