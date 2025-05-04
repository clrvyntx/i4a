#include "client.h"

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SERVER_PORT 3999
#define BUFFER_SIZE 512

static const char *LOGGING_TAG = "tcp_client";

static int client_sock = -1;

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

    client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client_sock < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    ESP_LOGI(LOGGING_TAG, "Connecting to %s:%d...", server_ip, SERVER_PORT);

    int err = connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err != 0) {
        ESP_LOGE(LOGGING_TAG, "Socket unable to connect: errno %d", errno);
        close(client_sock);
        client_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(LOGGING_TAG, "Successfully connected to %s", server_ip);
    // call on_peer_connected()
    
    // Handle incoming data
    socket_read_loop(client_sock, server_ip);

    // Cleanup once connection has been closed
    ESP_LOGI(LOGGING_TAG, "Closing connection from %s", server_ip);
    shutdown(client_sock, 0);
    close(client_sock);
    client_sock = -1;
    // call on_peer_lost();
    vTaskDelete(NULL);
}

void client_connect(const char *ip) {
    xTaskCreate(tcp_client_task, "tcp_client", 4096, (void *)ip, 5, NULL);
}

bool client_send_message(const uint8_t *msg, uint16_t len) {
    if (client_sock < 0) {
        ESP_LOGW(LOGGING_TAG, "Client not connected");
        return false;
    }

    if (!msg || len == 0) {
        ESP_LOGE(LOGGING_TAG, "Invalid message: msg is NULL or length is 0");
        return false;
    }

    int sent = send(client_sock, msg, len, 0);
    if (sent == len) {
        ESP_LOGI(LOGGING_TAG, "Sent %d bytes to server", sent);
        return true;
    } else {
        ESP_LOGE(LOGGING_TAG, "Failed to send message: errno %d", errno);
        return false;
    }
}
