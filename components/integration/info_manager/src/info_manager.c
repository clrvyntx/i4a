#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "node.h"
#include "info_manager.h"

#define UDP_SERVER_IP   "10.255.255.254"
#define UDP_SERVER_PORT 8000

typedef struct __attribute__((packed)) {
    uint8_t orientation;
    uint8_t channel;
    int8_t rssi;

    uint32_t subnet;
    uint32_t mask;

    uint32_t rx_bytes;
    uint32_t tx_bytes;

    char uuid[12];
    char link[12];
} udp_packet_t;

static void im_client_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(5000));

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        return;
    }

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_SERVER_PORT),
        .sin_addr.s_addr = inet_addr(UDP_SERVER_IP),
    };

    while (1) {

        const im_ring_packet_t *ring = im_get_ring_info();

        udp_packet_t packets[MAX_ORIENTATIONS];

        for (int i = 0; i < MAX_ORIENTATIONS; i++) {

            packets[i].orientation = ring[i].orientation;
            packets[i].channel     = ring[i].channel;
            packets[i].rssi        = ring[i].rssi;

            packets[i].subnet      = ring[i].subnet;
            packets[i].mask        = ring[i].mask;

            packets[i].rx_bytes    = ring[i].rx_bytes;
            packets[i].tx_bytes    = ring[i].tx_bytes;

            snprintf(packets[i].uuid, sizeof(packets[i].uuid), "%s",
                     node_get_device_mac());

            snprintf(packets[i].link, sizeof(packets[i].link), "%s",
                     node_get_link_name());
        }

        sendto(sock,
               packets,
               sizeof(packets),
               0,
               (struct sockaddr *)&dest,
               sizeof(dest));

        vTaskDelay(pdMS_TO_TICKS(60000)); // 1 min
    }
}
