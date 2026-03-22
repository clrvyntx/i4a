#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "task_config.h"
#include "remote_control.h"

#define PORT 3999
#define BUFFER_SIZE 512

static const char *LOGGING_TAG = "remote_control_server";

static int listen_sock = -1;
static bool server_is_up = false;

// Function to get the AP's IP address
static bool get_ap_ip_info(esp_ip4_addr_t *ap_ip) {
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    if (!netif || esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        ESP_LOGE(LOGGING_TAG, "Failed to get network info");
        return false;
    }

    *ap_ip = ip_info.ip;  // Store the AP's IP address
    return true;
}

// Function to execute a remote command (this is where the command is processed)
static void execute_remote_command(const char *command) {
    ESP_LOGI(LOGGING_TAG, "Executing command: %s", command);
    // Add your logic here to handle different commands
    // For example:
    // if (strcmp(command, "start") == 0) { start_task(); }
    // else if (strcmp(command, "stop") == 0) { stop_task(); }
}

// Function to initialize the TCP server
static void remote_command_server_init() {
    struct sockaddr_in dest_addr;
    esp_ip4_addr_t ap_ip;

    // Get the AP IP address
    if (!get_ap_ip_info(&ap_ip)) {
        ESP_LOGE(LOGGING_TAG, "Failed to get AP IP address");
        return;
    }

    // Create a TCP socket
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to create TCP socket: errno %d", errno);
        return;
    }

    // Setup the address to bind to the AP's IP
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = ap_ip.addr;  // Use the AP's IP

    // Bind the socket to the AP's IP address
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to bind TCP socket: errno %d", errno);
        close(listen_sock);
        return;
    }

    // Start listening on the socket
    err = listen(listen_sock, 1);
    if (err < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to listen on TCP socket: errno %d", errno);
        close(listen_sock);
        return;
    }

    ESP_LOGI(LOGGING_TAG, "Server bound to AP IP: %s", inet_ntoa(ap_ip));
}

// Function to handle incoming TCP messages (remote commands)
static void remote_command_server_task(void *pvParameters) {
    uint8_t rx_buffer[BUFFER_SIZE];
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);
    char addr_str[128];

    while (1) {
        // Wait for a connection
        ESP_LOGI(LOGGING_TAG, "Waiting for a new connection...");
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(LOGGING_TAG, "Unable to accept connection: errno %d", errno);
            continue;  // Wait for next connection attempt
        }

        // Convert the client's IP to a string
        inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(LOGGING_TAG, "Accepted connection from %s", addr_str);

        while (1) {
            // Receive a TCP packet
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0) {
                ESP_LOGE(LOGGING_TAG, "Error receiving TCP packet: errno %d", errno);
                break;  // Close connection if error occurs
            }

            // Null-terminate the received data
            rx_buffer[len] = 0;

            // Log the received command
            ESP_LOGI(LOGGING_TAG, "Received TCP packet from %s: %s", addr_str, rx_buffer);

            // Execute the received command
            execute_remote_command((const char *)rx_buffer);

            // Optionally, send a response back to the client
            const char *response = "Message received";
            send(sock, response, strlen(response), 0);
        }

        // Cleanup once connection is closed
        ESP_LOGI(LOGGING_TAG, "Closing connection from %s", addr_str);
        shutdown(sock, 0);
        close(sock);
    }
}

// Function to start the remote command server
void remote_command_server_create() {
    if (!server_is_up) {  // Prevent starting the server if it's already running
        server_is_up = true;
        remote_command_server_init();  // Initialize the TCP server socket
        xTaskCreatePinnedToCore(remote_command_server_task, "remote_control_task",
                                TASK_REMOTE_CONTROL_STACK, NULL, TASK_REMOTE_CONTROL_PRIORITY,
                                NULL, TASK_REMOTE_CONTROL_CORE);
        ESP_LOGI(LOGGING_TAG, "Remote Command Server started");
    } else {
        ESP_LOGW(LOGGING_TAG, "Server is already running");
    }
}

// Function to close the remote command server
void remote_command_server_close() {
    if (server_is_up) {
        server_is_up = false;
        if (listen_sock >= 0) {
            shutdown(listen_sock, SHUT_RDWR);
            close(listen_sock);
        }
        ESP_LOGI(LOGGING_TAG, "Remote Command Server stopped");
    } else {
        ESP_LOGW(LOGGING_TAG, "Server is not running, cannot close it.");
    }
}
