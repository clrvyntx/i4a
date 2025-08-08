#include <string.h>
#include <stdint.h>
#include <stdbool.h>

bool is_network_allowed(char* device_uuid, char* network_prefix, char* network_name) {
    // Must contain the prefix
    if (strstr(network_name, network_prefix) == NULL) {
        return false;
    }

    // Must NOT contain the device UUID
    if (strstr(network_name, device_uuid) != NULL) {
        return false;
    }

    // Must NOT be a center node (check if "_C_" is part of the network name)
    if (strstr(network_name, "_C_") != NULL) {
        return false;
    }

    return true;
}
