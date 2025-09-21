#include "os/os.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include <stdarg.h>
#include <stdio.h>

#define LOG_MESSAGE_BUFFER_SIZE 512

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

    char buffer[LOG_MESSAGE_BUFFER_SIZE];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    ESP_LOGE("PANIC", "%s", buffer);
    va_end(args);

}

void os_log(os_log_level_t level, const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[LOG_MESSAGE_BUFFER_SIZE];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    switch (level) {
        case OS_LOG_LEVEL_INFO:
            ESP_LOGI(tag, "%s", buffer);
            break;
        case OS_LOG_LEVEL_WARNING:
            ESP_LOGW(tag, "%s", buffer);
            break;
        case OS_LOG_LEVEL_ERROR:
        case OS_LOG_LEVEL_CRITICAL:
            ESP_LOGE(tag, "%s", buffer);
            break;
        default:
            ESP_LOGI(tag, "%s", buffer);
            break;
    }

    va_end(args);
}

void os_delay_ms(uint32_t milliseconds){
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}
