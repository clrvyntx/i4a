#include "os/os.h"

bool mutex_create(mutex_t *m) {
    *m = xSemaphoreCreateMutex();
    if (*m == NULL) {
        return false;
    }
    return true;
}

bool mutex_lock(mutex_t *m) {
    if (*m == NULL) {
        return false;
    }

    if (xSemaphoreTake(*m, portMAX_DELAY) == pdTRUE) {
        return true;
    }
    return false;
}


bool mutex_unlock(mutex_t *m) {
    if (*m == NULL) {
        return false;
    }

    if (xSemaphoreGive(*m) == pdTRUE) {
        return true;
    }
    return false;
}

void mutex_destroy(mutex_t *m) {
    if (*m != NULL) {
        vSemaphoreDelete(*m);
        *m = NULL;
    }
}

void os_panic(const char *fmt, ...) {
    va_list args;

    // Start the variable arguments list
    va_start(args, fmt);

    // Log the panic message with ESP32's logging system
    ESP_LOGE("PANIC", "PANIC: ");
    esp_log_vprintf(fmt, args);  // Print the formatted log message

    // Clean up variable arguments
    va_end(args);

    // Optionally trigger a system reboot to recover from the panic state
    ESP_LOGI("PANIC", "Rebooting system...");

    // Reboot the ESP32
    esp_restart();

}

void os_log(os_log_level_t level, const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Depending on the level, log using the appropriate ESP32 function
    switch (level) {
        case OS_LOG_LEVEL_INFO:
            esp_log_vprintf(ESP_LOG_INFO, tag, fmt, args);
            break;
        case OS_LOG_LEVEL_WARNING:
            esp_log_vprintf(ESP_LOG_WARN, tag, fmt, args);
            break;
        case OS_LOG_LEVEL_ERROR:
            esp_log_vprintf(ESP_LOG_ERROR, tag, fmt, args);
            break;
        case OS_LOG_LEVEL_CRITICAL:
            esp_log_vprintf(ESP_LOG_ERROR, tag, fmt, args);
            // Additional critical handling could be added here
            break;
    }

    va_end(args);
}
