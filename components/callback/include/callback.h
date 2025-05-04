#ifndef _CONTROL_CALLBACKS_H_
#define _CONTROL_CALLBACKS_H_

#include <stdint.h>  // For uint32_t, uint8_t

/// @brief Function that calls the control plane to communicate
/// a new node has joined the network.
/// @param context A pointer to the user-defined context (typically a control_t *)
/// @param network Network address of the new node.
/// @param mask Network mask.
void on_peer_connected(void *context, uint32_t network, uint32_t mask);

/// @brief Function that calls the control plane to communicate
/// a node has left the network.
/// @param context A pointer to the user-defined context (typically a control_t *)
/// @param network Network address of the lost node.
/// @param mask Network mask.
void on_peer_lost(void *context, uint32_t network, uint32_t mask);

/// @brief Function that calls the control plane to communicate
/// that a new message has been received.
/// @param context A pointer to the user-defined context (typically a control_t *)
/// @param msg Pointer to the message data.
/// @param len Length of the message.
void on_peer_message(void *context, const uint8_t *msg, uint16_t len);

/// @brief Informs the routing subsystem that a SPI sibling device has sent us a control message.
/// @param context A pointer to the user-defined context (typically a control_t *)
/// @param msg Message received.
/// @param len Length of the message. 0 < len <= 512.
void on_sibling_message(void *context, const uint8_t *msg, uint16_t len);

#endif  // _CONTROL_CALLBACKS_H_
