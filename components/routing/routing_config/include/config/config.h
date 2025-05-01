#ifndef _I4A_CONFIG_H_
#define _I4A_CONFIG_H_

#define N_DEVICES 5

typedef enum orientation {
    ORIENTATION_NORTH = 1,
    ORIENTATION_EAST = 2,
    ORIENTATION_SOUTH = 3,
    ORIENTATION_WEST = 4,
    ORIENTATION_CENTER = 5,
} orientation_t;

#endif  // _I4A_CONFIG_H_