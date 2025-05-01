#pragma once

#include "ring_link_lowlevel.h"
#include "ring_link_payload.h"
#include "broadcast.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RING_LINK_MEM_TASK 16384

esp_err_t ring_link_init(void);
void on_sibling_message(char *buffer, uint8_t len);

#ifdef __cplusplus
}
#endif

