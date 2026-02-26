#!/bin/bash
# deploy_esp_pi_server.sh
# Deploys both the NetworkManager dispatcher (NAT) and the Flask info server on a Raspberry Pi
# User-agnostic: does not assume any username

# -------------------------------
# CONFIGURATION
# -------------------------------
# Dispatcher script
DISPATCHER_SOURCE="./99-run-network-setup.sh"
DISPATCHER_DEST="/etc/NetworkManager/dispatcher.d/99-run-network-setup.sh"

# Flask server script
SERVER_SOURCE="./info_server.py"
SERVER_DEST="/home/$USER/info_server.py"   # place in current user's home
SERVER_PORT=8000

# Systemd service for Flask
SERVICE_FILE="/etc/systemd/system/info_server.service"

# -------------------------------
# CHECK SOURCE FILES
# -------------------------------
if [ ! -f "$DISPATCHER_SOURCE" ]; then
    echo "[-] Dispatcher script '$DISPATCHER_SOURCE' not found."
    exit 1
fi

if [ ! -f "$SERVER_SOURCE" ]; then
    echo "[-] Flask server script '$SERVER_SOURCE' not found."
    exit 1
fi

# -------------------------------
# INSTALL REQUIRED PACKAGES
# -------------------------------
echo "[+] Installing required packages..."
sudo apt update
sudo apt install -y iptables python3 python3-pip network-manager

# Install Flask for current user
pip3 install --user flask

# -------------------------------
# DEPLOY NETWORKMANAGER DISPATCHER
# -------------------------------
echo "[+] Deploying dispatcher script..."
sudo cp "$DISPATCHER_SOURCE" "$DISPATCHER_DEST"
sudo chown root:root "$DISPATCHER_DEST"
sudo chmod 755 "$DISPATCHER_DEST"
echo "[+] Dispatcher deployed."

# -------------------------------
# DEPLOY FLASK SERVER SCRIPT
# -------------------------------
echo "[+] Deploying Flask server script..."
cp "$SERVER_SOURCE" "$SERVER_DEST"
chmod 755 "$SERVER_DEST"
echo "[+] Flask server script deployed at $SERVER_DEST."

# -------------------------------
# CREATE SYSTEMD SERVICE FOR FLASK SERVER
# -------------------------------
echo "[+] Creating systemd service..."
sudo bash -c "cat > $SERVICE_FILE" <<EOL
[Unit]
Description=ESP Info Server
After=network.target

[Service]
ExecStart=/usr/bin/python3 $SERVER_DEST
WorkingDirectory=$HOME
Restart=always
User=$USER

[Install]
WantedBy=multi-user.target
EOL

sudo systemctl daemon-reload
sudo systemctl enable info_server.service
sudo systemctl start info_server.service
echo "[+] Systemd service created and started."

# -------------------------------
# RESTART NETWORKMANAGER TO APPLY DISPATCHER
# -------------------------------
echo "[+] Restarting NetworkManager to apply dispatcher..."
sudo systemctl restart NetworkManager

echo "[+] Deployment complete. Flask server running on port $SERVER_PORT."
