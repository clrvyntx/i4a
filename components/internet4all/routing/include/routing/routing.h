#ifndef _ROUTING_H_
#define _ROUTING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "ring_share/ring_share.h"
#include "shared_state/shared_state.h"
#include "sync/sync.h"
#include "wireless/wireless.h"

#include "routing/impl.h"
#include "routing/routing_table.h"
#include "routing/types.h"

typedef struct routing {
    /**
     * Module dependencies
     */
    rt_dependencies_t deps;

    /**
     * Current device's orientation.
     */
    orientation_t orientation;

    /**
     * Shared and synchronized node global
     * state.
     */
    rt_node_state_t node_state;

    /**
     * External events queue
     *
     * Holds peer events until the device gets its turn
     * in the critical section.
     */
    rt_external_queue_t external_queue;

    /**
     * Internal events queue
     *
     * Holds sibling messages until the device gets its
     * turn in the critical section.
     */
    rt_internal_queue_t internal_queue;

    /**
     * Private internal device state and role definition.
     */
    rt_role_t role;
} routing_t;

/**
 * Initializes the routing module and binds required dependencies.
 */
bool rt_create(
    routing_t *self, ring_share_t *rs, wireless_t *wl, sync_t *sync, shared_state_t *ss, orientation_t orientation
);

/**
 * Initialization functions. Only one of these should be used depending
 * on the kind of device that's starting up.
 *
 * rt_init_root:
 *  root_network/root_mask: Root's node network. This network will be spreaded
 *  over the rest of the nodes.
 * rt_init_home:
 *  Nothing special, the home device does very little and needs no extra info.
 * rt_init_forwarder:
 *  Nothing else needed.
 *
 * PRECONDITION: rt_create must have been called before.
 */
bool rt_init_root(routing_t *self, uint32_t root_network, uint32_t root_mask);
bool rt_init_home(routing_t *self);
bool rt_init_forwarder(routing_t *self);

/**
 * on_start event.
 *
 * Must be called after initialization but before calling any other routing methods.
 */
void rt_on_start(routing_t *self);

/**
 * Timer update callback.
 *
 * This function should be called periodically in intervals of 1 second
 * or less. It's used to refresh internal timers.
 *
 * dt_ms: Number of milliseconds that have passed since the last call
 *        to this function. E.g. if this function is called every second,
 *        this parameter will have a constant value of 1000 in each invocation.
 */
void rt_on_tick(routing_t *self, uint32_t dt_ms);

/**
 * Routing module output.
 *
 * The routing module will keep an internal routing table based on the information
 * exchanged with thre previous events that can be consulted using this function.
 *
 * This is intended to be used as a hook for lwIP routing function, although a thin
 * wrapper may be needed over it to handle special cases such as loopbacks.
 *
 * This method will return three possible outcomes:
 *  - NO_ROUTE: We can't route this packet, usually because a routing loop was detected.
 *  - ROUTE_SPI: Packet should be sent through SPI interface. Note that packets may
 *               bounce in the SPI interface if the need to be forwarder to a sibling.
 *  - ROUTE_WIFI: Packet should be sent through WLAN interface.
 */
rt_routing_result_t rt_do_route(routing_t *self, uint32_t src_ip, uint32_t dst_ip);

/**
 * Free up resources used by routing module.
 */
void rt_destroy(routing_t *self);

#ifdef __cplusplus
}
#endif

#endif  // _ROUTING_H_