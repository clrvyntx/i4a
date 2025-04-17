#include "forwarder.h"

#include <string.h>

#include "../utils.h"
#include "os/os.h"

static void on_update_dtr(routing_t *self, const rt_sibl_update_dtr_t *event) {
    rt_forwarder_state_t *state = GET_STATE(self);

    uint32_t peer_dtr = event->dtr;
    if (peer_dtr == 0) {
        log_error(TAG, "[sibl_dtr_update] wrong dtr received");
    } else if ((state->dtr == 0) || (peer_dtr < state->dtr)) {
        state->dtr = peer_dtr;
        state->is_local_root = false;
        if (state->local_state == LOCAL_STATE_CONNECTED) {
            rt_peer_event_t peer_event = {
                .event_id = PEER_UPDATE_DTR, 
                .payload.update_dtr = {
                    .dtr = state->dtr,
                },
            };

            wl_send_peer_message(self->deps.wl, &peer_event, sizeof(peer_event));
        }
    } else {
        log_error(TAG, "[sibl_dtr_update] Worse DTR received (%u > %u)", peer_dtr, state->dtr);
    }
}

static void on_provision(routing_t *self, const rt_sibl_provision_t *event) {
    rt_forwarder_state_t *state = GET_STATE(self);

    if (state->global_state == GLOBAL_STATE_WITH_NETWORK) {
        log_info(TAG, "[on_provision] Device already provisioned -- skipping new provision");
        return;
    }

    log_info(
        TAG, "[on_provision] Provisioned: %08X/%u by %u [dtr=%u]",  //
        event->network.addr, mask_size(event->network.mask), event->provider_id, event->dtr
    );

    state->dtr = event->dtr;
    state->device_network = get_node_subnet(&event->network, self->orientation);
    state->node_network = event->network;
    state->global_state = GLOBAL_STATE_WITH_NETWORK;
    state->is_local_root = false;

    wl_enable_ap_mode(self->deps.wl, state->device_network.addr, state->device_network.mask);
}

static void on_send_new_gateway_request(routing_t *self, const rt_sibl_send_new_gateway_request_t *event) {
    rt_forwarder_state_t *state = GET_STATE(self);

    if (state->dtr == 1)
        return;  // I am root

    if (state->global_state == GLOBAL_STATE_ON_GW_REQUEST)
        return;  // We've already sent it

    if (state->local_state == LOCAL_STATE_NOT_CONNECTED)
        return;  // We don't have a connection to a peer

    rt_peer_event_t peer_event = { .event_id = PEER_NEW_GATEWAY_REQUEST };
    memcpy(peer_event.payload.new_gateway_request.hag_networks, event->hag_networks, sizeof(event->hag_networks));

    network_t *hag_network = find_free_spot(
        peer_event.payload.new_gateway_request.hag_networks,
        sizeof(event->hag_networks) / sizeof(event->hag_networks[0])
    );

    if (!hag_network) {
        log_error(TAG, "[on_send_gw_req] Too many networks in message -- dropping");
        return;
    }

    state->global_state = GLOBAL_STATE_ON_GW_REQUEST;
    state->dtr = 0;
    *hag_network = state->node_network;

    wl_send_peer_message(self->deps.wl, &peer_event, sizeof(peer_event));
}

static void on_new_gateway_winner(routing_t *self, const rt_sibl_new_gateway_winner_t *event) {
    rt_forwarder_state_t *state = GET_STATE(self);

    if (state->dtr == 1) {  // I'm root
        rt_peer_event_t peer_event = {
            .event_id = PEER_NEW_GATEWAY_RESPONSE, 
            .payload.new_gateway_response = {
                .dtr = state->dtr,
                .external_network = state->node_network,
            }, 
        };

        wl_send_peer_message(self->deps.wl, &peer_event, sizeof(peer_event));
        return;
    }

    state->global_state = GLOBAL_STATE_WITH_NETWORK;
    state->is_local_root = false;
    state->dtr = event->dtr + 1;

    rt_peer_event_t peer_event = {
            .event_id = PEER_NEW_GATEWAY_RESPONSE, 
            .payload.new_gateway_response = {
                .dtr = state->dtr,
                .external_network = state->node_network,
            }, 
        };

    wl_send_peer_message(self->deps.wl, &peer_event, sizeof(peer_event));
}

void rt_fwd_sibl_set_required_callbacks(rt_role_impl_t *impl) {
    impl->on_sibl_update_dtr = on_update_dtr;
    impl->on_sibl_provision = on_provision;
    impl->on_sibl_send_new_gateway_request = on_send_new_gateway_request;
    impl->on_sibl_new_gateway_winner = on_new_gateway_winner;
}