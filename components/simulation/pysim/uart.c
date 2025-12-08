#include "driver/uart.h"
#include "esp_log.h"

#include "protocol.h"

#define TAG "pysim_uart"
#define PS_UART_PORT UART_NUM_1
#define PS_MAX_PAYLOAD_SIZE ((1600 * 2))

static struct {
    StaticSemaphore_t _st_read_lock, _st_write_lock;
    SemaphoreHandle_t read_lock, write_lock;
} self;

void setup_uart()
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
    self.read_lock = xSemaphoreCreateMutexStatic(&self._st_read_lock);
    self.write_lock = xSemaphoreCreateMutexStatic(&self._st_write_lock);
}

static void read_exact(void *buffer, uint32_t len)
{
    if (uart_read_bytes(PS_UART_PORT, buffer, len, portMAX_DELAY) != len)
    {
        ESP_LOGE(TAG, "Received incomplete command from controller -- aborting");
        abort();
    }
}

static void write_all(const void *buffer, uint32_t len)
{
    if (uart_write_bytes(PS_UART_PORT, buffer, len) != len)
    {
        ESP_LOGE(TAG, "Wrote incomplete command to controller -- aborting");
        abort();
    }
}


static void uart_write_lock() {
    while (!xSemaphoreTake(self.write_lock, portMAX_DELAY));
}

static void uart_write_unlock() {
    xSemaphoreGive(self.write_lock);
}

static void uart_read_lock() {
    while (!xSemaphoreTake(self.read_lock, portMAX_DELAY));
}

static void uart_read_unlock() {
    xSemaphoreGive(self.read_lock);
}

uint8_t uart_execute(
    uint8_t command, 
    size_t args_size,
    const void* args,
    size_t *resp_size,
    void* resp
) {
    if (args_size > 0xFFFFFF) {
        ESP_LOGE(TAG, "Maximum payload size is 0xFFFFFF");
        return 0xFE;
    }

    uint32_t payload = (command << 24) | args_size;

    uart_write_lock(); // Locks: write
    
    write_all(&payload, sizeof(uint32_t));
    if (args_size > 0) {
        write_all(args, args_size);
    }

    uart_read_lock(); // Locks: write, read
    
    uint32_t result = 0;
    read_exact(&result, sizeof(uint32_t));

    uint8_t ret = (result >> 24);
    uint32_t sz = (result & 0xFFFFFF);
    uint32_t buffer_size = resp_size ? *resp_size : 0;

    if (sz > buffer_size) {
        ESP_LOGE(TAG, "Simulator returned bigger payload than buffer -- aborting");
        abort();
    }

    if (sz > 0) {
        read_exact(resp, sz);
    }
    if (resp_size) {
        *resp_size = sz;
    }

    uart_read_unlock(); // Locks: write
    uart_write_unlock(); // Locks: -
    return ret;
}


uint8_t uart_query(uint8_t cmd) {
    uint8_t ret = uart_execute(
        cmd,
        0,
        NULL,
        NULL,
        NULL
    );

    if (ret & 0x80) {
        ESP_LOGE(TAG, "query(%u) failed: %u", cmd, ret);
    }

    return ret;
}



uint8_t uart_do_long_poll() {
    uart_write_lock(); // Locks: -
    uart_read_lock();  // Locks: write

    // Enter long polling
    uint32_t cmd = PS_PACK_CMD(PS_CMD_LONG_POLL, 0);
    write_all(&cmd, sizeof(uint32_t));  // Locks: write, read
    
    uart_write_unlock();    // Release write lock

    uint32_t result = 0;
    read_exact(&result, sizeof(uint32_t));
    uart_read_unlock();  // Got data, release read lock

    if (PS_RESPONSE_LEN(result) != 0) {
        ESP_LOGE(TAG, "long poll returned data -- aborting");
        abort();
    }

    return PS_RESPONSE_STATUS(result);
}