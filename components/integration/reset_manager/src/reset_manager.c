#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include <string.h>
#include "reset_manager/reset_manager.h"

static const char *TAG = "reset_manager";

static reset_manager_t reset_manager = {0};
static reset_manager_t *rm = &reset_manager;

#define RESET_TIMEOUT_US 30000000 // 30 Seconds

#define RM_OPCODE_RESET    0xA5
#define RM_OPCODE_STARTUP  0xB6

static void rm_generate_uuid_from_mac(char *uuid_out, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(uuid_out, len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void rm_on_sibling_message(void *ctx, const uint8_t *msg, uint16_t len) {
    if (len < 1) {
        return;
    }

    switch (msg[0]) {
        case RM_OPCODE_RESET:
            if (!rm->is_up) {
                rm->last_reset_time = esp_timer_get_time();
                ESP_LOGI(TAG, "Reset signal received: still on setup, not resetting");
                return;
            } else {
                ESP_LOGW(TAG, "Reset signal received: resetting device");
                esp_restart();
            }

            break;

        case RM_OPCODE_STARTUP:
            if (len != sizeof(rm_startup_packet_t)) {
                ESP_LOGW(TAG, "Invalid startup message length: %d", len);
                return;
            } else {
                const rm_startup_packet_t *startup = (const rm_startup_packet_t *)msg;

                strncpy(rm->uuid, startup->uuid, UUID_LENGTH);
                rm->uuid[UUID_LENGTH - 1] = '\0';
                rm->is_root = startup->is_root;
                ESP_LOGI(TAG, "Startup message received: UUID=%s, is_root=%d", rm->uuid, rm->is_root);

                rm->is_up = true;
            }
            break;

        default:
            ESP_LOGW(TAG, "Unknown message opcode: 0x%02X", msg[0]);
            break;
    }
}

// Initialize reset manager
void rm_init(ring_share_t *rs) {
    rm->rs = rs;
    rm->is_up = false;
    rm->is_root = false;
    rm->last_reset_time = esp_timer_get_time();
    rm_generate_uuid_from_mac(rm->mac, sizeof(rm->mac));
    memset(rm->uuid, 0, sizeof(rm->uuid));

    rs_register_component(
        rm->rs,
        RS_RESET_MANAGER,
        (ring_callback_t){
            .callback = rm_on_sibling_message,
            .context = rm
        }
    );

    ESP_LOGI(TAG, "Reset manager initialized");
}

bool rm_broadcast_reset(void) {
    if (!rm->rs) {
        ESP_LOGW(TAG, "RESET broadcast skipped: manager not initialized");
        return false;
    }

    rm->last_reset_time = esp_timer_get_time();
    uint8_t opcode = RM_OPCODE_RESET;

    bool result = rs_broadcast(rm->rs, RS_RESET_MANAGER, &opcode, 1);

    if(result) {
        ESP_LOGI(TAG, "Reset broadcast successfully sent to all devices");
    }

    return result;
}

bool rm_broadcast_startup_info(bool is_root) {
    if (!rm->rs) {
        ESP_LOGW(TAG, "STARTUP broadcast skipped: manager not initialized");
        return false;
    }

    rm_startup_packet_t packet;
    packet.opcode = RM_OPCODE_STARTUP;

    if(is_root){
        strncpy(rm->uuid, "000000000000", sizeof(rm->uuid));
    } else {
        strncpy(rm->uuid, rm->mac, sizeof(rm->uuid));
    }
 
    strncpy(packet.uuid, rm->uuid, sizeof(packet.uuid));
    packet.is_root = is_root ? 1 : 0;
    rm->is_root = is_root;

    bool broadcast = rs_broadcast(rm->rs, RS_RESET_MANAGER, (uint8_t *)&packet, sizeof(packet));

    if(broadcast) {
        ESP_LOGI(TAG, "Startup message broadcasted: UUID=%s, is_root=%d", rm->uuid, rm->is_root);
        rm->is_up = true;
    }

    return broadcast;
}

bool rm_should_device_reset(void){
    int64_t current_time = esp_timer_get_time();
    return ((current_time - rm->last_reset_time) > RESET_TIMEOUT_US);
}

bool rm_is_device_up(void) {
    return rm->is_up;
}

bool rm_is_root(void){
    return rm->is_root;
}

const char *rm_get_uuid(void) {
    return rm->uuid;
}

const char *rm_get_mac(void){
    return rm->mac;
}

int64_t rm_get_last_reset_time(void) {
    return rm->last_reset_time;
}
