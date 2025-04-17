#include "ring_share/ring_share.h"

#include <stdlib.h>
#include <string.h>

#include "os/os.h"
#include "siblings/siblings.h"

static void on_sibling_message(void *context, const uint8_t *msg, uint16_t len) {
    ring_share_t *self = context;

    if (len == 0) {
        os_panic("[bug] Got sibling message of length = 0");
        return;
    }

    uint8_t component_id = msg[0];
    if (component_id >= RS_MAX_COMPONENTS) {
        os_panic("[bug] Received a sibling message for a component out of range (%u)", msg[0]);
        return;
    }

    ring_callback_t *cb = &self->components[component_id];

    if (cb->callback)
        cb->callback(cb->context, msg + 1, len - 1);
}

bool rs_init(ring_share_t *self, siblings_t *sb) {
    memset(self, 0, sizeof(ring_share_t));

    if (!mutex_create(&self->broadcast_lock)) {
        os_panic("Could not create mutex");
        return false;
    }

    self->siblings = sb;
    sb_register_callback(self->siblings, on_sibling_message, self);
    return true;
}

void rs_register_component(ring_share_t *self, component_id_t component, ring_callback_t callback) {
    self->components[component] = callback;
}

bool rs_broadcast(ring_share_t *self, component_id_t component, const void *msg, uint16_t len) {
    if (len > RS_MAX_BROADCAST_LEN) {
        os_panic("rs_broadcast message too long (%u)", len);
        return false;
    }

    bool ret = false;

    WITH_LOCK(&self->broadcast_lock, {
        self->buffer[0] = component;
        memcpy(self->buffer + 1, msg, len);
        ret = sb_broadcast_to_siblings(self->siblings, self->buffer, len + 1);
    });

    return ret;
}

void rs_shutdown(ring_share_t *self) {
    mutex_destroy(&self->broadcast_lock);
    memset(self, 0, sizeof(ring_share_t));
}