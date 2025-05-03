#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static const uint16_t SOUTH_NORTH_CHANNELS[] = {1, 3, 5, 7, 9, 11, 13};
static const uint16_t EAST_WEST_CHANNELS[] = {2, 4, 6, 8, 10, 12};

bool is_in_list(uint16_t number, const uint16_t list[], int size) {
  for (int i = 0; i < size; i++) {
    if (list[i] == number) {
      return true;
    }
  }
  return false;
}

bool is_channel_allowed(uint16_t orientation, uint16_t channel) {
  if (orientation == 0 || orientation == 1) {
    // Pass the size of the SOUTH_NORTH_CHANNELS array
    return is_in_list(channel, SOUTH_NORTH_CHANNELS, sizeof(SOUTH_NORTH_CHANNELS) / sizeof(SOUTH_NORTH_CHANNELS[0]));
  } else if (orientation == 2 || orientation == 3) {
    // Pass the size of the EAST_WEST_CHANNELS array
    return is_in_list(channel, EAST_WEST_CHANNELS, sizeof(EAST_WEST_CHANNELS) / sizeof(EAST_WEST_CHANNELS[0]));
  } else {
    return false; // Invalid orientation
  }
}

bool is_network_allowed(char* device_uuid, char* network_prefix, char* network_name) {
  return ((strstr(network_name, network_prefix) != NULL) && (strstr(network_name, device_uuid) == NULL));
}
