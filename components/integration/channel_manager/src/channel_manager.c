#include "channel_manager/channel_manager.h"
#include "esp_random.h"
#include "esp_log.h"

#define CHANNELS 5

static const char *TAG = "channel_manager";

static const uint8_t formation_1[CHANNELS] = {1, 7, 4, 10, 11};
static const uint8_t formation_2[CHANNELS] = {5, 11, 2, 8, 11};

static channel_manager_t channel_manager = { 0 };
static channel_manager_t *cm = &channel_manager;

static void on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    if (len != CHANNELS) {
        ESP_LOGW(TAG, "Ignoring sibling message with wrong length: %d", len);
        return;
    }
    
    channel_manager_t *cm = ctx;
    cm->suggested_channel = msg[cm->orientation];
    ESP_LOGI(TAG, "Updated suggested channel=%d", cm->suggested_channel);
}

void cm_init(ring_share_t *rs, orientation_t orientation) {
    cm->rs = rs;
    cm->orientation = orientation;
    cm->suggested_channel = formation_1[cm->orientation];

    rs_register_component(
        cm->rs, RS_CHANNEL_MANAGER,
        (ring_callback_t){
            .callback = on_sibling_message,
            .context = cm,
        }
    );

    ESP_LOGI(TAG, "Channel manager initialized");
}

static const uint8_t *select_formation(uint8_t connected_channel) {
    // Check the first 4 channels of formation_1
    for (int i = 0; i < 4; i++) {
        if (formation_1[i] == connected_channel) {
            return formation_2; // swap to the other formation
        }
    }
    // If not in formation_1, use formation_1 explicitly
    return formation_1;
}

bool cm_provide_to_siblings(uint8_t connected_channel) {
    if (cm->rs == NULL) {
        ESP_LOGW(TAG, "Channel manager not initialized. Skipping broadcast.");
        return false;
    }

    const uint8_t *selected_formation = select_formation(connected_channel, cm->orientation);
    cm->suggested_channel = selected_formation[cm->orientation];

    bool broadcast = rs_broadcast(cm->rs, RS_CHANNEL_MANAGER, selected_formation, CHANNELS);
    if(broadcast){
        ESP_LOGI(TAG, "Provided channels to siblings, connected channel=%d)", connected_channel);
    }

    return broadcast;
}

// Expose the suggested channel
uint8_t cm_get_suggested_channel(void) {
    if (cm->rs == NULL) {
        uint8_t random_channel = (uint8_t)((esp_random() % 11) + 1);
        return random_channel;
    } else {
        return cm->suggested_channel;
    }
}
