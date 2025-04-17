#ifndef __ROUTE_H
#define __ROUTE_H


#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"


#include "ring_link_netif.h"
#include "wifi.h"

#define MAX_NUMBER_ROUTES 10

typedef enum {
    SPI_GATEWAY = 0,
    WIFI_GATEWAY = 1
} gateway_t;


/**
 * @brief Adds a new route to the routing table.
 *
 * This function creates a new network interface with the specified IP address, gateway, and netmask,
 * and adds it to the routing table. **Note:** This function does **not** perform validation for
 * duplicate routes. If a route with the same IP and netmask already exists, this function will
 * still create a new interface and add the route, potentially leading to multiple identical routes.
 *
 * **Thread Safety:** To ensure correct operation, calls to `add_route` should be synchronized externally
 * if used in a multithreaded environment. This could be achieved using mutexes or by ensuring that
 * all route modifications occur from a single thread.
 *
 * @param ip       The IP address for the new route (as a string, e.g., "192.168.1.1").
 * @param gw       The gateway type for the new route (e.g., `SPI_GATEWAY` or `WIFI_GATEWAY`).
 * @param netmask  The netmask for the new route (as a string, e.g., "255.255.255.0").
 * @param key      A unique key or identifier for the route (as a string).
 *
 * @return
 *     - `ESP_OK` on success.
 *     - `ESP_ERR_INVALID_STATE` if the network interface could not be created.
 */
esp_err_t add_route(char * ip, gateway_t gw, char * netmask, char * key);

/**
 * @brief Removes a route from the routing table.
 *
 * This function searches for a network interface (`netif`) that matches the specified IP address and netmask.
 * If a matching interface is found, it stops and destroys the network interface, effectively removing the route
 * from the routing table. The `num_routes` counter is decremented accordingly.
 *
 * **Thread Safety:** To ensure correct operation, calls to `add_route` should be synchronized externally
 * if used in a multithreaded environment. This could be achieved using mutexes or by ensuring that
 * all route modifications occur from a single thread.
 * 
 * **Note:** If the specified route does not exist, the function will return `ESP_ERR_NOT_FOUND`. The function
 * does not perform extensive validation beyond checking for an exact match of the IP address and netmask.
 *
 * @param ip       The IP address of the route to remove (as a string, e.g., "192.168.1.1").
 * @param netmask  The netmask of the route to remove (as a string, e.g., "255.255.255.0").
 *
 * @return
 *     - `ESP_OK` if the route was found and removed successfully.
 *     - `ESP_ERR_NOT_FOUND` if the route was not found in the routing table.
 */
esp_err_t rm_route(char * ip, char * netmask);
unsigned get_num_routes(void);

#endif
