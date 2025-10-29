#include "channel_manager/channel_manager.h"
#include "esp_random.h"

#define CHANNELS 5

// Default orientation array
static const uint8_t default_channels[CHANNELS] = {1, 6, 3, 9, 11};
static channel_manager_t channel_manager = { 0 };
static channel_manager_t *cm = &channel_manager;

// Rotate the directional part of the array (N, S, E, W), center stays unchanged
static void rotate_channels(const uint8_t *input, uint8_t *output, int rotation_degrees) {
    // Map directions: [NORTH, SOUTH, EAST, WEST] -> indices 0,1,2,3
    const int dir_indices[4] = {0, 1, 2, 3};

    // Copy CENTER (index 4) as-is
    output[4] = input[4];

    // Determine new index mapping after rotation
    for (int i = 0; i < 4; i++) {
        int new_index = -1;
        switch (rotation_degrees) {
            case 0:
                new_index = dir_indices[i];
                break;
            case 90:
                new_index = dir_indices[(i + 3) % 4];  // Rotate clockwise
                break;
            case 180:
                new_index = dir_indices[(i + 2) % 4];
                break;
            case 270:
                new_index = dir_indices[(i + 1) % 4];
                break;
            default:
                // Invalid rotation, fallback to original
                new_index = dir_indices[i];
                break;
        }
        output[new_index] = input[dir_indices[i]];
    }
}

static int get_rotation_from_channel(uint8_t connected_channel) {
    for (int i = 0; i < 4; i++) {
        if (default_channels[i] == connected_channel) {
            // Map index to rotation needed to bring it to CENTER (index 4)
            switch (i) {
                case 0: return 180; // NORTH
                case 1: return 0;   // SOUTH
                case 2: return 270; // EAST
                case 3: return 90;  // WEST
            }
        }
    }
    // If channel not found or from CENTER, no rotation
    return 0;
}

// Called when sibling messages are received
static void on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    // Don't update on wrong message
    if (len != CHANNELS) {
        return;
    }
    
    channel_manager_t *cm = ctx;

    if (cm == NULL) {
        return;
    }

    if (cm->orientation >= CHANNELS) {
        return;
    }

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
    // Do nothing if channel manager has not been initialized
    if (cm->rs == NULL) {
        return;
    }
    
    // Rotate default orientations
    uint8_t rotated[CHANNELS];
    int rotation = get_rotation_from_channel(connected_channel);
    rotate_channels(default_channels, rotated, rotation);

    // Use the channel that maps to our orientation
    cm->suggested_channel = rotated[cm->orientation];

    // Broadcast the shifted array to siblings
    rs_broadcast(cm->rs, RS_CHANNEL_MANAGER, rotated, CHANNELS);
}

// Expose the suggested channel
uint8_t cm_get_suggested_channel(void) {
    // Return a random number if channel manager has not been initialized
    if (cm->rs == NULL) {
        return (uint8_t)((esp_random() % 11) + 1);
    } else {
     return cm->suggested_channel;
    }
}
