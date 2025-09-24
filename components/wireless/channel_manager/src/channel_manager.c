#include "channel_manager/channel_manager.h"

static void on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    channel_manager_t *self = ctx;
}

void cm_init(channel_manager_t *self, ring_share_t *rs, orientation_t orientation) {
    self->rs = rs;
    self->orientation = orientation;
    self->suggested_channel = 3; // random?

    rs_register_component(
        self->rs, RS_CHANNEL_MANAGER,
        (ring_callback_t){
            .callback = on_sibling_message,
            .context = self,
        }
    );

    // at this point, anything broadcasted like this:
    //   rs_broadcast(self->rs, RS_CHANNEL_MANAGER, &message, sizeof(message));
    // will cause on_sibling_message(self, &message, sizeof(message)) to be called on all
    // sibling devices
    // race conditions during start up must be taken into account (i.e. wait until all devices
    // are configured).
}

uint8_t cm_get_suggested_channel(channel_manager_t *self) {
    return self->suggested_channel;
}
