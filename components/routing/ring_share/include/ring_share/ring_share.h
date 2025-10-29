#ifndef _I4A_RING_SHARE_
#define _I4A_RING_SHARE_

#ifdef __cplusplus
#define _Static_assert static_assert
extern "C" {
#endif

#include <stdint.h>

#include "os/os.h"
#include "siblings/siblings.h"

#define RS_MAX_BROADCAST_LEN 512
#define RS_MAX_COMPONENTS 5

typedef enum component_id {
    RS_SYNC = 0,
    RS_ROUTING = 1,
    RS_SHARED_STATE = 2,
    RS_CHANNEL_MANAGER = 3,

    /* Keep this variant last */
    RS_LAST_COMPONENT_ID,
} component_id_t;

_Static_assert(RS_LAST_COMPONENT_ID <= RS_MAX_COMPONENTS, "Too many components, raise RS_MAX_COMPONENT");

typedef void (*ring_share_callback_fn_t)(void *ctx, const uint8_t *msg, uint16_t len);

typedef struct ring_callback {
    ring_share_callback_fn_t callback;
    void *context;
} ring_callback_t;

typedef struct ring_share {
    siblings_t *siblings;
    mutex_t broadcast_lock;
    uint8_t buffer[RS_MAX_BROADCAST_LEN + 1];
    ring_callback_t components[RS_MAX_COMPONENTS];
} ring_share_t;

bool rs_init(ring_share_t *self, siblings_t *sb);

void rs_register_component(ring_share_t *self, component_id_t component, ring_callback_t callback);

bool rs_broadcast(ring_share_t *self, component_id_t component, const void *msg, uint16_t len);

void rs_shutdown(ring_share_t *self);

#ifdef __cplusplus
}
#endif

#endif  // _I4A_RING_SHARE_