#include "ring_share/ring_share.h"
#include "channel_manager/channel_manager.h"
#include "callbacks.h"
#include "internal_messages.h"

static ring_share_t rs = { 0 };

void node_setup_internal_messages(uint8_t orientation){
    siblings_t *sb = node_get_siblings_instance();
    rs_init(&rs, sb);
    cm_init(&rs, orientation);
}

ring_share_t *node_get_rs_instance(void){
    return &rs;
}
