#include "client.h"

#define SERVER_PORT 3999
#define BUFFER_SIZE 512
#define RETRY_DELAY_MS 5000

static const char *LOGGING_TAG = "tcp_client";
static int server_sock = -1;
static bool sta_is_up = false;

static bool get_gateway_ip(char *ip_str, size_t ip_str_len) {
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (!netif || esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        ESP_LOGE(LOGGING_TAG, "Failed to get gateway IP");
        return false;
    }

    inet_ntoa_r(ip_info.gw, ip_str, ip_str_len);
    return true;
}

static void socket_read_loop(const int sock, const char *server_ip) {
    uint8_t rx_buffer[BUFFER_SIZE];
    server_sock = sock;
    // on_peer_connected();

    while (1) {
        int len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
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

    // on_peer_lost();
    server_sock = -1;
}

static void tcp_client_task(void *pvParameters) {
    (void) pvParameters;  // unused

    struct sockaddr_in server_addr;
    char gateway_ip[INET_ADDRSTRLEN];

    while (sta_is_up) {

        if (!get_gateway_ip(gateway_ip, sizeof(gateway_ip))) {
            ESP_LOGE(LOGGING_TAG, "Failed to get gateway IP, retrying...");
            vTaskDelay(RETRY_DELAY_MS / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(LOGGING_TAG, "Connecting to gateway at %s:%d...", gateway_ip, SERVER_PORT);

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(LOGGING_TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(RETRY_DELAY_MS / portTICK_PERIOD_MS);
            continue;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, gateway_ip, &server_addr.sin_addr);

        int err = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (err != 0) {
            ESP_LOGE(LOGGING_TAG, "Socket connect failed: errno %d", errno);
            close(sock);
            vTaskDelay(RETRY_DELAY_MS / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(LOGGING_TAG, "Connected to %s", gateway_ip);
        
        // Handle incoming data
        socket_read_loop(server_sock, gateway_ip);

        // Cleanup once connection has been closed
        ESP_LOGW(LOGGING_TAG, "Connection to %s lost. Reconnecting...", gateway_ip);
        if(sock >= 0){
        shutdown(sock, 0);
        close(sock);
        }

        // Retry connection
        vTaskDelay(RETRY_DELAY_MS / portTICK_PERIOD_MS);
    }

    // Stop task if STA is not connected
    ESP_LOGW(LOGGING_TAG, "STA not connected, stopping task...");
    vTaskDelete(NULL);
}

void client_open() {
    if (!sta_is_up) {
        sta_is_up = true;
        xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
        ESP_LOGI(LOGGING_TAG, "Client started");
    } else {
        ESP_LOGW(LOGGING_TAG, "Client is already running");
    }
}

void client_close() {
    if (sta_is_up) {
        ESP_LOGW(LOGGING_TAG, "Closing client...");

        sta_is_up = false;

        if (server_sock >= 0) {
            shutdown(server_sock, 0);
            close(server_sock);
            server_sock = -1;
        }

    } else {
        ESP_LOGW(LOGGING_TAG, "Client is not running");
    }
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
