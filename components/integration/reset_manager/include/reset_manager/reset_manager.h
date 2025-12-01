#ifndef _RESET_MANAGER_H_
#define _RESET_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

#include "ring_share/ring_share.h"

#define UUID_LENGTH 13

typedef struct {
    uint8_t opcode;
    char uuid[UUID_LENGTH];
    uint8_t is_root;
} rm_startup_packet_t;

typedef struct reset_manager {
    ring_share_t *rs;
    bool is_up;
    bool is_root;
    char uuid[UUID_LENGTH];
    char mac[UUID_LENGTH];
    int64_t last_reset_time;
} reset_manager_t;

void rm_init(ring_share_t *rs);
bool rm_broadcast_reset(void);
bool rm_broadcast_startup_info(bool is_root);
bool rm_is_device_up(void);
bool rm_is_root(void);
bool rm_should_device_reset(void);
const char *rm_get_uuid(void);
const char *rm_get_mac(void);
int64_t rm_get_last_reset_time(void);

#endif  // _RESET_MANAGER_H_
