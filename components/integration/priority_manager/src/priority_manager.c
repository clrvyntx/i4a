#include "priority_manager/priority_manager.h"
#include "esp_log.h"

#define MAX_DEVICES 4

static const char *TAG = "priority_manager";

static int8_t devices_rssi[MAX_DEVICES] = {-127, -127, -127, -127};

static priority_manager_t priority_manager = { 0 };
static priority_manager_t *pm = &priority_manager;

typedef struct {
    uint8_t orientation;
    int8_t rssi;
} pm_message_t;

static void on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    if (len != sizeof(pm_message_t)) {
        ESP_LOGW(TAG, "Ignoring sibling message with wrong length: %d", len);
        return;
    }

    const pm_message_t *packet = (const pm_message_t *)msg;
    uint8_t orientation = packet->orientation;
    if (orientation >= MAX_DEVICES) {
        orientation = MAX_DEVICES - 1;
    }
    
    devices_rssi[orientation] = packet->rssi;
    ESP_LOGI(TAG, "Stored RSSI in priority list: orientation=%d, rssi=%d", orientation, devices_rssi[orientation]);

}

void pm_init(ring_share_t *rs, node_device_orientation_t orientation) {
    pm->rs = rs;
    pm->orientation = orientation;

    rs_register_component(
        pm->rs, RS_PRIORITY_MANAGER,
        (ring_callback_t){
            .callback = on_sibling_message,
            .context = pm,
        }
    );

    ESP_LOGI(TAG, "Priority manager initialized");
}

bool pm_provide_to_siblings(int8_t rssi) {
    if (pm->rs == NULL) {
        ESP_LOGW(TAG, "Priority manager not initialized. Skipping broadcast.");
        return false;
    }

    devices_rssi[pm->orientation] = rssi;
    pm_message_t msg = {0};
    msg.orientation = pm->orientation;
    msg.rssi = rssi;

    bool broadcast = rs_broadcast(pm->rs, RS_PRIORITY_MANAGER, (uint8_t *)&msg, sizeof(msg));
    if (broadcast) {
        ESP_LOGI(TAG, "Provided RSSI to siblings: orientation=%d, rssi=%d", msg.orientation, msg.rssi);
    }

    return broadcast;
}

// Expose the suggested orientation
uint8_t pm_get_suggested_priority(void) {
    uint8_t suggested = 0;
    for(int i = 1; i < MAX_DEVICES; i++) {
        if(devices_rssi[i] > devices_rssi[suggested]) {
            suggested = i;
        }
    }
    return suggested;
}

// Reset all values to -127
void pm_reset_priority_list(void) {
    for(int i = 0; i < MAX_DEVICES; i++) {
        devices_rssi[i] = -127;
    }
}
