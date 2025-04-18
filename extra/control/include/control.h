#ifndef _CONTROL_H_
#define _CONTROL_H_

#include "internet4all/siblings/siblings.h"
#include "internet4all/wireless/wireless.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Control module structure
typedef struct control_t {
    wireless_t *wireless;              // Pointer to wireless module state, responsible for managing wireless connections
    siblings_t *siblings;              // Pointer to siblings module, responsible for communication with sibling nodes
    QueueHandle_t routing_event_queue; // Queue to handle events related to routing (e.g., peer connections, messages)
} control_t;

/**
 * @brief Create a new control instance.
 *
 * Allocates memory for a control_t structure. Must be followed by control_init().
 *
 * @return Pointer to control_t on success, NULL on failure.
 */
control_t *control_create(void);

/**
 * @brief Destroys a control instance.
 *
 * Frees all internal allocations and the control structure itself.
 *
 * @param self Pointer to the control_t structure to free.
 */
void control_destroy(control_t *self);

/**
 * @brief Initialize the control subsystem.
 *
 * This function initializes the wireless module, siblings module, creates the event queue,
 * and launches the routing event handler task. It registers the provided callbacks for
 * wireless and siblings-related events.
 *
 * @param self Pointer to the control_t structure to initialize.
 * @param wireless_callbacks Callbacks for handling wireless events (e.g., peer connected, message received).
 * @param siblings_callback Callback for handling sibling messages.
 *
 * @return ESP_OK on success, or an appropriate error code on failure (e.g., ESP_ERR_NO_MEM, ESP_ERR_INVALID_ARG).
 */
esp_err_t control_init(control_t *self, wireless_callbacks_t wireless_callbacks, siblings_callback_t siblings_callback);

#endif  // _CONTROL_H_
