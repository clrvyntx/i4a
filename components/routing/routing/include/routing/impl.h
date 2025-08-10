#ifndef _I4A_ROUTING_IMPL_H_
#define _I4A_ROUTING_IMPL_H_

#include <stdint.h>

#include "routing_config/routing_config.h"

// Forward decl
struct routing;

/**
 * Subnet defined by a network address and a subnet mask.
 */
typedef struct network {
    uint32_t addr;
    uint32_t mask;
} network_t;

#define NETWORK(nw_addr, nw_mask)        \
    (network_t) {                        \
        .addr = nw_addr, .mask = nw_mask \
    }

typedef struct rt_root_state {
    /**
     * Network that will be distributed by the
     * root node. Usually: 10.0.0.0/8.
     */
    network_t network;

    /**
     * Flag for the gateway requested timer.
     *
     * If true, the timer is enabled and a timeout will occur
     * in `gateway_requested_timeout` milliseconds.
     */
    bool gateway_requested;

    /**
     * Number of milliseconds left for the gateway timer to
     * expire.
     */
    uint32_t gateway_requested_timeout;
} rt_root_state_t;

typedef struct rt_home_state {
    /**
     * If true, this device was already provisioned.
     */
    bool is_provisioned;
} rt_home_state_t;

typedef struct rt_forwarder_state {
    /**
     * Subnet assigned to this node when it joined the
     * network.
     */
    network_t node_network;

    /**
     * Portion of the node's subnet assigned to this specific
     * device.
     */
    network_t device_network;

    /**
     * If true, this device is acting as the gateway to the root
     * node.
     */
    bool is_local_root;

    /**
     * Indicates if this device is connected to a wireless peer.
     */
    enum {
        LOCAL_STATE_NOT_CONNECTED = 0,
        LOCAL_STATE_CONNECTED = 1,
    } local_state;

    /**
     * Indicates the relation of this node with the
     * network.
     */
    enum {
        GLOBAL_STATE_WITH_NETWORK = 1,
        GLOBAL_STATE_WITHOUT_NETWORK = 2,
        GLOBAL_STATE_READY = 3,
        GLOBAL_STATE_ON_GW_REQUEST = 4,
    } global_state;

    /**
     * Distance to the root node through the current gateway.
     *
     * This distance is measured in the number of _nodes_ that
     * a packet needs to jump though in order to get to the root
     * node.
     */
    uint32_t dtr;
} rt_forwarder_state_t;

typedef struct rt_peer_handshake {
    network_t external_network;
    network_t provided_network;
    uint32_t dtr;
} rt_peer_handshake_t;

typedef struct rt_peer_update_dtr {
    uint32_t dtr;
} rt_peer_update_dtr_t;

typedef struct rt_peer_new_gateway_request {
    network_t hag_networks[16];  // TODO: Limit max path length
} rt_peer_new_gateway_request_t;

typedef struct rt_peer_new_gateway_response {
    network_t external_network;
    uint32_t dtr;
} rt_peer_new_gateway_response_t;

typedef struct rt_sibl_update_dtr {
    uint32_t dtr;
} rt_sibl_update_dtr_t;

typedef struct rt_sibl_provision {
    uint16_t provider_id;
    uint16_t dtr;
    network_t network;
} rt_sibl_provision_t;

typedef struct rt_sibl_send_new_gateway_request {
    network_t hag_networks[16];  // TODO: Limit max path length
} rt_sibl_send_new_gateway_request_t;

typedef struct rt_sibl_new_gateway_winner {
    network_t network;
    uint32_t dtr;
} rt_sibl_new_gateway_winner_t;

typedef struct rt_sibl_event {
    enum {
        SIBL_ON_START = 1,
        SIBL_UPDATE_DTR,
        SIBL_PROVISION,
        SIBL_SEND_NEW_GATEWAY_REQUEST,
        SIBL_NEW_GATEWAY_WINNER,
    } event_id;
    union {
        rt_sibl_update_dtr_t update_dtr;
        rt_sibl_provision_t provision;
        rt_sibl_send_new_gateway_request_t send_new_gateway_request;
        rt_sibl_new_gateway_winner_t new_gateway_winner;
    } payload;
} rt_sibl_event_t;

typedef struct rt_peer_event {
    enum {
        PEER_HANDSHAKE = 1,
        PEER_UPDATE_DTR,
        PEER_NEW_GATEWAY_REQUEST,
        PEER_NEW_GATEWAY_RESPONSE,
        PEER_CONNECTED,
        PEER_LOST,
    } event_id;
    union {
        rt_peer_handshake_t handshake;
        rt_peer_update_dtr_t update_dtr;
        rt_peer_new_gateway_request_t new_gateway_request;
        rt_peer_new_gateway_response_t new_gateway_response;
        network_t connection;
    } payload;
} rt_peer_event_t;

typedef struct rt_role_impl {
    void (*on_start)(struct routing *self);
    void (*on_tick)(struct routing *self, uint32_t dt_ms);
    void (*on_peer_connected)(struct routing *self, const network_t *conn);
    void (*on_peer_handshake)(struct routing *self, const rt_peer_handshake_t *event);
    void (*on_peer_update_dtr)(struct routing *self, const rt_peer_update_dtr_t *event);
    void (*on_peer_new_gateway_request)(struct routing *self, const rt_peer_new_gateway_request_t *event);
    void (*on_peer_new_gateway_response)(struct routing *self, const rt_peer_new_gateway_response_t *event);
    void (*on_peer_lost)(struct routing *self, const network_t *conn);
    void (*on_sibl_update_dtr)(struct routing *self, const rt_sibl_update_dtr_t *event);
    void (*on_sibl_provision)(struct routing *self, const rt_sibl_provision_t *event);
    void (*on_sibl_send_new_gateway_request)(struct routing *self, const rt_sibl_send_new_gateway_request_t *event);
    void (*on_sibl_new_gateway_winner)(struct routing *self, const rt_sibl_new_gateway_winner_t *event);
} rt_role_impl_t;

#endif  // _I4A_rt_role_IMPL_H_
