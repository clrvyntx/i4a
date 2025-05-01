#ifndef _ROUTING_TABLE_H_
#define _ROUTING_TABLE_H_

#include <stdint.h>
#include <stdlib.h>

#define MAX_ROUTING_ENTRIES 16

/**
 * The routing_entry and routing_table structure will be shared
 * raw between siblings. They cannot contain any pointers.
 */

/**
 * Routing table entry.
 *  network: Network IP address in integer format. E.g: 10.0.0.1 would
 *           be encoded as 0x0A000001.
 *  mask:    Subnet mask in integer format. E.g. 255.0.0.0 would be encoded
 *           as 0xFF000000.
 *  output:  This can be any value between 0 and 255 used to reference the
 *           output.
 *
 * NOTE: This structure cannot contain pointers.
 */
typedef struct rt_routing_entry {
    uint32_t network;
    uint32_t mask;
    uint8_t output;
} rt_routing_entry_t;

/**
 * Routing table entry.
 *  default_gateway: Any value between 0 and 255 used to reference the output
 *                   used as default gateway.
 *  count:           Number of used entries.
 *  entries:         Routing table entries array
 *
 * NOTE: This structure cannot contain pointers.
 */
typedef struct rt_routing_table {
    uint8_t default_gateway;

    size_t count;
    rt_routing_entry_t entries[MAX_ROUTING_ENTRIES];
} rt_routing_table_t;

/**
 * Initializes the routing table clearing all the entries and setting
 * the default gateway to the specified value.
 */
void routing_table_init(rt_routing_table_t *table, uint8_t default_gateway);

/**
 * Adds a new entry to the routing table.
 *
 * If an entry with the exact network and mask is present, it will replace the output.
 *
 * NOTE: Will abort if there's no more space in the table.
 */
void routing_table_add(rt_routing_table_t *table, uint32_t network, uint32_t mask, uint8_t output);

/**
 * Removes all non-default routes with the specified output as destination.
 *
 * The default gateway will not be changed.
 */
void routing_table_remove_by_output(rt_routing_table_t *table, uint8_t output);

/**
 * Determines which output to use to route to this IP using the
 * longest prefix match algorithm.
 *
 * If no route is found, the default gateway will be returned.
 */
uint8_t routing_table_route(const rt_routing_table_t *table, uint32_t ip);

/**
 * Prints the routing table to log_info.
 */
void routing_table_show(const rt_routing_table_t *table);

#endif  // _ROUTING_TABLE_H_
