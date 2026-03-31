#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "task_config.h"
#include "remote_control.h"
#include "node.h"

#define PORT 3999
#define BUFFER_SIZE 512

#define AUTH_PASSWORD "I4A123456"
#define PASSWORD_LEN 9

typedef enum remote_commands {
    AP_ENABLE = 0,
    AP_DISABLE,
    REMOTE_COMMAND_NUMBER
} remote_commands_t;

static const char *command_table[REMOTE_COMMAND_NUMBER] = {
    [AP_ENABLE]  = "ap_enable",
    [AP_DISABLE] = "ap_disable"
};

static const char *LOGGING_TAG = "remote_control_server";

static int listen_sock = -1;
static bool server_is_up = false;

// Get the AP's IP address
static bool get_ap_ip_info(esp_ip4_addr_t *ap_ip) {
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    if (!netif || esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        ESP_LOGE(LOGGING_TAG, "Failed to get network info");
        return false;
    }

    *ap_ip = ip_info.ip;
    return true;
}

static int determine_command(const char *input) {
    // Check minimum length
    if (strlen(input) < PASSWORD_LEN + 2) {
        return -1;
    }

    // Check ':' separator
    if (input[PASSWORD_LEN] != ':') {
        return -1;
    }

    // Check password
    if (strncmp(input, AUTH_PASSWORD, PASSWORD_LEN) != 0) {
        return -1;
    }

    // Extract command string
    const char *cmd_str = input + PASSWORD_LEN + 1;

    // Iterate table
    for (int i = 0; i < REMOTE_COMMAND_NUMBER; i++) {
        if (command_table[i] && strcmp(cmd_str, command_table[i]) == 0) {
            return i;
        }
    }

    // Return error if couldn't find a match
    return -1;
}

static void apply_command(remote_commands_t command) {
    if (command == AP_DISABLE) {
        ESP_LOGI(LOGGING_TAG, "Disabling AP (deauth STAs)...");
        node_disable_ap();
    }

    if (command == AP_ENABLE) {
        ESP_LOGI(LOGGING_TAG, "Enabling AP (allow new connections)...");
        node_enable_ap();
    }

}

// Initialize the TCP server
static void remote_command_server_init() {
    struct sockaddr_in dest_addr;
    esp_ip4_addr_t ap_ip;

    if (!get_ap_ip_info(&ap_ip)) {
        ESP_LOGE(LOGGING_TAG, "Failed to get AP IP address");
        return;
    }

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to create TCP socket: errno %d", errno);
        return;
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = ap_ip.addr;

    if (bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to bind TCP socket: errno %d", errno);
        close(listen_sock);
        return;
    }

    if (listen(listen_sock, 1) < 0) {
        ESP_LOGE(LOGGING_TAG, "Unable to listen on TCP socket: errno %d", errno);
        close(listen_sock);
        return;
    }

    ESP_LOGI(LOGGING_TAG, "Server bound to AP IP: %s", inet_ntoa(ap_ip));
}

// Handle incoming TCP commands
static void remote_command_server_task(void *pvParameters) {
    uint8_t rx_buffer[BUFFER_SIZE];
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);
    char addr_str[128];

    while (1) {
        ESP_LOGI(LOGGING_TAG, "Waiting for a new connection...");
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(LOGGING_TAG, "Unable to accept connection: errno %d", errno);
            continue;
        }

        inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(LOGGING_TAG, "Accepted connection from %s", addr_str);

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(LOGGING_TAG, "Error receiving TCP packet: errno %d", errno);
            shutdown(sock, 0);
            close(sock);
            continue;
        }

        rx_buffer[len] = 0;  // Null-terminate

        
        // Determine command and response
        int command = determine_command((const char *)rx_buffer);
        char response[128];
        
        if(command != -1) {
            snprintf(response, sizeof(response), "Applying command: %s", command_table[command]);
        } else {
            snprintf(response, sizeof(response),
                     "Error: invalid command or password. Check formatting.");
        }

        // Send response
        send(sock, response, strlen(response), 0);

        // Close the connection
        shutdown(sock, 0);
        close(sock);
        ESP_LOGI(LOGGING_TAG, "Connection from %s closed", addr_str);

        // Apply AP change after socket closed
        if(command != -1) {
            apply_command(command);
        }
    }
}

// Start the server
void remote_command_server_create() {
    if (!server_is_up) {
        server_is_up = true;
        remote_command_server_init();
        xTaskCreatePinnedToCore(remote_command_server_task, "remote_control_task",
                                TASK_REMOTE_CONTROL_STACK, NULL, TASK_REMOTE_CONTROL_PRIORITY,
                                NULL, TASK_REMOTE_CONTROL_CORE);
        ESP_LOGI(LOGGING_TAG, "Remote Command Server started");
    } else {
        ESP_LOGW(LOGGING_TAG, "Server is already running");
    }
}

// Stop the server
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
