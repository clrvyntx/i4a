#include "ring_link.h"
#include "esp_check.h"

#define NODE_NAME_PREFIX "I4A"
#define NODE_LINK_PASSWORD "zWfAc2wXq5"

#include "node.h"

static node_t current_node;
static node_t *current_node_ptr = &current_node;
static bool is_first_network_setup = false;

static char *wifi_network_prefix = NODE_NAME_PREFIX;
static char *wifi_network_password;
static char *network_cidr;
static char *network_gateway;
static char *network_mask;

static void generate_uuid_from_mac(char *uuid_out, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(uuid_out, len, "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

void node_setup(){
    config_setup();
    config_print();

    ESP_ERROR_CHECK(device_wifi_init());
    ESP_ERROR_CHECK(ring_link_init());

    current_node_ptr->node_device_orientation = config_get_orientation();

    if(current_node_ptr->node_device_orientation == CONFIG_ORIENTATION_CENTER){
	generate_uuid_from_mac(current_node_ptr->device_uuid, sizeof(device_uuid));
	current_node_ptr->node_center_is_root = config_mode_is(CONFIG_MODE_ROOT);
	// stay on loop until i can broadcast these 2 things to the rest of the devices of the node
    } else {
	// non center device, stay waiting to receive the broadcast value
    }
}
