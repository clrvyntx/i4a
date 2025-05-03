#include "client.h"

#define PORT 3999

static const char *LOGGING_TAG = "tcp_client";

// Function to handle the TCP send logic
bool send_message(const char *ip, const uint8_t *msg, uint16_t len) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);  // Create socket
    if (sock < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to create socket: errno %d", errno);
        return false;  // Return false if socket creation fails
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = inet_addr(ip);  // Set the server's IP

    // Connect to the server (create a TCP connection)
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to connect to server: errno %d", errno);
        close(sock);  // Clean up socket if connection fails
        return false;  // Return false if connection fails
    }

    ESP_LOGI(LOGGING_TAG, "Connected to server: %s", ip);

    // Send the message to the server using TCP
    int sent_bytes = send(sock, msg, len, 0);
    if (sent_bytes < 0) {
        ESP_LOGE(LOGGING_TAG, "Send failed: errno %d", errno);
        close(sock);  // Close the socket if sending failed
        return false;  // Return false if send fails
    } else {
        ESP_LOGI(LOGGING_TAG, "Sent %d bytes to %s", sent_bytes, ip);
    }

    // Close the socket after sending the message (no persistent connection)
    close(sock);
    return true;  // Return true if the message was sent successfully
}
