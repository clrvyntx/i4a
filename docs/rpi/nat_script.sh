#!/bin/bash
#
# FILE: /etc/NetworkManager/dispatcher.d/99-run-network-setup.sh
# PURPOSE: Auto-configure routing and NAT when wlan0 connects.
#
# This script is triggered by NetworkManager whenever wlan0 comes up.
# It:
#  - Enables IP forwarding (routing)
#  - Removes incorrect default route via wlan0 (if present)
#  - Adds a route to the 10.0.0.0/8 network via the access point at 192.168.3.1
#  - Applies NAT (MASQUERADE) so traffic from wlan0 is routed out eth0
#
# MAKE SURE:
#  - This script is executable: sudo chmod +x /etc/NetworkManager/dispatcher.d/99-run-network-setup.sh

IFACE="$1"
STATUS="$2"

if [ "$IFACE" == "wlan0" ] && [ "$STATUS" == "up" ]; then
    echo "[+] wlan0 is up, applying custom network rules..."

    # Enable IP forwarding (for runtime)
    sysctl -w net.ipv4.ip_forward=1

    # Remove bad default route via wlan0, if present
    ip route del default via 192.168.3.2 dev wlan0 2>/dev/null

    # Route all 10.0.0.0/8 traffic to the AP bridge at 192.168.3.1
    ip route add 10.0.0.0/8 via 192.168.3.1 dev wlan0

    # Set up NAT: outgoing traffic from wlan0 to eth0 gets masqueraded
    iptables -t nat -C POSTROUTING -o eth0 -j MASQUERADE 2>/dev/null || \
    iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE

    echo "[+] Network rules applied successfully."
fi
