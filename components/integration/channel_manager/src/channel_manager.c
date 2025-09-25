#include "channel_manager/channel_manager.h"
#include <string.h> // for memcpy if needed

#define SIZE 5

// Default orientation array
static const uint8_t default_channels[SIZE] = {1, 6, 3, 9, 11};
static channel_manager_t channel_manager = { 0 };
static channel_manager_t *cm = &channel_manager;

// Shifted orientation logic
static void shift_orientation(const uint8_t *input, uint8_t *output, uint8_t orientation, uint8_t connected_channel) {
    // Find index of connected_channel in the current input array
    int current_index = -1;
    for (int i = 0; i < SIZE; i++) {
        if (input[i] == connected_channel) {
            current_index = i;
            break;
        }
    }

    if (current_index == -1) {
        // Fallback: just use default as-is
        memcpy(output, input, SIZE);
        return;
    }

    // Calculate shift so connected_channel ends up at index = orientation
    int shift = (orientation - current_index + SIZE) % SIZE;

    // Circularly shift the array
    for (int i = 0; i < SIZE; i++) {
        output[i] = input[(i - shift + SIZE) % SIZE];
    }
}

// Called when sibling messages are received
static void on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    if (len != SIZE) return;
    channel_manager_t *cm = ctx;
    cm->suggested_channel = msg[cm->orientation];
}

// Called once during init
void cm_init(ring_share_t *rs, orientation_t orientation) {
    cm->rs = rs;
    cm->orientation = orientation;
    cm->suggested_channel = default_channels[cm->orientation];

    rs_register_component(
        cm->rs, RS_CHANNEL_MANAGER,
        (ring_callback_t){
            .callback = on_sibling_message,
            .context = cm,
        }
    );
    
}

// Broadcast channel provision to siblings
void cm_provide_to_siblings(uint8_t connected_channel) {
    // Shift default orientation so connected_channel ends up at orientation index
    uint8_t shifted[SIZE];
    shift_orientation(default_channels, shifted, cm->orientation, connected_channel);

    // Use the channel that maps to our orientation
    cm->suggested_channel = shifted[cm->orientation];

    // Broadcast the shifted array to siblings
    rs_broadcast(cm->rs, RS_CHANNEL_MANAGER, shifted, SIZE);
}

// Expose the suggested channel
uint8_t cm_get_suggested_channel(void) {
    return cm->suggested_channel;
}
