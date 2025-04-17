#pragma once

#include "ring_link_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BROADCAST_TIMEOUT_MS 500


esp_err_t broadcast_init( void );
esp_err_t broadcast_handler(ring_link_payload_t *p);

bool broadcast_to_siblings(const void *msg, uint16_t len);

#ifdef __cplusplus
}
#endif

