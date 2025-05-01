#include "shared_state/shared_state.h"

#include <string.h>

#define TAG "shared_state"

static void on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    shared_state_t *self = ctx;

    uint8_t component = msg[0];
    if (component >= RS_LAST_COMPONENT_ID) {
        os_panic("[shared_state] Invalid component id %u\n", component);
        return;
    }

    shared_data_t *data = &self->data[component];
    if (!data->ptr) {
        os_panic("[shared_state] Component %u has no data to refresh\n", component);
        return;
    }

    if (data->length != len - 1) {
        os_panic("[shared_state] Data length does not match (%u != %u)\n", data->length, len - 1);
        return;
    }

    WITH_LOCK(data->lock, { memcpy(data->ptr, msg + 1, data->length); });
}

bool ss_init(shared_state_t *self, sync_t *sync, ring_share_t *rs, orientation_t orientation) {
    memset(self, 0, sizeof(shared_state_t));

    self->sync = sync;
    self->rs = rs;
    self->orientation = orientation;

    if (!mutex_create(&self->broadcast_lock))
        return false;

    rs_register_component(
        self->rs, RS_SHARED_STATE,
        (ring_callback_t){
            .callback = on_sibling_message,
            .context = self,
        }
    );

    return true;
}

static void broadcast_data(shared_state_t *self, component_id_t component) {
    shared_data_t *data = &self->data[component];

    WITH_LOCK(data->lock, {
        WITH_LOCK(self->broadcast_lock, {
            uint8_t buffer[1 + data->length];
            buffer[0] = component;
            memcpy(buffer + 1, data->ptr, data->length);

            rs_broadcast(self->rs, RS_SHARED_STATE, buffer, 1 + data->length);
        });
    });
}

void ss_watch(shared_state_t *self, component_id_t component, shared_data_t data) {
    self->data[component] = data;  // TODO: There could be a race condition here
}

void ss_refresh(shared_state_t *self, component_id_t component) {
    if (!sync_is_inside_critical_section(self->sync, component)) {
        os_panic("[shared_state] Not inside critical section -- aborting");
        return;
    }

    broadcast_data(self, component);
}

void ss_destroy(shared_state_t *self) {
    rs_register_component(
        self->rs, RS_SHARED_STATE,
        (ring_callback_t){
            .callback = NULL,
            .context = NULL,
        }
    );
    mutex_destroy(&self->broadcast_lock);
}