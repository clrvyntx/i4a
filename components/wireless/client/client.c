#include "client.h"

#define SERVER_PORT 3999
#define BUFFER_SIZE 512
#define RETRY_DELAY_MS 5000

static const char *LOGGING_TAG = "tcp_client";

static int server_sock = -1;

static void socket_read_loop(const int sock, const char *server_ip) {
    uint8_t rx_buffer[BUFFER_SIZE];

    while (1) {
        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(LOGGING_TAG, "Receive error from %s: errno %d", server_ip, errno);
            break;
        } else if (len == 0) {
            ESP_LOGW(LOGGING_TAG, "Server %s closed the connection", server_ip);
            break;
        } else {
            ESP_LOGI(LOGGING_TAG, "Received %d bytes from %s", len, server_ip);
            // call on_peer_message(rx_buffer, len);
        }
    }
}

static void tcp_client_task(void *pvParameters) {
    const char *server_ip = (const char *)pvParameters;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    ESP_LOGI(LOGGING_TAG, "Connecting to %s:%d...", server_ip, SERVER_PORT);

    while (1) {
        int err = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (err != 0) {
            ESP_LOGE(LOGGING_TAG, "Socket unable to connect: errno %d, retrying in 5 seconds...", errno);
            close(sock);
            vTaskDelay(RETRY_DELAY_MS / portTICK_PERIOD_MS);  // Wait before retrying
            continue;  // Retry connection
        } else {
            break; // Successfully connected
        }
    }

    ESP_LOGI(LOGGING_TAG, "Successfully connected to %s", server_ip);
    server_sock = sock;
    // call on_peer_connected()

    // Handle incoming data
    socket_read_loop(server_sock, server_ip);

    // Cleanup once connection has been closed
    ESP_LOGI(LOGGING_TAG, "Closing connection from %s", server_ip);
    server_sock = -1;
    // call on_peer_lost();
    shutdown(sock, 0);
    close(sock);

}

void client_connect(const char *ip) {
    xTaskCreate(tcp_client_task, "tcp_client", 4096, (void *)ip, 5, NULL);
}

bool client_send_message(const uint8_t *msg, uint16_t len) {
    if (server_sock < 0) {
        ESP_LOGW(LOGGING_TAG, "Not connected to server");
        return false;
    }

    if (!msg || len == 0) {
        ESP_LOGE(LOGGING_TAG, "Invalid message: msg is NULL or length is 0");
        return false;
    }

    int sent = send(server_sock, msg, len, 0);
    if (sent == len) {
        ESP_LOGI(LOGGING_TAG, "Sent %d bytes to server", sent);
        return true;
    } else {
        ESP_LOGE(LOGGING_TAG, "Failed to send message: errno %d", errno);
        return false;
    }
}
