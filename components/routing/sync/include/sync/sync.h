#ifndef _I4A_SYNC_H_
#define _I4A_SYNC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "routing_config/routing_config.h"
#include "ring_share/ring_share.h"
#include "sync/impl.h"

/**
 * Implementation of a distributed lock using the Token Ring algorithm.
 *
 * Usage:
 *   - First register a critical section for your component by calling
 *       sync_register_critical_section(COMPONENT_ID, callback)
 *   - Whenever you need a critical section, request one by calling
 *       sync_request_critical_section(COMPONENT_ID)
 *   - Once a token is assigned for the current device you callback
 *     will be called from the siblings thread.
 *   - When the callback is called, it's ensured that the current device
 *     is the only one executing the callback code at that moment.
 *
 *  NOTE: The callback will be called whenever the token is assigned
 *  to this device. This means that it might be called even if you
 *  did not request a critical section but another device did.
 *
 *  NOTE: Each component ID has a different token. Critical sections
 *  are not shared between components.
 */

typedef void (*sync_cs_callback_fn_t)(void *context);

typedef struct sync_cs_callback {
    sync_cs_callback_fn_t callback;
    void *context;
    bool is_inside;
} sync_cs_callback_t;

typedef struct sync {
    ring_share_t *rs;
    uint8_t orientation;
    bool is_leader;
    sync_cs_callback_t critical_sections[RS_LAST_COMPONENT_ID];
    sync_impl_t *impl;
    union {
        sync_leader_t leader;
    } _st;
} sync_t;

/**
 * Initialize the SYNC component.
 *
 * Requires a functioning ring_share instance, it will register itself
 * as component id = RS_SYNC.
 */
bool sync_init(sync_t *self, ring_share_t *rs, orientation_t orientation);

/**
 * Register a critical section for a component.
 *
 * Each component identified by a RS_* constant in the component_id_t enum
 * has a single critical section available to use. If your component requires
 * more than a CS, consider adding a new ID for it in the component_id_t enum.
 */
void sync_register_critical_section(sync_t *self, component_id_t cs_id, sync_cs_callback_fn_t callback, void *ctx);

/**
 * Request a new critical section for the current device.
 *
 * Critical sections are implemented using the Token Ring algorithm.
 * This method will issue a request to the leader device (center) to
 * issue a new token to the SPI ring.
 */
void sync_request_critical_section(sync_t *self, component_id_t cs_id);

/**
 * Returns true if the current device is currently inside the critical
 * section for the given component.
 */
bool sync_is_inside_critical_section(sync_t *sync, component_id_t cs_id);

/**
 * Free resources associated with this component.
 */
void sync_destroy(sync_t *self);

#ifdef __cplusplus
}
#endif

#endif  // _I4A_SYNC_H_
