#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "virtual_nic.h"

typedef struct buffer
{
    uint8_t *data;
    size_t len;
} buffer_t;

vnic_result_t vnic_create(vnic_t *self)
{
    self->next = NULL;
    self->esp_driver = NULL;
    self->rx_queue = xQueueCreate(1, sizeof(buffer_t));
    if (!self->rx_queue)
    {
        return VNIC_NO_MEMORY;
    }
    return VNIC_OK;
}

vnic_result_t vnic_bind_receiver(vnic_t *self, vnic_t *rx)
{
    self->next = rx;
    return VNIC_OK;
}

vnic_result_t vnic_transmit(vnic_t *self, const uint8_t *buffer, size_t len)
{
    if (len > VNIC_MAX_LEN)
    {
        return VNIC_INVALID_PARAM;
    }

    if (!self->next)
    {
        return VNIC_NO_RECEIVER;
    }

    buffer_t tx_buffer = {
        .data = calloc(1, len),
        .len = len,
    };

    if (!tx_buffer.data)
    {
        return VNIC_NO_MEMORY;
    }

    memcpy(tx_buffer.data, buffer, len);
    while (xQueueSend(self->next->rx_queue, &tx_buffer, portMAX_DELAY) != pdTRUE)
    {
        // Keep retrying to send -- receiver may be busy
    }
    return VNIC_OK;
}

vnic_result_t vnic_receive(vnic_t *self, uint8_t *buffer, size_t buffer_sz, size_t *bytes_written)
{
    if (buffer_sz < VNIC_MAX_LEN)
    {
        return VNIC_INVALID_PARAM;
    }

    buffer_t rx_buffer = {0};
    while (xQueueReceive(self->rx_queue, &rx_buffer, portMAX_DELAY) != pdTRUE)
    {
        // keep waiting until someone sends something
    }

    memcpy(buffer, rx_buffer.data, rx_buffer.len);

    if (bytes_written)
        *bytes_written = rx_buffer.len;

    free(rx_buffer.data);
    return VNIC_OK;
}

void vnic_destroy(vnic_t *self)
{
    vQueueDelete(self->rx_queue);
    *self = (vnic_t){0};
}