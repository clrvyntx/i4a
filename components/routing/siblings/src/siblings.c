#include "siblings/siblings.h"
#include "node.h"

bool sb_broadcast_to_siblings(siblings_t *sb, const uint8_t *msg, uint16_t len){
    return node_broadcast_to_siblings(msg, len);
}
