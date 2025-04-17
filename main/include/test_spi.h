#pragma once

// C libraries
#include <stdio.h>
#include <stdint.h>

// ESP32 libraries
#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"

#include "config.h"
#include "ring_link_payload.h"
#include "ring_link_internal.h"
#include "broadcast.h"

#ifdef __cplusplus
extern "C" {
#endif


void test_spi_internal_message_any_sibling( void );

void test_spi_internal_wrong_sibling( void );

void test_spi_internal_broadcast_to_siblings( void );

void test_spi_netif_message_any_sibling( void );

void test_spi_netif_bouncing_message( void );

void test_spi_run_all( void );

#ifdef __cplusplus
}
#endif

