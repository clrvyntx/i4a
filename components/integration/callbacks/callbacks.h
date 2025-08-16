#ifndef _CALLBACKS_H_
#define _CALLBACKS_H_

#include "esp_err.h"
#include "wireless/wireless.h"
#include "siblings/siblings.h"

#ifdef __cplusplus
extern "C" {
#endif

// Init
esp_err_t node_init_event_queues(void);
esp_err_t node_start_event_tasks(void);

// Routing algorithm instances
wireless_t *node_get_wireless_instance(void);
siblings_t *node_get_siblings_instance(void);

// Callback wrappers for module interconnection
void node_on_peer_connected(uint32_t net, uint32_t mask);
void node_on_peer_lost(uint32_t net, uint32_t mask);
void node_on_peer_message(void *msg, uint16_t len);
void node_on_sibling_message(void *msg, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // _CALLBACKS_H_
