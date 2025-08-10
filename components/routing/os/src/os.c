#include "os/os.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include <stdarg.h>
#include <stdio.h>

#define TAG "OS"

bool mutex_create(mutex_t *m)
{
    if (!m) return false;
    *m = (mutex_t)xSemaphoreCreateMutex();
    return (*m != NULL);
}

bool mutex_lock(mutex_t *m)
{
    if (!m || *m == NULL) return false;
    return (xSemaphoreTake((SemaphoreHandle_t)(*m), portMAX_DELAY) == pdTRUE);
}

bool mutex_unlock(mutex_t *m)
{
    if (!m || *m == NULL) return false;
    return (xSemaphoreGive((SemaphoreHandle_t)(*m)) == pdTRUE);
}

void mutex_destroy(mutex_t *m)
{
    if (!m || *m == NULL) return;
    vSemaphoreDelete((SemaphoreHandle_t)(*m));
    *m = NULL;
}

void os_panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ESP_LOGE(TAG, "PANIC: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);

    ESP_LOGE(TAG, "System will restart due to panic.");
    esp_restart();
}

void os_log(os_log_level_t level, const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    esp_log_level_t esp_level;
    switch (level) {
        case OS_LOG_LEVEL_INFO:
            esp_level = ESP_LOG_INFO;
            break;
        case OS_LOG_LEVEL_WARNING:
            esp_level = ESP_LOG_WARN;
            break;
        case OS_LOG_LEVEL_ERROR:
            esp_level = ESP_LOG_ERROR;
            break;
        case OS_LOG_LEVEL_CRITICAL:
            esp_level = ESP_LOG_ERROR;
            break;
        default:
            esp_level = ESP_LOG_INFO;
    }

    if (esp_log_level_get(tag) >= esp_level) {
        vprintf(fmt, args);
        printf("\n");
    }

    va_end(args);
}
