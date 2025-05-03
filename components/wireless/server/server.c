#include "server.h"

#define PORT 3999
#define BUFFER_SIZE 512

static const char *LOGGING_TAG = "udp_server";

static void udp_server_task(void *pvParameters) {
  uint8_t rx_buffer[BUFFER_SIZE];
  char addr_str[128];
  struct sockaddr_storage source_addr;
  socklen_t socklen = sizeof(source_addr);

  int addr_family = AF_INET;
  int ip_protocol = IPPROTO_IP;

  struct sockaddr_in dest_addr;
  dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(PORT);

  int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
  if (sock < 0) {
    ESP_LOGE(LOGGING_TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(LOGGING_TAG, "Socket created");

  int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0) {
    ESP_LOGE(LOGGING_TAG, "Socket unable to bind: errno %d", errno);
    close(sock);
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(LOGGING_TAG, "Socket bound, port %d", PORT);

  while (1) {
    ESP_LOGI(LOGGING_TAG, "Waiting for data...");
    int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0,
                       (struct sockaddr *)&source_addr, &socklen);

    if (len < 0) {
      ESP_LOGE(LOGGING_TAG, "recvfrom failed: errno %d", errno);
      break;
    } else {
      // Convert address to readable form
      if (source_addr.ss_family == AF_INET) {
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr,
                    addr_str, sizeof(addr_str) - 1);
      } else {
        strcpy(addr_str, "Unknown AF");
      }

      ESP_LOGI(LOGGING_TAG, "Received %d bytes from %s", len, addr_str);

      // call on_peer_message(rx_buffer, len);
    }
  }

  close(sock);
  vTaskDelete(NULL);
}

void create_server() {
  xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}


void create_server() {
  xTaskCreate(tcp_server_task, "tcp_server", 4096, (void *)AF_INET, 5, NULL);
}
