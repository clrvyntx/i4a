#ifndef _I4A_OS_H_
#define _I4A_OS_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * OS provided abstractions:
 *  - Mutex
 *  - Timers
 *  - Queues
 */

typedef void *mutex_t;

bool mutex_create(mutex_t *m);
bool mutex_lock(mutex_t *m);
bool mutex_unlock(mutex_t *m);
void mutex_destroy(mutex_t *m);

#define WITH_LOCK(lock, block) \
    do {                       \
        mutex_lock(lock);      \
        block;                 \
        mutex_unlock(lock);    \
    } while (0);

/**
 * Called when the device encounters a condition that won't allow
 * it to continue operating.
 *
 * After calling os_panic the device may be in an inconsistent state.
 * Reboot may be required.
 *
 * Accepts printf-like formatted arguments.
 */
void os_panic(const char *fmt, ...);

typedef enum {
    OS_LOG_LEVEL_INFO,
    OS_LOG_LEVEL_WARNING,
    OS_LOG_LEVEL_ERROR,
    OS_LOG_LEVEL_CRITICAL,
} os_log_level_t;

/**
 * Outputs a log message using the platform's facilities.
 *
 * Accepts printf-like formatted arguments.
 *
 * Platform developers should only implement this function.
 *
 * Component developers should only use the convenience macros
 * log_info, log_warn, log_error and log_crit which are simple
 * wrappers to this function.
 */
void os_log(os_log_level_t level, const char *tag, const char *fmt, ...);

#define log_info(tag, fmt, ...) os_log(OS_LOG_LEVEL_INFO, tag, fmt, ##__VA_ARGS__)
#define log_warn(tag, fmt, ...) os_log(OS_LOG_LEVEL_WARNING, tag, fmt, ##__VA_ARGS__)
#define log_error(tag, fmt, ...) os_log(OS_LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#define log_crit(tag, fmt, ...) os_log(OS_LOG_LEVEL_CRITICAL, tag, fmt, ##__VA_ARGS__)

/**
 * Called when the device needs a delay for stability or setup purposes
 */

void os_delay_ms(uint32_t milliseconds);

#endif  // _I4A_OS_H_
