#pragma once

#include "esp_log.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_PIN_0 22 // the lowest bit
#define CONFIG_PIN_1 21
#define CONFIG_PIN_2 16 // the highest bit
#define CONFIG_PIN_MASK  ((1ULL<<CONFIG_PIN_0) | (1ULL<<CONFIG_PIN_1) | (1ULL<<CONFIG_PIN_2))

typedef enum __attribute__((__packed__)) {
    CONFIG_ORIENTATION_NORTH = 0, // 000
    CONFIG_ORIENTATION_SOUTH = 1, // 001
    CONFIG_ORIENTATION_EAST  = 2, // 010
    CONFIG_ORIENTATION_WEST  = 3, // 011
    CONFIG_ORIENTATION_NONE  = 9,
} config_orientation_t;

typedef enum __attribute__((__packed__)) {
    CONFIG_MODE_PEER_LINK    = 0, // 0**
    CONFIG_MODE_ACCESS_POINT = 4, // 100 - Wi-Fi AccessPoint
    CONFIG_MODE_ROOT         = 5, // 101
    CONFIG_MODE_NONE         = 9,
} config_mode_t;

typedef enum __attribute__((__packed__)) {
    CONFIG_ID_NORTH   = 0, // 000
    CONFIG_ID_SOUTH   = 1, // 001
    CONFIG_ID_EAST    = 2, // 010
    CONFIG_ID_WEST    = 3, // 011
    CONFIG_ID_CENTER  = 4, // 100
    CONFIG_ID_NONE    = 9,
    CONFIG_ID_ANY     = 10,
    CONFIG_ID_ALL     = 11,
} config_id_t;

typedef struct {
    config_id_t id;
    config_mode_t mode;
    config_orientation_t orientation;
} config_t;

void config_setup(void);

void config_print(void);

config_id_t config_get_id(void);

config_mode_t config_get_mode(void);

config_orientation_t config_get_orientation(void);

#ifdef __cplusplus
}
#endif

