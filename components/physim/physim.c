#include "physim.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_log.h"

#include "daemon.h"

#define TAG "==> physim"

static uint32_t spi_in_link_id;
static uint32_t spi_out_link_id;



void physim_setup()
{
    physimd_start();

    // Ensure we are connected to the simulation controller
    uint32_t magic = physim_get_config_word(PS_CFG_MAGIC);
    if (magic != PS_MAGIC)
    {
        ESP_LOGE(TAG, "Handshake failed -- aborting");
        abort();
    }

    // Bind links
    uint32_t link_count = physim_get_config_word(PS_CFG_ATTACHED_LINKS_COUNT);
    ESP_LOGI(TAG, "Read PS_CFG_ATTACHED_LINKS_COUNT=%lu", link_count);

    if (link_count > 15) {
        ESP_LOGE(TAG, "Refusing to bind more than 15 links (got %lu)", link_count);
        return;
    }

    if (link_count > 0) {
        channel_t *channels = malloc(sizeof(channel_t) * link_count);
        assert(channels != NULL);

        for (uint32_t i = 0; i < link_count; i++) {
            uint32_t link_id = physim_get_config_word(PS_CFG_ATTACHED_LINK(i));
            physimd_bind_channel(link_id, &channels[i]);
            ESP_LOGI(TAG, "Bound channel %lu", link_id);
        }
    }
}

uint32_t physim_get_config_word(uint32_t word_id)
{
    command_t get_word = {
        .header = {
            .command = PS_CMD_GET_WORD,
            .channel = PS_CHAN_CTRL,
            .payload_size = sizeof(uint32_t)},
        .payload = &word_id};

    physimd_send_command(&get_word);
    physimd_recv_command(PS_CHAN_CTRL, &get_word);

    assert(get_word.header.command == PS_CMD_GET_WORD);
    uint32_t result = *((uint32_t *)get_word.payload);
    free(get_word.payload);
    return result;
}

void physim_link_send(uint16_t link_id, const void *data, uint32_t data_sz)
{
    command_t link_send = {
        .header = {
            .command = PS_CMD_LINK_SEND,
            .channel = link_id,
            .payload_size = data_sz},
        .payload = (void*)data};

    physimd_send_command(&link_send);
}

uint8_t *physim_link_recv(uint16_t link_id, uint32_t *size)
{
    command_t cmd;
    if (!physimd_recv_command(link_id, &cmd))
        return NULL;

    assert(cmd.header.command == PS_CMD_LINK_RECV);
    *size = cmd.header.payload_size;
    return cmd.payload;
}


esp_err_t physim_init(void) {
    // Establish connection with simulation controller
    physim_setup();

    uint32_t simulation_device_id = physim_get_config_word(PS_CFG_DEVICE_ID);
    ESP_LOGI(TAG, "I am device %lu", simulation_device_id);

    spi_in_link_id = physim_get_config_word(SPI_IN);
    ESP_LOGI(TAG, "SPI_IN link id: %lu", spi_in_link_id);

    spi_out_link_id = physim_get_config_word(SPI_OUT);
    ESP_LOGI(TAG, "SPI_OUT link id: %lu", spi_out_link_id);
    
    return ESP_OK;
}

esp_err_t physim_transmit(void *p, size_t len) {
    ESP_LOGI(TAG, "physim_transmit len: %u", len);
    physim_link_send(spi_out_link_id, p, len);
    ESP_LOGI(TAG, "physim_transmit ended");
    return ESP_OK;
}

esp_err_t physim_receive(void *p, size_t len)
{
    ESP_LOGI(TAG, "physim_receive len: %u", len);
    uint32_t buffer_len = 0;
    uint8_t *buffer = physim_link_recv(spi_in_link_id, &buffer_len);
    ESP_LOGI(TAG, "physim_receive buffer_len: %lu", buffer_len);
    if (buffer_len > 0) {
        memccpy(p, buffer, buffer_len, len);
        free(buffer);
    }
    return ESP_OK;
}