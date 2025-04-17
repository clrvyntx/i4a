#pragma once

// C libraries
#include <stdio.h>
#include <stdint.h>

#include "config.h"
#include "ring_link_lowlevel_impl.h"

#ifdef __cplusplus
extern "C" {
#endif


#define PADDING_SIZE(x) (4 - ((x) % 4))
#define RING_LINK_PAYLOAD_BUFFER_SIZE (RING_LINK_LOWLEVEL_BUFFER_SIZE + PADDING_SIZE(RING_LINK_LOWLEVEL_BUFFER_SIZE))
#define RING_LINK_PAYLOAD_TTL 4

/**
 * @brief Types of payloads in the ring link communication.
 *
 * This enumeration defines the various types of payloads that can be
 * transmitted in the ring link system. The values are carefully chosen
 * to distinguish between internal and external payloads.
 *
 * Internal payloads (< 0x80):
 * - Used for communication within the ring link system itself.
 * - Include types like regular internal messages and heartbeats.
 *
 * External payloads (>= 0x80):
 * - Used for payloads originating from or destined to external systems.
 * - Include types like ESP-NETIF messages.
 */
typedef enum __attribute__((__packed__)) {
    RING_LINK_PAYLOAD_TYPE_INTERNAL = 0x11,
    RING_LINK_PAYLOAD_TYPE_ESP_NETIF = 0x80,
} ring_link_payload_buffer_type_t;

typedef uint8_t ring_link_payload_id_t;

typedef struct
{
    ring_link_payload_id_t id;
    ring_link_payload_buffer_type_t buffer_type;
    uint8_t len;
    uint8_t ttl;
    config_id_t src_id;
    config_id_t dst_id;
    char buffer[RING_LINK_PAYLOAD_BUFFER_SIZE];
} ring_link_payload_t;

bool ring_link_payload_is_for_device(ring_link_payload_t *p);

bool ring_link_payload_is_from_device(ring_link_payload_t *p);

bool ring_link_payload_is_broadcast(ring_link_payload_t *p);

bool ring_link_payload_is_internal(ring_link_payload_t *p);

bool ring_link_payload_is_esp_netif(ring_link_payload_t *p);


#ifdef __cplusplus
}
#endif

