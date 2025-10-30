#include "forwarder.h"

#include <string.h>

#include "../utils.h"
#include "os/os.h"

#define UNUSED(x) ((void)x)

static void on_peer_connected(routing_t *self, const network_t *connection) {
    UNUSED(connection);

    rt_forwarder_state_t *state = GET_STATE(self);

    state->local_state = LOCAL_STATE_CONNECTED;
    rt_peer_event_t peer_event = {
        .event_id = PEER_HANDSHAKE, 
        .payload.handshake = {
            .external_network = state->node_network,
            .provided_network = state->device_network,
            .dtr = state->dtr,
        },
    };

    wl_send_peer_message(self->deps.wl, &peer_event, sizeof(peer_event));
}

static void on_handshake(routing_t *self, const rt_peer_handshake_t *event) {
    rt_forwarder_state_t *state = GET_STATE(self);

    if (state->global_state == GLOBAL_STATE_WITHOUT_NETWORK) {
        // Provision node
        state->dtr = event->dtr;
        state->device_network = get_node_subnet(&event->provided_network, self->orientation);
        state->node_network = event->provided_network;
        state->global_state = GLOBAL_STATE_WITH_NETWORK;
        state->is_local_root = true;

        // Update routing table
        add_global_route(self, &NETWORK(0, 0), self->orientation);

        rt_sibl_event_t provision = {
            .event_id = SIBL_PROVISION,
            .payload.provision = {
                .network = state->node_network,
                .provider_id = self->orientation,
                .dtr = state->dtr + 1,
            },
        };

        rs_broadcast(self->deps.rs, RS_ROUTING, &provision, sizeof(provision));
    } else {
        // We already have a network, add a redundant route to the table
        add_global_route(self, &event->external_network, self->orientation);
    }
}

static void on_update_dtr(routing_t *self, const rt_peer_update_dtr_t *event) {
    rt_forwarder_state_t *state = GET_STATE(self);

    uint32_t peer_dtr = event->dtr;
    uint32_t my_dtr = state->dtr;

    if (peer_dtr == 0) {
        // Peer is not connected to the network
        return;
    }

    if ((my_dtr == 0) || (peer_dtr + 1 < my_dtr)) {
        // I'm not connected to the network or I can improve my DTR
        state->dtr = peer_dtr + 1;
        state->is_local_root = true;
        add_global_route(self, &NETWORK(0, 0), self->orientation);

        rt_sibl_event_t event = {
            .event_id = SIBL_UPDATE_DTR,
            .payload.update_dtr.dtr = peer_dtr + 1,
        };
        rs_broadcast(self->deps.rs, RS_ROUTING, &event, sizeof(event));
    }
}

static void on_new_gateway_request(routing_t *self, const rt_peer_new_gateway_request_t *event) {
    rt_forwarder_state_t *state = GET_STATE(self);

    for (uint32_t i = 0; i < sizeof(event->hag_networks) / sizeof(event->hag_networks[0]); i++) {
        if (event->hag_networks[0].addr == 0)
            break;

        add_global_route(self, &event->hag_networks[i], self->orientation);
    }

    rt_sibl_event_t sibl_event = {
        .event_id = SIBL_SEND_NEW_GATEWAY_REQUEST,
    };

    _Static_assert(  // Ensure we won't generate a buffer overflow in the memcpy
        sizeof(sibl_event.payload.send_new_gateway_request.hag_networks) >= sizeof(event->hag_networks),
        "The sibling event structure supports fewer hag_networks than the peer event counterpart"
    );
    memcpy(&sibl_event.payload.send_new_gateway_request.hag_networks, event->hag_networks, sizeof(event->hag_networks));

    if (state->dtr == 1) {
        // I am root
        rs_broadcast(self->deps.rs, RS_ROUTING, &sibl_event, sizeof(sibl_event));
        return;
    }

    if (state->global_state == GLOBAL_STATE_ON_GW_REQUEST)
        return;

    state->global_state = GLOBAL_STATE_ON_GW_REQUEST;
    state->dtr = 0;
    rs_broadcast(self->deps.rs, RS_ROUTING, &sibl_event, sizeof(sibl_event));
}

static void on_new_gateway_response(routing_t *self, const rt_peer_new_gateway_response_t *event) {
    rt_forwarder_state_t *state = GET_STATE(self);

    uint32_t peer_dtr = event->dtr;

    if ((state->dtr != 0) && (state->dtr <= peer_dtr)) {
        // This DTR is worse than mine
        return;
    }

    state->global_state = GLOBAL_STATE_WITH_NETWORK;
    state->is_local_root = true;
    state->dtr = peer_dtr + 1;

    add_global_route(self, &NETWORK(0, 0), self->orientation);
    rt_sibl_event_t sibl_event = {
        .event_id = SIBL_NEW_GATEWAY_WINNER,
        .payload.new_gateway_winner.dtr = state->dtr,
        .payload.new_gateway_winner.network = event->external_network,
    };
    rs_broadcast(self->deps.rs, RS_ROUTING, &sibl_event, sizeof(sibl_event));
}

static void on_peer_lost(routing_t *self, const network_t *connection) {
    UNUSED(connection);

    rt_forwarder_state_t *state = GET_STATE(self);

    state->local_state = LOCAL_STATE_NOT_CONNECTED;
    remove_routes_by_output(self, self->orientation);

    if (state->is_local_root) {
        log_info(TAG, "[peer_lost] Connection to ROOT node has been lost");
        state->is_local_root = false;
        state->dtr = 0;
        state->global_state = GLOBAL_STATE_ON_GW_REQUEST;
        rt_sibl_event_t sibl_event = {
            .event_id = SIBL_SEND_NEW_GATEWAY_REQUEST,
        };
        rs_broadcast(self->deps.rs, RS_ROUTING, &sibl_event, sizeof(sibl_event));
    }
}

void rt_fwd_peer_set_required_callbacks(rt_role_impl_t *impl) {
    impl->on_peer_connected = on_peer_connected;
    impl->on_peer_handshake = on_handshake;
    impl->on_peer_update_dtr = on_update_dtr;
    impl->on_peer_new_gateway_request = on_new_gateway_request;
    impl->on_peer_new_gateway_response = on_new_gateway_response;
    impl->on_peer_lost = on_peer_lost;
}