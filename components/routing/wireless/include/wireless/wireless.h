#ifndef _WIRELESS_H_
#define _WIRELESS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct wireless_callbacks {
    /**
     * on_peer_connected callback
     *
     * Called when a new connection with a peer is established.
     *
     * This callback is executed in both devices (the one acting as AP
     * and the one acting as STAtion) with the exact same values for
     * network and mask.
     *
     * E.g.:
     *  - Device d1 (STA) connects to device d2 (STA)
     *  - Device d2 provides d1 10.160.0.0/11 (via DHCP or any other method)
     *  - d1 and d2 are now able to exchange messages and IP traffic
     *  - on_peer_connnected is triggered in *both* devices with:
     *    - network=0x0AA0_0000 (10.160.0.0 encoded as uint32_t)
     *    - mask=0xFFE0_0000 (255.224.0.0 encoded as uint32_t)
     */
    void (*on_peer_connected)(void *ctx, uint32_t network, uint32_t mask);

    /**
     * on_peer_message callback
     *
     * Called when the wireless peer sends a peer message to us.
     *
     * NOTE: It is assumed that this callback will be called after
     * a call to on_peer_connected and before on_peer_lost.
     */
    void (*on_peer_message)(void *ctx, const uint8_t *msg, uint16_t len);

    /**
     * on_peer_lost callback
     *
     * Called when a connection with the wireless peer is lost.
     *
     * This callback is executed in *both* devices in the same way as
     * on_peer_connected. The exact same rules apply, and the same
     * information should be provided in the parameters.
     */
    void (*on_peer_lost)(void *ctx, uint32_t network, uint32_t mask);
} wireless_callbacks_t;

typedef struct wireless {
    wireless_callbacks_t callbacks;
    void *context;
} wireless_t;

/**
 * Registers a set of functions to be called whenever a wireless event occurs.
 *
 * See the structure docs for the definition of each callback.
 */
void wl_register_peer_callbacks(wireless_t *wl, wireless_callbacks_t callbacks, void *context);

/**
 * Sends a message to the wireless peer.
 *
 * Returns true on success, false in case of error (usually, peer lost).
 */
bool wl_send_peer_message(wireless_t *wl, const void *msg, uint16_t len);

/**
 * Informs the wireless subsystem that this device can provide Internet
 * at the given network.
 *
 * Parameters network and mask are encoded in the same way as in the
 * callbacks for on_peer_connected/lost.
 */
void wl_enable_ap_mode(wireless_t *wl, uint32_t network, uint32_t mask);

#endif  // _WIRELESS_H_
