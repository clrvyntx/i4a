#!/bin/bash

# File to deploy
SCRIPT_SOURCE="./script"

# Destination in dispatcher.d
SCRIPT_DEST="/etc/NetworkManager/dispatcher.d/99-run-network-setup.sh"

# Check if source script exists
if [ ! -f "$SCRIPT_SOURCE" ]; then
    echo "[-] Source script '$SCRIPT_SOURCE' not found."
    exit 1
fi

# Check if iptables is installed
if ! command -v iptables >/dev/null 2>&1; then
    echo "[+] iptables not found. Installing..."
    sudo apt update
    sudo apt install -y iptables
else
    echo "[+] iptables is already installed."
fi

echo "[+] Copying NAT script to dispatcher.d..."
sudo cp "$SCRIPT_SOURCE" "$SCRIPT_DEST"

echo "[+] Setting ownership to root..."
sudo chown root:root "$SCRIPT_DEST"

echo "[+] Setting executable permissions..."
sudo chmod 755 "$SCRIPT_DEST"

echo "[+] Restarting NetworkManager to apply changes..."
sudo systemctl restart NetworkManager
