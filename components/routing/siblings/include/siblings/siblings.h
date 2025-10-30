#ifndef _SIBLINGS_H_
#define _SIBLINGS_H_

#include <stdbool.h>
#include <stdint.h>

typedef void (*siblings_callback_t)(void *context, const uint8_t *msg, uint16_t len);

typedef struct siblings {
    siblings_callback_t callback;
    void *context;
} siblings_t;

/**
 * Registers a function to be called whenever a broadcast sibling message is received
 */
void sb_register_callback(siblings_t *sb, siblings_callback_t callback, void *context);

/**
 * Broadcasts a message to all sibling devices.
 *
 * Returns true if the broadcast was successful, false otherwise.
 */
bool sb_broadcast_to_siblings(siblings_t *sb, const uint8_t *msg, uint16_t len);

#endif  // _SIBLINGS_H_
