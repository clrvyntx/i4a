#ifndef _DAEMON_H_
#define _DAEMON_H_

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define PS_CHAN_CTRL 0xFFFF

#define PS_CMD_GET_WORD  0xaa00
#define PS_CMD_LINK_SEND 0xbb00
#define PS_CMD_LINK_RECV 0xbb01

// Special configuration variables
#define PS_CFG_ATTACHED_LINKS_COUNT (PS_CFG_BASE + 0x01)
#define PS_CFG_ATTACHED_LINK(id)    (PS_CFG_BASE + 0x02 + (id))
#define PS_CFG_MAGIC                0xFFFFFFFF

// Physim's magic number
#define PS_MAGIC 0xaabbccdd


typedef struct header
{
    uint16_t command;
    uint16_t channel;
    uint32_t payload_size;
} header_t;

typedef struct command
{
    header_t header;
    void *payload;
} command_t;

typedef struct channel
{
    uint16_t channel_id;
    QueueHandle_t queue;
    struct channel *next;
} channel_t;

void physimd_start();
void physimd_send_command(const command_t *cmd);
void physimd_bind_channel(uint16_t channel, channel_t *storage);
bool physimd_recv_command(uint16_t channel, command_t *cmd);

#endif // _DAEMON_H_