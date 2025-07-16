#include "heartbeat.h"

static const char *TAG = "==> heartbeat";

static bool s_node_online = false;
static int s_heartbeat_id = 0;
static int s_fail_streak = 0;

static void node_online_callback(){
    ESP_LOGI(TAG, "Node is ONLINE.");
    s_heartbeat_id = 0;
}

static void node_offline_callback(){
    ESP_LOGE(TAG, "Maximum failures reached. Node considered OFFLINE.");
}

static void heartbeat_callback() {
    bool succeded = broadcast_to_siblings(HEARTBEAT_PAYLOAD, sizeof(HEARTBEAT_PAYLOAD));
    if (succeded) {
        s_fail_streak = 0;  // reset counter after each success
        ESP_LOGD(TAG, "Heartbeat %d succeeded.", s_heartbeat_id);
        if (!s_node_online) {
            node_online_callback();
            s_node_online = true;
        }
    } else {
        s_fail_streak++;
        ESP_LOGW(TAG, "Heartbeat %d failed. Failure #%d", s_heartbeat_id, s_fail_streak);
        if (s_node_online && (s_fail_streak >= HEARTBEAT_FAIL_THRESHOLD)) {
            node_offline_callback();
            s_node_online = false;
        }
    }
    s_heartbeat_id++;
}

esp_err_t heartbeat_init(void) {
    esp_timer_handle_t timer_handle;
    esp_timer_create_args_t timer_args = {
        .name                  = HEARTBEAT_TIMER_NAME,
        .callback              = &heartbeat_callback,
        .skip_unhandled_events = true
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, HEARTBEAT_PERIOD));

    return ESP_OK;
}