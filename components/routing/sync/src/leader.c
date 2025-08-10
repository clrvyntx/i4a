#include "leader.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "routing_config/routing_config.h"
#include "os/os.h"
#include "ring_share/ring_share.h"

#define TAG "sync"
#define GET_CTX(self) (&((self)->_st.leader))

static void issue_new_token(sync_t *sync, component_id_t cs_id) {
    sync_leader_t *self = GET_CTX(sync);
    self->is_token_out[cs_id] = true;

    sync_msg_t msg = { .kind = SYNC_MSG_TOKEN_GRANT,
                       .token_grant = {
                           .cs_id = cs_id,
                           .destination = self->first_device_to_get_token,
                       } };

    rs_broadcast(sync->rs, RS_SYNC, (const uint8_t *)&msg, sizeof(msg));
}

static void request_critical_section(sync_t *sync, component_id_t cs_id) {
    sync_leader_t *self = GET_CTX(sync);

    if (self->is_token_out[cs_id]) {
        if (self->requested_tokens[cs_id] < 255) {
            self->requested_tokens[cs_id]++;
        } else {
            log_warn(TAG, "Too many tokens requested for cs_id=%u (>255)", cs_id);
        }
    } else {
        issue_new_token(sync, cs_id);
    }
}

static void on_token_grant(sync_t *sync, const sync_token_grant_t *grant) {
    sync_leader_t *self = GET_CTX(sync);
    if (grant->destination != sync->orientation)
        return;

    component_id_t cs_id = grant->cs_id;
    if (sync->critical_sections[cs_id].callback) {
        sync_cs_callback_t *callback = &sync->critical_sections[cs_id];
        callback->is_inside = true;
        callback->callback(callback->context);
        callback->is_inside = false;
    }

    if (self->requested_tokens[cs_id] > 0) {
        self->requested_tokens[cs_id]--;
        issue_new_token(sync, cs_id);
    } else {
        self->is_token_out[cs_id] = false;
    }
}

static void on_token_request(sync_t *sync, const sync_token_request_t *request) {
    component_id_t cs_id = request->cs_id;
    request_critical_section(sync, cs_id);
}

static sync_impl_t LEADER_IMPL = {
    .on_token_grant = on_token_grant,
    .on_token_request = on_token_request,
    .request_critical_section = request_critical_section,
};

sync_impl_t *sync_leader_init(sync_t *sync) {
    sync_leader_t *leader_ctx = GET_CTX(sync);

    memset(leader_ctx, 0, sizeof(sync_leader_t));
    leader_ctx->first_device_to_get_token = ORIENTATION_NORTH;

    return &LEADER_IMPL;
}
