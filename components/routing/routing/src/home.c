#include "routing/routing.h"

#include "os/os.h"

#include "impl_priv.h"
#include "routing/impl.h"
#include "utils.h"

#define TAG "home"
#define GET_STATE(self) (&((self)->role.state.home))

static void on_start(routing_t *self) {
    ss_refresh(self->deps.ss, RS_ROUTING);
}

static void on_provision(routing_t *self, const rt_sibl_provision_t *provision) {
    rt_home_state_t *state = GET_STATE(self);

    if (state->is_provisioned) {
        log_warn(
            TAG, "[on_provision] Home device already provisioned -- skipping new provision (provider: %u)",
            provision->provider_id
        );
        return;
    }

    network_t my_subnet = get_node_subnet(&provision->network, self->orientation);
    log_warn(TAG, "[on_provision] Got subnet %08X/%d", my_subnet.addr, mask_size(my_subnet.mask));

    add_global_route(self, &my_subnet, self->orientation);
    log_info(TAG, "[on_provision] Added entry to node routing table");

    wl_enable_ap_mode(self->deps.wl, my_subnet.addr, my_subnet.mask);
    log_info(TAG, "[on_provision] Enabled AP mode");

    state->is_provisioned = true;
}

bool create_home_core(routing_t *self) {
    rt_home_state_t *state = GET_STATE(self);
    state->is_provisioned = false;

    self->role.impl = (rt_role_impl_t){
        .on_start = on_start,
        .on_sibl_provision = on_provision,
    };

    return true;
}