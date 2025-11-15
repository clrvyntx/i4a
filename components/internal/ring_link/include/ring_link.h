#pragma once

#include "ring_link_lowlevel.h"
#include "ring_link_internal.h"
#include "ring_link_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RING_LINK_MEM_TASK 16384

esp_err_t ring_link_init(void);


#ifdef __cplusplus
}
#endif

