#ifndef _PYSIM_H_
#define _PYSIM_H_

#ifndef PYSIM
#error "PYSIM constant is not defined. Add -DPYSIM to compile options."
#endif

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifndef CONFIG_PYSIM_MAX_EVENTS
  #define CONFIG_PYSIM_MAX_EVENTS 8
#endif

typedef void (*ps_event_callback_t)(uint8_t event_id, const void *event_data, size_t sz_event_data);

void ps_register_event(uint8_t event_id, ps_event_callback_t callback);
void pysim_start();
uint8_t ps_execute(uint8_t command, const void* args, size_t sz_args, void* ret, size_t *sz_ret);
uint8_t ps_query(uint8_t command);

#endif // _PYSIM_H_
