#include "node.h"

#define NODE_NAME_PREFIX "I4A"
#define NODE_LINK_PASSWORD "zWfAc2wXq5"
#define MAX_DEVICES_PER_HOUSE 4

static Device node_device;
static DevicePtr node_device_ptr = &node_device;

static char node_uuid[7];
static bool network_is_setup = false;

static void generate_uuid_from_mac(char *uuid_out, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(uuid_out, len, "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

DevicePtr node_setup(){
    config_setup();
    config_print();

    ESP_ERROR_CHECK(device_wifi_init());
    ESP_ERROR_CHECK(ring_link_init());

    node_device_ptr->device_orientation = config_get_orientation();
    node_device_ptr->device_is_root = (uint8_t) config_mode_is(CONFIG_MODE_ROOT);

    generate_uuid_from_mac(node_uuid, sizeof(node_uuid));

    return node_device_ptr;
}

void node_set_as_sta(){
    if(network_is_setup){
        device_reset(node_device_ptr);
    }

    char *wifi_network_prefix = NODE_NAME_PREFIX;
    char *wifi_network_password = "";

    device_init(node_device_ptr, node_uuid, node_device_ptr->device_orientation, wifi_network_prefix, wifi_network_password, 6, 4, 0, STATION);
    device_start_station(node_device_ptr);
    device_connect_station(node_device_ptr);
    network_is_setup = true;
}

void node_set_as_ap(uint32_t network, uint32_t mask){
    if(network_is_setup){
        device_reset(node_device_ptr);
    }

    uint32_t node_gateway;
    uint8_t ap_channel_to_emit = (rand() % 11) + 1;
    uint8_t ap_max_sta_connections;
    char *wifi_network_prefix = NODE_NAME_PREFIX;
    char *wifi_network_password;

    if (node_device_ptr->device_orientation == CONFIG_ORIENTATION_CENTER) {
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

    device_init(node_device_ptr, node_uuid, node_device_ptr->device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, 0, AP);
    device_set_network_ap(node_device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(node_device_ptr);
    network_is_setup = true;
}


