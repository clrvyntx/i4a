#ifndef _ROUTING_FORWARDER_H_
#define _ROUTING_FORWARDER_H_

#include "routing/routing.h"

#define TAG "forwarder"
#define GET_STATE(self) (&((self)->role.state.forwarder))

bool create_forwarder_core(routing_t *self);
void rt_fwd_peer_set_required_callbacks(rt_role_impl_t *impl);
void rt_fwd_sibl_set_required_callbacks(rt_role_impl_t *impl);

#endif  // _ROUTING_FORWARDER_H_
