#include "channel_manager/channel_manager.h"
#include <string.h> // for memcpy if needed

#define SIZE 5

// Default orientation array
static const uint8_t DEFAULT_ORIENTATION[SIZE] = {1, 6, 3, 9, 11};

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
    channel_manager_t *self = ctx;
    self->suggested_channel = msg[self->orientation];
}

// Called once during init
void cm_init(channel_manager_t *self, ring_share_t *rs, orientation_t orientation) {
    self->rs = rs;
    self->orientation = orientation;
    self->suggested_channel = DEFAULT_ORIENTATION[self->orientation];

    rs_register_component(
        self->rs, RS_CHANNEL_MANAGER,
        (ring_callback_t){
            .callback = on_sibling_message,
            .context = self,
        }
    );
    
}

// Broadcast channel provision to siblings
void channel_provision(channel_manager_t *self, uint8_t connected_channel) {
    // Shift default orientation so connected_channel ends up at orientation index
    uint8_t shifted[SIZE];
    shift_orientation(DEFAULT_ORIENTATION, shifted, self->orientation, connected_channel);

    // Use the channel that maps to our orientation
    self->suggested_channel = shifted[self->orientation];

    // Broadcast the shifted array to siblings
    rs_broadcast(self->rs, RS_CHANNEL_MANAGER, shifted, SIZE);
}

// Expose the suggested channel
uint8_t cm_get_suggested_channel(channel_manager_t *self) {
    return self->suggested_channel;
}
