#ifndef _TASK_CONFIG_H_
#define _TASK_CONFIG_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Priorities

#define LOW_PRIORITY (tskIDLE_PRIORITY + 2)
#define MID_PRIORITY (tskIDLE_PRIORITY + 4)
#define HIGH_PRIORITY (tskIDLE_PRIORITY + 10)

// Task config

#define TASK_HTTP_CLIENT_CORE         1
#define TASK_HTTP_CLIENT_STACK        8092
#define TASK_HTTP_CLIENT_PRIORITY     LOW_PRIORITY

#define TASK_IM_SCHEDULER_CORE        1
#define TASK_IM_SCHEDULER_STACK       4096
#define TASK_IM_SCHEDULER_PRIORITY    LOW_PRIORITY

#define TASK_CALLBACK_CORE            1
#define TASK_CALLBACK_STACK           4096
#define TASK_CALLBACK_PRIORITY        LOW_PRIORITY

#define TASK_DEVICE_CORE              1
#define TASK_DEVICE_STACK             4096
#define TASK_DEVICE_PRIORITY          LOW_PRIORITY

#define TASK_CLIENT_CORE              1
#define TASK_CLIENT_STACK             4096
#define TASK_CLIENT_PRIORITY          LOW_PRIORITY

#define TASK_SERVER_CORE              1
#define TASK_SERVER_STACK             4096
#define TASK_SERVER_PRIORITY          LOW_PRIORITY

#define TASK_ROUTING_CORE             0
#define TASK_ROUTING_STACK            4096
#define TASK_ROUTING_PRIORITY         LOW_PRIORITY

#endif // _TASK_CONFIG_H
