#include "client.h"

#define PORT 3999

static const char *LOGGING_TAG = "tcp_client";

// Struct to pass data to the task
typedef struct {
    const char *ip;
    const uint8_t *msg;
    uint16_t len;
} tcp_task_params_t;

// Function to handle the TCP send logic in a FreeRTOS task
void tcp_send_task(void *pvParameters) {
    tcp_task_params_t *params = (tcp_task_params_t *)pvParameters;

    int sock = socket(AF_INET, SOCK_STREAM, 0);  // Create socket
    if (sock < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);  // Clean up task if socket creation fails
        return;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = inet_addr(params->ip);  // Set the server's IP

    // Connect to the server (create a TCP connection)
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to connect to server: errno %d", errno);
        close(sock);  // Clean up socket if connection fails
        vTaskDelete(NULL);  // Clean up task
        return;
    }

    ESP_LOGI(LOGGING_TAG, "Connected to server: %s", params->ip);

    // Send the message to the server using TCP
    int sent_bytes = send(sock, params->msg, params->len, 0);
    if (sent_bytes < 0) {
        ESP_LOGE(LOGGING_TAG, "Send failed: errno %d", errno);
        close(sock);  // Close the socket if sending failed
        vTaskDelete(NULL);  // Clean up task
        return;
    } else {
        ESP_LOGI(LOGGING_TAG, "Sent %d bytes to %s", sent_bytes, params->ip);
    }

    // Close the socket after sending the message (no persistent connection)
    close(sock);
    vTaskDelete(NULL);  // End the task after completion
}

// Wrapper function to start the TCP send task
bool send_message(const char *ip, const uint8_t *msg, uint16_t len) {
    // Allocate memory for task parameters
    tcp_task_params_t *params = (tcp_task_params_t *)malloc(sizeof(tcp_task_params_t));
    if (!params) {
        ESP_LOGE(LOGGING_TAG, "Failed to allocate memory for task parameters");
        return false;
    }

    // Initialize task parameters
    params->ip = ip;
    params->msg = msg;
    params->len = len;

    // Create a FreeRTOS task to handle the TCP send
    xTaskCreate(tcp_send_task, "TCP Send Task", 4096, params, 5, NULL);

    return true;
}
