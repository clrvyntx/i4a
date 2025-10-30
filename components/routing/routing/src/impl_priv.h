#ifndef _IMPL_PRIV_H_
#define _IMPL_PRIV_H_

#include <stdint.h>

#include "routing/routing.h"

bool create_root_core(routing_t *self, uint32_t root_network, uint32_t root_mask);
bool create_home_core(routing_t *self);

#endif  // _IMPL_PRIV_H_