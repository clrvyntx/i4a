#pragma once

#include <esp_timer.h>
#include <esp_log.h>

#include "ring_link_internal.h"


#ifdef __cplusplus
extern "C" {
#endif

#define HEARTBEAT_PAYLOAD "HEARTBEAT..."
#define HEARTBEAT_TIMER_NAME "heartbeat"
#define HEARTBEAT_PERIOD_IN_SEC 5
#define HEARTBEAT_PERIOD (HEARTBEAT_PERIOD_IN_SEC*1000000)
#define HEARTBEAT_FAIL_THRESHOLD 3  // Number of consecutive failures before considering a board as "out"


esp_err_t heartbeat_init(void);


#ifdef __cplusplus
}
#endif
