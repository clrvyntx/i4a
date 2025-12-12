#include "pysim.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "protocol.h"

#define PS_UART_PORT UART_NUM_1
#define PS_MAX_PAYLOAD_SIZE ((1600 * 2))

#define TAG "pysim"

static struct {
    bool initialized;

    StaticSemaphore_t _st_read_lock, _st_write_lock;
    SemaphoreHandle_t read_lock, write_lock;
    ps_event_callback_t event_callbacks[CONFIG_PYSIM_MAX_EVENTS];
} self = { 0 };

static void uart_polling_task();

void pysim_start() {
    if (self.initialized) {
        ESP_LOGW(TAG, "Trying to reinitialize HAL -- skipping.");
        return;
    }

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

    xTaskCreatePinnedToCore(uart_polling_task, "uart_polling_task", 4096, NULL, 10, NULL, 1);
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

uint8_t ps_execute(uint8_t command, const void* args, size_t sz_args, void* resp, size_t *sz_resp) {
    if (sz_args > 0xFFFFFF) {
        ESP_LOGE(TAG, "Maximum payload size is 0xFFFFFF");
        return 0xFE;
    }

    uint32_t payload = (command << 24) | sz_args;

    uart_write_lock(); // Locks: write
    
    write_all(&payload, sizeof(uint32_t));
    if (sz_args > 0) {
        write_all(args, sz_args);
    }

    uart_read_lock(); // Locks: write, read
    
    uint32_t result = 0;
    read_exact(&result, sizeof(uint32_t));

    uint8_t ret = (result >> 24);
    uint32_t sz = (result & 0xFFFFFF);
    uint32_t buffer_size = sz_resp ? *sz_resp : 0;

    if (sz > buffer_size) {
        ESP_LOGE(TAG, "Simulator returned bigger payload (%zu) than buffer (%zu) -- aborting", sz, buffer_size);
        abort();
    }

    if (sz > 0) {
        read_exact(resp, sz);
    }
    if (sz_resp) {
        *sz_resp = sz;
    }

    uart_read_unlock(); // Locks: write
    uart_write_unlock(); // Locks: -
    return ret;
}


uint8_t ps_query(uint8_t cmd) {
    uint8_t ret = ps_execute(
        cmd,
        NULL,
        0,
        NULL,
        NULL
    );

    if (ret & 0x80) {
        ESP_LOGE(TAG, "query(%u) failed: %u", cmd, ret);
    }

    return ret;
}

void ps_register_event(uint8_t event_id, ps_event_callback_t callback) {
    if (event_id > CONFIG_PYSIM_MAX_EVENTS) {
        ESP_LOGE(
            TAG, 
            "Trying to register handler for event ID=%u but max number of allowed events is %u -- check CONFIG_PYSIM_MAX_EVENTS", 
            event_id,
            CONFIG_PYSIM_MAX_EVENTS
        );
        esp_system_abort("ps_register_event with invalid event id");
    } else {
        self.event_callbacks[event_id] = callback;
    }
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

static void uart_polling_task() {
    static uint8_t event_buffer[1600];
    size_t event_buffer_sz = 1600;

    while (1) {
        uint8_t ret = uart_do_long_poll();
        if (ret == 0) {
            // Nothing happened - wake up from another cmd
            continue;
        }

        // There are pending events
        event_buffer_sz = sizeof(event_buffer);
        ret = ps_execute(0xF5, NULL, 0, event_buffer, &event_buffer_sz);

        if (PS_IS_ERROR(ret)) {
            ESP_LOGE(TAG, "PySIM failed to retrieve event!! err=%u", ret);
            esp_system_abort("PySIM failed to retrieve an event");
        } else {
            if (ret > CONFIG_PYSIM_MAX_EVENTS) {
                ESP_LOGE(
                    TAG, 
                    "Got event ID=%u but max number of allowed events is %u -- check CONFIG_PYSIM_MAX_EVENTS", 
                    ret,
                    CONFIG_PYSIM_MAX_EVENTS
                );
                continue;
            }

            if (self.event_callbacks[ret]) {
                self.event_callbacks[ret](ret, event_buffer, event_buffer_sz);
            } else {
                ESP_LOGW(TAG, "Got event ID=%u but no handler registered");
            }
        }
        
        
    }

    vTaskDelete(NULL);
}