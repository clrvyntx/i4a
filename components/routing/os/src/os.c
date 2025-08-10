#include "os/os.h"

bool mutex_create(mutex_t *m) {
    (void)m;
    return true;
}

bool mutex_lock(mutex_t *m) {
    (void)m;
    return true;
}

bool mutex_unlock(mutex_t *m) {
    (void)m;
    return true;
}

void mutex_destroy(mutex_t *m) {
    (void)m;
}
