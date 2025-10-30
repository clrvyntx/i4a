#ifndef _I4A_SHARED_STATE_H_
#define _I4A_SHARED_STATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "routing_config/routing_config.h"
#include "ring_share/ring_share.h"
#include "sync/sync.h"

typedef struct shared_data {
    mutex_t lock;
    void *ptr;
    uint8_t length;
} shared_data_t;

typedef struct shared_state {
    sync_t *sync;
    ring_share_t *rs;
    mutex_t broadcast_lock;
    uint8_t orientation;
    shared_data_t data[RS_LAST_COMPONENT_ID];
} shared_state_t;

/**
 * Initializes the shared state component.
 *
 * The device at the center should push the first update of the data.
 */
bool ss_init(shared_state_t *ss, sync_t *sync, ring_share_t *rs, orientation_t orientation);

/**
 * Register an opaque pointer to be watched and copied to all other devices.
 *
 * IMPORTANT: The component that's syncing data must be inside the critical
 * section while calling this method. Otherwise the method will abort.
 */
void ss_watch(shared_state_t *ss, component_id_t component, shared_data_t data);

/**
 * Refreshes the watched data in all the other devices.
 *
 * IMPORTANT: The component that's syncing data must be inside the critical
 * section while calling this method. Otherwise the method will abort.
 */
void ss_refresh(shared_state_t *ss, component_id_t component);

/**
 * Free resources used by this component.
 */
void ss_destroy(shared_state_t *ss);

#ifdef __cplusplus
}
#endif

#endif  // _I4A_SHARED_STATE_H_
