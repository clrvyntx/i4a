#include "driver/uart.h"
#include "esp_log.h"

void setup_uart();
uint8_t uart_execute();
uint8_t uart_query(uint8_t cmd);
uint8_t uart_do_long_poll();