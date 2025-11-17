#ifndef INTERNAL_MESSAGES_H
#define INTERNAL_MESSAGES_H

#include <stdint.h>
#include "ring_share/ring_share.h"
#include "node.h"

// Initialize internal messages / ring share system
void node_setup_internal_messages(uint8_t orientation);

// Get the global ring_share instance
ring_share_t *node_get_rs_instance(void);

#endif // INTERNAL_MESSAGES_H
