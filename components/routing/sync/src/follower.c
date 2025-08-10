#include "follower.h"

#include <stdint.h>

#include "routing_config/routing_config.h"
#include "ring_share/ring_share.h"
#include "sync/sync.h"

static void on_token_grant(sync_t *sync, const sync_token_grant_t *grant) {
    if (grant->destination != sync->orientation) {
        return;
    }

    if (sync->critical_sections[grant->cs_id].callback) {
        sync_cs_callback_t *callback = &sync->critical_sections[grant->cs_id];
        callback->is_inside = true;
        callback->callback(callback->context);
        callback->is_inside = false;
    }

    sync_msg_t msg = { .kind = SYNC_MSG_TOKEN_GRANT,
                       .token_grant = { .cs_id = grant->cs_id, .destination = (sync->orientation % N_DEVICES) + 1 } };
    rs_broadcast(sync->rs, RS_SYNC, (const uint8_t *)&msg, sizeof(msg));
}

static void request_critical_section(sync_t *sync, component_id_t cs_id) {
    sync_msg_t msg = { .kind = SYNC_MSG_TOKEN_REQUEST, .token_request = { .cs_id = cs_id } };
    rs_broadcast(sync->rs, RS_SYNC, (const uint8_t *)&msg, sizeof(msg));
}

static sync_impl_t FOLLOWER_IMPL = {
    .on_token_grant = on_token_grant,
    .request_critical_section = request_critical_section,
};

sync_impl_t *sync_follower_init(sync_t *sync) {
    (void)sync;
    return &FOLLOWER_IMPL;
}
