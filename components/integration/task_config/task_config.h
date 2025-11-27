#ifndef _TASK_CONFIG_H_
#define _TASK_CONFIG_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Priority Levels

#define LOW_PRIORITY  (tskIDLE_PRIORITY + 2)
#define MID_PRIORITY  (tskIDLE_PRIORITY + 4)
#define HIGH_PRIORITY (tskIDLE_PRIORITY + 10)

// SPI polling task

#define TASK_SPI_CORE        1
#define TASK_SPI_STACK       4096
#define TASK_SPI_PRIORITY    HIGH_PRIORITY

// Ring Link Layer tasks

#define TASK_RING_LINK_CORE        1
#define TASK_RING_LINK_STACK       4096
#define TASK_RING_LINK_PRIORITY    MID_PRIORITY

#define TASK_RING_LINK_NETIF_CORE         1
#define TASK_RING_LINK_NETIF_STACK        4096
#define TASK_RING_LINK_NETIF_PRIORITY     MID_PRIORITY

#define TASK_RING_LINK_INTERNAL_CORE      1
#define TASK_RING_LINK_INTERNAL_STACK     4096
#define TASK_RING_LINK_INTERNAL_PRIORITY  LOW_PRIORITY

// Info Manager tasks

#define TASK_HTTP_CLIENT_CORE       1
#define TASK_HTTP_CLIENT_STACK      8192
#define TASK_HTTP_CLIENT_PRIORITY   LOW_PRIORITY

#define TASK_IM_SCHEDULER_CORE      0
#define TASK_IM_SCHEDULER_STACK     4096
#define TASK_IM_SCHEDULER_PRIORITY  LOW_PRIORITY

// Callbacks tasks

#define TASK_CALLBACK_CORE          0
#define TASK_CALLBACK_STACK         4096
#define TASK_CALLBACK_PRIORITY      LOW_PRIORITY

// Wireless tasks

#define TASK_CLIENT_CORE            1
#define TASK_CLIENT_STACK           4096
#define TASK_CLIENT_PRIORITY        LOW_PRIORITY

#define TASK_SERVER_CORE            1
#define TASK_SERVER_STACK           4096
#define TASK_SERVER_PRIORITY        LOW_PRIORITY

#define TASK_DEVICE_CORE            0
#define TASK_DEVICE_STACK           4096
#define TASK_DEVICE_PRIORITY        LOW_PRIORITY

// Routing tasks

#define TASK_ROUTING_CORE           0
#define TASK_ROUTING_STACK          4096
#define TASK_ROUTING_PRIORITY       LOW_PRIORITY


#endif // _TASK_CONFIG_H
