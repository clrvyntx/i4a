#include <stdint.h>
#include <stdbool.h>
#include <socket.h>

#include "esp_netif.h"
#include "esp_log.h"
#include "esp_crc.h"

#include "spi_hal.h"
#include "callbacks.h"

#define BROADCAST_TIMEOUT_MS 1000
#define SPI_SIBLING_UDP_PORT 9797
#define MAX_SPI_MESSAGE_LEN 512
#define TAG "spi_comm"

typedef struct {
    uint32_t checksum;
    uint8_t orientation;
} __attribute__((packed)) spi_header_t;

typedef struct {
    spi_header_t header;
    uint8_t payload[MAX_SPI_MESSAGE_LEN];
} __attribute__((packed)) spi_payload_t;


static struct
{
    int spi_socket;
    spi_payload_t tx_buffer;
    struct sockaddr_in spi_next_hop;
    uint32_t orientation;
    QueueHandle_t msg_back_queue;
} self = {0};

static void task_spi_listener(void *)
{
    spi_payload_t rx_buffer;

    while (true)
    {
        ssize_t total_len = recvfrom(self.spi_socket, &rx_buffer, sizeof(spi_payload_t), 0, NULL, NULL);
        if (total_len <= 0)
        {
            ESP_LOGE(TAG, "recvfrom failed: %u -- aborting", errno);
            abort();
            break;
        }

        if (total_len < sizeof(spi_header_t)) {
            ESP_LOGE(TAG, "got less bytes than an broadcast header - dropping");
            continue;
        }

        // Validate message integrity
        uint32_t expected_crc = rx_buffer.header.checksum;
        // Remove checksum from payload before checking
        rx_buffer.header.checksum = 0;
        uint32_t calculated_crc = esp_crc32_le(0, (const uint8_t*)&rx_buffer, total_len); 

        if (expected_crc != calculated_crc) {
            ESP_LOGE(TAG, "crc mismatch for broadcast (got: %08lX, expected: %08lX) -- dropping",
                calculated_crc, expected_crc);
            continue;
        }

        uint8_t source = rx_buffer.header.orientation;
        if (source == self.orientation)
        {
            int msg_id = 0; // unused
            ESP_LOGD(TAG, "Packet has been broadcasted correctly");
            while (xQueueSend(self.msg_back_queue, &msg_id, portMAX_DELAY) != pdTRUE)
                ;
            continue;
        }
        else
        {
            // Put CRC back in the payload before sending
            rx_buffer.header.checksum = expected_crc;
            if (sendto(self.spi_socket, &rx_buffer, total_len, 0, (const struct sockaddr *)&self.spi_next_hop, sizeof(self.spi_next_hop)) < 0)
            {
                ESP_LOGE(TAG, "sendto failed: errno=%d", errno);
                abort();
                break;
            }

            node_on_sibling_message(rx_buffer.payload, total_len - sizeof(spi_header_t));
        }
    }
}

bool setup_spi_communication(uint32_t orientation)
{
    self.orientation = orientation;
    self.msg_back_queue = xQueueCreate(16, sizeof(uint32_t));
    assert(self.msg_back_queue);

    // Set address of next sibling
    self.spi_next_hop.sin_family = AF_INET;
    self.spi_next_hop.sin_addr.s_addr = ESP_IP4TOADDR(172, 16, 1, 255);
    self.spi_next_hop.sin_port = htons(SPI_SIBLING_UDP_PORT);

    // Create UDP socket and bind it to 127.0.0.x
    self.spi_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (self.spi_socket < 0)
    {
        ESP_LOGE(TAG, "Could not setup SPI communication -- socket failed: %d", errno);
        perror("socket");
        return false;
    }

    struct sockaddr_in my_addr = {0};
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = ESP_IP4TOADDR(172, 16, 1, self.orientation);
    my_addr.sin_port = htons(SPI_SIBLING_UDP_PORT);

    if (bind(self.spi_socket, (const struct sockaddr *)&my_addr, sizeof(my_addr)) < 0)
    {
        ESP_LOGE(TAG, "Could not setup SPI communication -- bind failed: %d", errno);
        perror("bind");
        return false;
    }

    assert(xTaskCreate(task_spi_listener, "spi_listener", 4096, &self, tskIDLE_PRIORITY + 2, NULL) == pdTRUE);
    return true;
}

bool broadcast_to_siblings(const uint8_t *msg, uint16_t len)
{
    // Send to the next sibling -- it'll know what to do
    if (len > MAX_SPI_MESSAGE_LEN)
    {
        ESP_LOGE(TAG, "Tried to broadcast a siblings message bigger than 512B (len=%u)", len);
        return false;
    }

    size_t total_len = sizeof(spi_header_t) + len;
    self.tx_buffer.header.checksum = 0;
    self.tx_buffer.header.orientation = (uint8_t) self.orientation;
    memcpy(self.tx_buffer.payload, msg, len);

    // add crc to message for integrity
    uint32_t crc = esp_crc32_le(0, (const uint8_t*)&self.tx_buffer, total_len); 
    self.tx_buffer.header.checksum = crc;


    if (sendto(self.spi_socket, &self.tx_buffer, total_len, 0, (const struct sockaddr *)&self.spi_next_hop, sizeof(self.spi_next_hop)) < 0)
    {
        ESP_LOGE(TAG, "sendto failed: errno=%d", errno);
        return false;
    }

    // Wait for up to 5 secs for a response to come back
    int _ = 0;
    return xQueueReceive(self.msg_back_queue, &_, BROADCAST_TIMEOUT_MS / portTICK_PERIOD_MS) == pdTRUE;
}