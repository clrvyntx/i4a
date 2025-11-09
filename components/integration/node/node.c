#include "device.h"
#include "config.h"
#include "ring_link.h"
#include "esp_check.h"
#include "callbacks.h"
#include "channel_manager/channel_manager.h"
#include "node.h"

#define ROOT_UUID "000000000000"
#define MAX_DEVICES_PER_HOUSE 4

#define NODE_NAME_PREFIX "I4A"
#define NODE_LINK_PASSWORD "zWfAc2wXq5"

#define NAT_NETWORK_NAME "Internet4All_Root"
#define NAT_NETWORK_PASSWORD "I4A123456"

#define HOUSE_NETWORK_NAME "ComNetAR"

#define UUID_LENGTH 13
#define CALIBRATION_DELAY_SECONDS 2
#define AP_STA_DELAY_SECONDS 1

#define BRIDGE_NETWORK  0xC0A80300  // 192.168.3.0
#define BRIDGE_MASK 0xFFFFFFFC  // /30

#define DEFAULT_SUBNET 0x00000000
#define DEFAULT_MASK 0xFFFFFFFF

static const char *TAG = "node";

typedef struct node {
  DevicePtr node_device_ptr;
  char node_device_uuid[UUID_LENGTH];
  node_device_orientation_t node_device_orientation;
  bool node_device_is_center_root;
  uint32_t node_device_subnet;
  uint32_t node_device_mask;
} node_t;

typedef struct {
  char uuid[UUID_LENGTH];
  uint8_t is_center_root;
} center_broadcast_msg_t;

static node_t node = {
  .node_device_subnet = DEFAULT_SUBNET,
  .node_device_mask = DEFAULT_MASK,
};

static Device node_device = {
  .mode = NAN,
};

static node_t *node_ptr = &node;

static node_device_orientation_t node_get_config_orientation(void){
  config_id_t config_bits = config_get_id();
    if ((config_bits >> 2) == 0) {
      return config_bits;
  } else {
      return NODE_DEVICE_ORIENTATION_CENTER;
  }
}

static void generate_uuid_from_mac(char *uuid_out, size_t len) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  snprintf(uuid_out, len, "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void read_uuid(void *ctx, const uint8_t *data, uint16_t len) {
  if (strlen(node_ptr->node_device_uuid) != 0) {
    return;
  }

  if (len != sizeof(center_broadcast_msg_t)) {
    ESP_LOGW(TAG, "Received unexpected message length: %d", len);
    return;
  }

  const center_broadcast_msg_t *msg = (const center_broadcast_msg_t *)data;

  memcpy(node_ptr->node_device_uuid, msg->uuid, sizeof(msg->uuid));
  node_ptr->node_device_uuid[sizeof(msg->uuid) - 1] = '\0';

  node_ptr->node_device_is_center_root = (bool)msg->is_center_root;
}

static void do_nothing_peer(void *ctx, uint32_t net, uint32_t mask) {
}

static void do_nothing_message(void *ctx, const uint8_t *data, uint16_t len) {
}

static wireless_callbacks_t default_callbacks = {
  .on_peer_connected = do_nothing_peer,
  .on_peer_lost = do_nothing_peer,
  .on_peer_message = do_nothing_message
};

void node_setup(void){
  ESP_ERROR_CHECK(node_init_event_queues());
  ESP_ERROR_CHECK(node_start_event_tasks());

  node_register_wireless_callbacks(default_callbacks, NULL);
  node_register_siblings_callbacks(read_uuid, NULL);

  config_setup();
  config_print();

  node_ptr->node_device_ptr = &node_device;
  node_ptr->node_device_orientation = node_get_config_orientation();

  // Wait in sequence to avoid current peaks while node calibrates
  vTaskDelay(pdMS_TO_TICKS(node_ptr->node_device_orientation * CALIBRATION_DELAY_SECONDS * 1000));

  ESP_ERROR_CHECK(device_wifi_init());
  ESP_ERROR_CHECK(ring_link_init());

  if(node_ptr->node_device_orientation == NODE_DEVICE_ORIENTATION_CENTER){
    node_ptr->node_device_is_center_root = config_mode_is(CONFIG_MODE_ROOT);
    
    if(node_ptr->node_device_is_center_root){
      strncpy(node_ptr->node_device_uuid, ROOT_UUID, UUID_LENGTH);
      node_ptr->node_device_uuid[UUID_LENGTH - 1] = '\0';
    } else {
      generate_uuid_from_mac(node_ptr->node_device_uuid, sizeof(node_ptr->node_device_uuid));
    }

    center_broadcast_msg_t msg;
    memcpy(msg.uuid, node_ptr->node_device_uuid, sizeof(msg.uuid));
    msg.is_center_root = (uint8_t)node_ptr->node_device_is_center_root;

    while (!node_broadcast_to_siblings((uint8_t *)&msg, sizeof(msg))) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Center device UUID broadcasted: %s, Center root: %d", msg.uuid, msg.is_center_root);

  } else {
    while(strlen(node_ptr->node_device_uuid) == 0){
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Peripheral received UUID: %s, Center root: %d", node_ptr->node_device_uuid, node_ptr->node_device_is_center_root);
  }

  node_register_siblings_callbacks(do_nothing_message, NULL);

}

void node_set_as_sta(){
  if(node_ptr->node_device_ptr->mode != NAN){
    device_reset(node_ptr->node_device_ptr);
  }

  char *wifi_network_prefix = NODE_NAME_PREFIX;
  char *wifi_network_password = NODE_LINK_PASSWORD;

  // Wait in sequence to avoid current peaks while STA starts up
  vTaskDelay(pdMS_TO_TICKS(node_ptr->node_device_orientation * AP_STA_DELAY_SECONDS * 1000));

  device_init(node_ptr->node_device_ptr, node_ptr->node_device_uuid, node_ptr->node_device_orientation, wifi_network_prefix, wifi_network_password, 6, 4, (uint8_t)node_ptr->node_device_is_center_root, STATION);
  device_start_station(node_ptr->node_device_ptr);
  device_connect_station(node_ptr->node_device_ptr);
}

void node_set_as_ap(uint32_t network, uint32_t mask){
  if(node_ptr->node_device_ptr->mode != NAN){
    device_reset(node_ptr->node_device_ptr);
  }

  node_ptr->node_device_subnet = network;
  node_ptr->node_device_mask = mask;

  uint32_t node_gateway;
  uint8_t ap_channel_to_emit = cm_get_suggested_channel();
  uint8_t ap_max_sta_connections;
  char *wifi_network_prefix;
  char *wifi_network_password;

  if (node_ptr->node_device_orientation == NODE_DEVICE_ORIENTATION_CENTER) {
    if(node_ptr->node_device_is_center_root){
      network = BRIDGE_NETWORK;
      mask = BRIDGE_MASK;
      node_gateway = network + 2;
      wifi_network_prefix = NAT_NETWORK_NAME;
      wifi_network_password = NAT_NETWORK_PASSWORD;
      ap_max_sta_connections = 1;
    } else {
      node_gateway = network + 1;
      wifi_network_prefix = HOUSE_NETWORK_NAME;
      wifi_network_password = "";
      ap_max_sta_connections = MAX_DEVICES_PER_HOUSE;
    }
  } else {
    network = BRIDGE_NETWORK;
    mask = BRIDGE_MASK;
    node_gateway = network + 2;
    wifi_network_prefix = NODE_NAME_PREFIX;
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

  // Wait in sequence to avoid current peaks while AP starts up
  vTaskDelay(pdMS_TO_TICKS(node_ptr->node_device_orientation * AP_STA_DELAY_SECONDS * 1000));
  
  if (node_ptr->node_device_orientation == NODE_DEVICE_ORIENTATION_CENTER || node_ptr->node_device_is_center_root){
    device_init(node_ptr->node_device_ptr, node_ptr->node_device_uuid, node_ptr->node_device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, (uint8_t)node_ptr->node_device_is_center_root, AP);
    device_set_network_ap(node_ptr->node_device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(node_ptr->node_device_ptr);
  } else {
    device_init(node_ptr->node_device_ptr, node_ptr->node_device_uuid, node_ptr->node_device_orientation, wifi_network_prefix, wifi_network_password, ap_channel_to_emit, ap_max_sta_connections, (uint8_t)node_ptr->node_device_is_center_root, AP_STATION);
    device_set_network_ap(node_ptr->node_device_ptr, network_cidr, network_gateway, network_mask);
    device_start_ap(node_ptr->node_device_ptr);
    device_start_station(node_ptr->node_device_ptr);
    device_connect_station(node_ptr->node_device_ptr);
  }
}

node_device_orientation_t node_get_device_orientation(void){
  return node_ptr->node_device_orientation;
}

bool node_is_device_center_root(void){
  return node_ptr->node_device_is_center_root;
}

bool node_broadcast_to_siblings(const uint8_t *msg, uint16_t len){
  return broadcast_to_siblings(msg, len);
}

bool node_send_wireless_message(const uint8_t *msg, uint16_t len){
  return device_send_wireless_message(node_ptr->node_device_ptr, msg, len);
}

bool node_is_point_to_point_message(uint32_t dst){
  return ((dst & BRIDGE_MASK) == BRIDGE_NETWORK);
}

bool node_is_packet_for_this_subnet(uint32_t dst) {
    return ((dst & node_ptr->node_device_mask) == node_ptr->node_device_subnet);
}

esp_netif_t *node_get_wifi_netif(void) {
  return device_get_netif(node_ptr->node_device_ptr);
}

esp_netif_t *node_get_spi_netif(void) {
  return get_ring_link_tx_netif();
}

