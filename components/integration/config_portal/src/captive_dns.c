#include "captive_dns.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"

static int dns_socket = -1;
static TaskHandle_t dns_task;
static SemaphoreHandle_t dns_stopped;

/* Accept one uncompressed question. Unsupported types get an empty answer. */
static size_t dns_reply(uint8_t *packet, size_t length, size_t capacity)
{
    if (length < 12 || (packet[2] & 0xf8) != 0 ||
        packet[4] != 0 || packet[5] != 1) {
        return 0;
    }
    size_t offset = 12;
    for (;;) {
        if (offset >= length) return 0;
        uint8_t label = packet[offset++];
        if (label == 0) break;
        if (label > 63 || offset + label > length) return 0;
        offset += label;
        if (offset - 12 > 254) return 0;
    }
    if (offset + 4 > length) return 0;
    bool answer = packet[offset] == 0 && packet[offset + 1] == 1 &&
                  packet[offset + 2] == 0 && packet[offset + 3] == 1;
    offset += 4;
    if (answer && offset + 16 > capacity) return 0;
    packet[2] = 0x80 | (packet[2] & 1);
    packet[3] = 0;
    memset(packet + 6, 0, 6);
    if (answer) {
        static const uint8_t record[] = {
            0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 0, 30, 0, 4,
            192, 168, 4, 1,
        };
        packet[7] = 1;
        memcpy(packet + offset, record, sizeof(record));
        offset += sizeof(record);
    }
    return offset;
}

static void dns_worker(void *argument)
{
    (void)argument;
    uint8_t packet[512];
    while (ulTaskNotifyTake(pdTRUE, 0) == 0) {
        struct sockaddr_in peer;
        socklen_t peer_length = sizeof(peer);
        int length = recvfrom(dns_socket, packet, sizeof(packet), 0,
                              (struct sockaddr *)&peer, &peer_length);
        if (length <= 0) continue;
        size_t reply_length = dns_reply(packet, (size_t)length, sizeof(packet));
        if (reply_length != 0) {
            sendto(dns_socket, packet, reply_length, 0,
                   (struct sockaddr *)&peer, peer_length);
        }
    }
    close(dns_socket);
    xSemaphoreGive(dns_stopped);
    vTaskDelete(NULL);
}

esp_err_t captive_dns_start(void)
{
    if (dns_task != NULL) return ESP_OK;
    dns_stopped = xSemaphoreCreateBinary();
    if (dns_stopped == NULL) return ESP_ERR_NO_MEM;
    dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = inet_addr("192.168.4.1"),
    };
    struct timeval timeout = {.tv_sec = 1};
    if (dns_socket < 0 ||
        setsockopt(dns_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        bind(dns_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        if (dns_socket >= 0) close(dns_socket);
        dns_socket = -1;
        vSemaphoreDelete(dns_stopped);
        dns_stopped = NULL;
        return ESP_FAIL;
    }
    if (xTaskCreate(dns_worker, "portal_dns", 3072, NULL, 4, &dns_task) != pdPASS) {
        close(dns_socket);
        dns_socket = -1;
        vSemaphoreDelete(dns_stopped);
        dns_stopped = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void captive_dns_stop(void)
{
    if (dns_task == NULL) return;
    xTaskNotifyGive(dns_task);
    xSemaphoreTake(dns_stopped, portMAX_DELAY);
    dns_task = NULL;
    dns_socket = -1;
    vSemaphoreDelete(dns_stopped);
    dns_stopped = NULL;
}
