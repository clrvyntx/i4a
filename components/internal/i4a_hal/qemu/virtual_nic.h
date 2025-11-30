#ifndef _VIRTUAL_NIC_H_
#define _VIRTUAL_NIC_H_

#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "esp_netif.h"

#define VNIC_MAX_LEN 1600

typedef enum vnic_result
{
    VNIC_OK = 0,
    VNIC_INVALID_PARAM,
    VNIC_NO_RECEIVER,
    VNIC_BUFFER_FULL,
    VNIC_NO_MEMORY
} vnic_result_t;

typedef struct vnic
{
    struct vnic *next;
    QueueHandle_t rx_queue;
    void *esp_driver;
} vnic_t;

// Initializes a new Virtual NIC instance
vnic_result_t vnic_create(vnic_t *self);

// Binds the receiver's input queue to this vnic output queue.
//
// After this operation, any packet transmitted by this nic will
// be received by `rx`.
//
// Note that this operation replaces any previous receiver and will
// clear the transmission buffer.
vnic_result_t vnic_bind_receiver(vnic_t *self, vnic_t *rx);

// Copies `buffer` to the TX buffer.
//
// This operation will do nothing if vnic_bind_receiver was not previously
// called. If the transmission buffer is full because the receiver has not
// read the information yet, this function will return an error.
//
// Buffer must be smaller than VNIC_MAX_LEN.
//
// Errors returned:
//  - INVALID_PARAM if len >= VNIC_MAX_LEN
//  - BUFFER_FULL if transmission buffer is full
//  - NO_RECEIVER if no receiver has been bound to this nic
vnic_result_t vnic_transmit(vnic_t *self, const uint8_t *buffer, size_t len);

// Waits for a new buffer to arrive.
//
// Buffer must be at least VNIC_MAX_LEN bytes big.
//
// Args:
//  - buffer: buffer of at least VNIC_MAX_LEN bytes
//  - len: size of buffer, in bytes
//  - bytes_written: [out] number of bytes written to buffer
//
// Errors returned:
//  - INVALID_PARAM if len < VNIC_MAX_LEN
vnic_result_t vnic_receive(vnic_t *self, uint8_t *buffer, size_t buffer_sz, size_t *bytes_written);

// Deinits and cleans up any resource allocated by this VNIC.
void vnic_destroy(vnic_t *self);

// Register virtual nic within the ESP Netif subsystem so it can be used with lwIP.
// vnic_result_t vnic_register_esp_netif(vnic_t *self, const char *if_key, const esp_netif_ip_info_t ip_config);
vnic_result_t vnic_register_esp_netif(vnic_t *self, esp_netif_config_t config);

#endif // _VIRTUAL_NIC_H_