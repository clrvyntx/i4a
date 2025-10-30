#include "siblings/siblings.h"
#include "node.h"

void sb_register_callback(siblings_t *sb, siblings_callback_t callback, void *context){
    sb->callback = callback;
    sb->context = context;
}

bool sb_broadcast_to_siblings(siblings_t *sb, const uint8_t *msg, uint16_t len){
    return node_broadcast_to_siblings(msg, len);
}
