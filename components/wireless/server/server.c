#include "server.h"

#define PORT 3999
#define KEEPALIVE_IDLE 5
#define KEEPALIVE_INTERVAL 5
#define KEEPALIVE_COUNT 3
#define BUFFER_SIZE 512

static const char *LOGGING_TAG = "tcp_server";

static void socket_read_loop(const int sock, const char *client_ip) {
  uint8_t rx_buffer[BUFFER_SIZE];

  while (1) {
    int len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
    if (len < 0) {
      ESP_LOGE(LOGGING_TAG, "Receive error from %s: errno %d", client_ip, errno);
      break;
    } else if (len == 0) {
      ESP_LOGW(LOGGING_TAG, "Client %s disconnected gracefully", client_ip);
      break;
    } else {
      ESP_LOGI(LOGGING_TAG, "Received %d bytes from %s", len, client_ip);
      // call on_peer_message(rx_buffer, len);
    }
  }
}

static void tcp_server_task(void *pvParameters) {
  char addr_str[128];
  int addr_family = AF_INET;
  int ip_protocol = IPPROTO_IP;
  struct sockaddr_in dest_addr;

  dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(PORT);

  int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
  if (listen_sock < 0) {
    ESP_LOGE(LOGGING_TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }

  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  ESP_LOGI(LOGGING_TAG, "Socket created");

  int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err != 0) {
    ESP_LOGE(LOGGING_TAG, "Socket unable to bind: errno %d", errno);
    goto CLEAN_UP;
  }

  ESP_LOGI(LOGGING_TAG, "Socket bound, port %d", PORT);

  err = listen(listen_sock, 1);
  if (err != 0) {
    ESP_LOGE(LOGGING_TAG, "Error occurred during listen: errno %d", errno);
    goto CLEAN_UP;
  }

  while (1) {
    ESP_LOGI(LOGGING_TAG, "Waiting for a new connection...");

    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
      ESP_LOGE(LOGGING_TAG, "Unable to accept connection: errno %d", errno);
      continue;
    }

    // Get client IP
    if (source_addr.ss_family == AF_INET) {
      inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr,
                  addr_str, sizeof(addr_str) - 1);
    }

    ESP_LOGI(LOGGING_TAG, "Accepted connection from %s", addr_str);
    // call on_peer_connected()

    // Enable TCP Keep-Alive
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

    // Handle incoming data
    socket_read_loop(sock, addr_str);

    // Cleanup once connection has been closed
    ESP_LOGI(LOGGING_TAG, "Closing connection from %s", addr_str);
    // call on_peer_lost();
    shutdown(sock, 0);
    close(sock);
  }

  CLEAN_UP:
  close(listen_sock);
  vTaskDelete(NULL);
}

void create_server() {
  xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}
