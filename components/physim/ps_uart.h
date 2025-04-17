#ifndef _PS_UART_H_
#define _PS_UART_H_

#include <stdint.h>

#include "driver/uart.h"
#include "esp_log.h"

#define PS_UART_PORT UART_NUM_1
#define PS_MAX_PAYLOAD_SIZE ((1600 * 2))

static inline void setup_uart()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_driver_install(PS_UART_PORT, 1600 * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(PS_UART_PORT, &uart_config));
}

static inline void read_exact(void *buffer, uint32_t len)
{
    if (uart_read_bytes(PS_UART_PORT, buffer, len, portMAX_DELAY) != len)
    {
        ESP_LOGE("ps_uart", "Received incomplete command from controller -- aborting");
        abort();
    }
}

static inline void write_all(const void *buffer, uint32_t len)
{
    if (uart_write_bytes(PS_UART_PORT, buffer, len) != len)
    {
        ESP_LOGE("ps_uart", "Wrote incomplete command to controller -- aborting");
        abort();
    }
}

#endif // _PS_UART_H_