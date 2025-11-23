#include <string.h>
#include "node.h"
#include "traffic.h"
#include "esp_log.h"
#include "info_manager/info_manager.h"

static const char *TAG = "info_manager";

static im_manager_t info_manager = {0};
static im_manager_t *im = &info_manager;

static void im_on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    if (len < sizeof(im_ring_packet_t)) return;

    const im_ring_packet_t *pkt = (const im_ring_packet_t *)msg;
    uint8_t idx = pkt->orientation;

    if (idx >= MAX_ORIENTATIONS) {
        ESP_LOGW(TAG, "Invalid orientation %d received", idx);
        return;
    }

    im->ring[idx] = *pkt;
    ESP_LOGD(TAG, "Updated info for orientation %d: UUID=%s", idx, pkt->uuid);
}

void im_init(ring_share_t *rs) {
    if (!rs) {
        ESP_LOGE(TAG, "Invalid parameters to im_init");
        return;
    }

    im->rs = rs;
    memset(im->ring, 0, sizeof(im->ring));

    rs_register_component(
        im->rs,
        RS_INFO_MANAGER,
        (ring_callback_t){
            .callback = im_on_sibling_message,
            .context = im
        }
    );

    ESP_LOGI(TAG, "Info manager initialized");
}

bool im_broadcast_info(void) {
    if (!im->rs) {
        ESP_LOGW(TAG, "Cannot broadcast: ring_share not initialized");
        return false;
    }

    im_ring_packet_t pkt = {0};
    pkt.orientation = node_get_device_orientation();
    pkt.subnet = node_get_device_subnet();
    pkt.mask = node_get_device_mask();
    pkt.is_root = node_is_device_center_root();
    pkt.rssi = node_get_device_rssi();
    pkt.rx_bytes = node_traffic_get_rx_count();
    pkt.tx_bytes = node_traffic_get_tx_count();
    snprintf(pkt.uuid, sizeof(pkt.uuid), "%s", node_get_uuid());

    im->ring[pkt.orientation] = pkt;

    bool result = rs_broadcast(im->rs, RS_INFO_MANAGER, (uint8_t *)&pkt, sizeof(pkt));

    if(result) {
        node_traffic_reset_counters(); // Reset TX/RX counters as everyone got the bytes properly
    }

    return result;
}

im_ring_packet_t *im_get_ring_info(void) {
    return im->ring;
}
