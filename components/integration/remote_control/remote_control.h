#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
    #endif

    // Function to start the remote command server
    void remote_command_server_create();

    // Function to close the remote command server
    void remote_command_server_close();

    #ifdef __cplusplus
}
#endif

#endif // REMOTE_CONTROL_H
