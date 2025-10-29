#ifndef _I4A_SYNC_IMPL_H_
#define _I4A_SYNC_IMPL_H_

#include <stdbool.h>
#include <stdint.h>

#include "ring_share/ring_share.h"

// Forward declaration
struct sync;

typedef enum {
    SYNC_MSG_TOKEN_REQUEST = 1,
    SYNC_MSG_TOKEN_GRANT,
} sync_msg_id_t;

typedef struct {
    uint8_t destination;
    uint8_t cs_id;
} sync_token_grant_t;

typedef struct {
    uint8_t cs_id;
} sync_token_request_t;

typedef struct {
    uint8_t kind;
    union {
        sync_token_grant_t token_grant;
        sync_token_request_t token_request;
    };
} sync_msg_t;

typedef struct sync_impl {
    void (*on_token_grant)(struct sync *self, const sync_token_grant_t *grant);
    void (*on_token_request)(struct sync *self, const sync_token_request_t *request);
    void (*request_critical_section)(struct sync *self, component_id_t cs_id);
} sync_impl_t;

typedef struct sync_leader {
    uint8_t first_device_to_get_token;
    uint8_t requested_tokens[RS_LAST_COMPONENT_ID];
    bool is_token_out[RS_LAST_COMPONENT_ID];
} sync_leader_t;

#endif  // _I4A_SYNC_IMPL_H_
