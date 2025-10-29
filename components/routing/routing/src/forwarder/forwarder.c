#include "forwarder.h"

#include "os/os.h"
#include "routing/impl.h"

bool create_forwarder_core(routing_t *self) {
    rt_forwarder_state_t *state = GET_STATE(self);

    state->global_state = GLOBAL_STATE_WITHOUT_NETWORK;
    state->local_state = LOCAL_STATE_NOT_CONNECTED;
    state->node_network = NETWORK(0, 0);
    state->device_network = NETWORK(0, 0);
    state->is_local_root = false;
    state->dtr = 0;

    rt_fwd_sibl_set_required_callbacks(&self->role.impl);
    rt_fwd_peer_set_required_callbacks(&self->role.impl);

    return true;
}