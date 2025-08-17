#include "daemon.h"

#include <stdint.h>
#include <stdbool.h>

#include "esp_log.h"

#include "ps_uart.h"

#define TAG "physimd"

#define PHYSIMD_TASK_STACK_SIZE 4096

static channel_t channel_list = {0};
static channel_t *channel_list_end = &channel_list;

static void physimd_main(void *)
{
    command_t command;
    while (true)
    {
        read_exact(&command.header, sizeof(command.header));
        if (command.header.payload_size > 0)
        {
            if (command.header.payload_size > PS_MAX_PAYLOAD_SIZE)
            {
                ESP_LOGE(TAG, "Invalid payload size (%lu)", command.header.payload_size);
                continue;
            }

            command.payload = malloc(command.header.payload_size);
            assert(command.payload != NULL);
            read_exact(command.payload, command.header.payload_size);
        }
        else
        {
            command.payload = NULL;
        }

        uint16_t src_channel = command.header.channel;
        for (const channel_t *ch = &channel_list; ch != NULL; ch = ch->next)
        {
            if (ch->channel_id == src_channel)
            {
                if (xQueueSend(ch->queue, &command, 0) != pdTRUE)
                {
                    ESP_LOGE(TAG, "Packet for channel %u dropped", src_channel);
                    if (command.payload)
                        free(command.payload);
                }

                break;
            }
        }
    }
}

void physimd_start()
{
    setup_uart();

    channel_list.channel_id = PS_CHAN_CTRL;
    channel_list.queue = xQueueCreate(1, sizeof(command_t));
    assert(channel_list.queue != NULL);
    channel_list.next = NULL;

    if (xTaskCreate(physimd_main, TAG, PHYSIMD_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL) != pdTRUE)
    {
        ESP_LOGE(TAG, "Could not start task for physim daemon -- aborting");
        abort();
    }
}

void physimd_send_command(const command_t *cmd)
{
    write_all(&cmd->header, sizeof(cmd->header));
    if (cmd->header.payload_size > 0)
        write_all(cmd->payload, cmd->header.payload_size);
}

void physimd_bind_channel(uint16_t channel, channel_t *storage)
{
    storage->channel_id = channel;
    storage->next = NULL;
    storage->queue = xQueueCreate(1, sizeof(command_t));
    assert(storage->queue != NULL);

    // channel_list_end->next could be accessed by the physimd thread
    // however, 32 bit reads and writes are atomic, so no data race can
    // occur here (in the worst case, a command is dropped).
    channel_list_end->next = storage;

    // Don't forget end of list ptr
    channel_list_end = storage;
}

bool physimd_recv_command(uint16_t channel, command_t *cmd)
{
    for (const channel_t *ch = &channel_list; ch != NULL; ch = ch->next)
    {
        if (ch->channel_id == channel)
        {
            return xQueueReceive(ch->queue, cmd, portMAX_DELAY) == pdTRUE;
        }
    }

    ESP_LOGW(TAG, "recv_command from unbound channel %u", channel);
    return false; // Channel is unbound
}
