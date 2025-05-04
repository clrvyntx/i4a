#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void client_connect(const char *ip);
void client_disconnect();
bool client_send_message(const uint8_t *msg, uint16_t len);
