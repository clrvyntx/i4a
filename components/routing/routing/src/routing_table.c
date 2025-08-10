#include "routing/routing_table.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os/os.h"
#include "utils.h"

#define TAG "routing"

void routing_table_init(rt_routing_table_t *table, uint8_t default_gateway) {
    memset(table, 0, sizeof(rt_routing_table_t));

    table->default_gateway = default_gateway;
}

void routing_table_add(rt_routing_table_t *table, uint32_t network, uint32_t mask, uint8_t output) {
    if ((network == 0) && (mask == 0)) {
        table->default_gateway = output;
        return;
    }

    size_t position = table->count;
    for (size_t i = 0; i < table->count; i++) {
        rt_routing_entry_t *entry = &table->entries[i];
        if ((entry->network == network) && (entry->mask == mask)) {
            // Same entry, update output
            entry->output = output;
            return;
        }

        if (entry->mask < mask) {
            // We need to insert at this index
            position = i;
            break;
        }
    }

    if ((table->count >= MAX_ROUTING_ENTRIES) || (position >= MAX_ROUTING_ENTRIES)) {
        os_panic("Routing table is full (used entries = %u) -- aborting\n", MAX_ROUTING_ENTRIES);
        return;
    }

    // We need to move everything between position and table->count one position to the right
    size_t elements_to_move = table->count - position;
    if (elements_to_move > 0)
        memmove(
            &table->entries[position + 1], &table->entries[position], elements_to_move * sizeof(rt_routing_entry_t)
        );

    table->entries[position].mask = mask;
    table->entries[position].network = network;
    table->entries[position].output = output;
    table->count++;
}

void routing_table_remove_by_output(rt_routing_table_t *table, uint8_t output) {
    rt_routing_table_t shadow = *table;
    size_t new_count = 0;
    for (size_t i = 0; i < shadow.count; i++) {
        if (shadow.entries[i].output == output)
            continue;

        table->entries[new_count] = shadow.entries[i];
        new_count++;
    }

    table->count = new_count;
}

uint8_t routing_table_route(const rt_routing_table_t *table, uint32_t ip) {
    for (size_t i = 0; i < table->count; i++) {
        const rt_routing_entry_t *entry = &table->entries[i];

        if ((ip & entry->mask) == entry->network) {
            return entry->output;
        }
    }

    return table->default_gateway;
}

void routing_table_show(const rt_routing_table_t *table) {
    log_info(TAG, "========= ROUTING TABLE ==========");
    log_info(TAG, "                default gateway: %u", table->default_gateway);

    for (size_t i = 0; i < table->count; i++) {
        const rt_routing_entry_t *entry = &table->entries[i];
        const uint8_t *ip = (const uint8_t *)&entry->network;
        char buffer[30];
        snprintf(
            buffer, 30, " %u.%u.%u.%u/%u -> %u",
            ip[3], ip[2], ip[1], ip[0],
            (unsigned int) mask_size(entry->mask),
            (unsigned int) entry->output);
        log_info(TAG, "%s", buffer);
    }
    log_info(TAG, "==================================");
}
