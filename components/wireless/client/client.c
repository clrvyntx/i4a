#include "client.h"

#define PORT 3999
#define BUFFER_SIZE 512

static const char *LOGGING_TAG = "udp_client";

// Global variable for the socket
static int sock = -1;

// Function to send a message to the server
bool send_message(const char *ip, const uint8_t *msg, uint16_t len) {
    if (sock < 0) {
        ESP_LOGE(LOGGING_TAG, "Client socket not created yet");
        return false;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = inet_addr(ip);  // Set the server's IP here

    int err = sendto(sock, msg, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    if (err < 0) {
        ESP_LOGE(LOGGING_TAG, "Send failed: errno %d", errno);
        return false;
    } else {
        ESP_LOGI(LOGGING_TAG, "Sent %d bytes to %s", len, ip);
        return true;
    }
}

// Function to create the client task and initialize the client
void create_client(void) {
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to create socket: errno %d", errno);
        return;
    }

    ESP_LOGI(LOGGING_TAG, "Socket created");
}
