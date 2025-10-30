#ifndef _ROUTING_TYPES_H_
#define _ROUTING_TYPES_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "ring_share/ring_share.h"
#include "shared_state/shared_state.h"
#include "sync/sync.h"
#include "wireless/wireless.h"

#include "routing/impl.h"
#include "routing/routing_table.h"

#define MAX_PEER_EVENT_QUEUED 16
#define MAX_SIBL_EVENT_QUEUED 16

typedef struct rt_dependencies {
    ring_share_t *rs;
    wireless_t *wl;
    sync_t *sync;
    shared_state_t *ss;
} rt_dependencies_t;

typedef enum rt_routing_result {
    ROUTE_NONE = 0,
    ROUTE_SPI,
    ROUTE_WIFI,
} rt_routing_result_t;

typedef struct rt_node_state {
    mutex_t m_lock;
    rt_routing_table_t routing_table;
} rt_node_state_t;

typedef union rt_device_state {
    rt_forwarder_state_t forwarder;
    rt_home_state_t home;
    rt_root_state_t root;
} rt_device_state_t;

typedef struct rt_role {
    rt_device_state_t state;
    rt_role_impl_t impl;
} rt_role_t;

typedef struct rt_internal_queue {
    rt_sibl_event_t queue[MAX_SIBL_EVENT_QUEUED];
    size_t count;
} rt_internal_queue_t;

typedef struct rt_external_queue {
    rt_peer_event_t queue[MAX_PEER_EVENT_QUEUED];
    size_t count;
} rt_external_queue_t;

#endif  // _ROUTING_TYPES_H_