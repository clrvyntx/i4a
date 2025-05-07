#include <string.h>
#include <stdint.h>
#include <stdbool.h>

bool is_network_allowed(char* device_uuid, char* network_prefix, char* network_name) {
  return ((strstr(network_name, network_prefix) != NULL) && (strstr(network_name, device_uuid) == NULL));
}
