"""
ESP LAN UDP JSON Receiver

This script listens for UDP packets from ESP devices on the local network.
Each UDP packet contains a JSON payload identical to the previous HTTP POST body.

Usage:
    1. Make sure your ESP devices are on the same LAN.
    2. Run the server:
         python info_server.py
    3. ESP devices should send UDP packets to:
         10.255.255.254:8000
"""

import socket
import json

UDP_IP = "10.255.255.254"
UDP_PORT = 8000
BUFFER_SIZE = 4096

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind to fixed LAN IP + port
sock.bind((UDP_IP, UDP_PORT))

print(f"UDP server listening on {UDP_IP}:{UDP_PORT}")

while True:
    try:
        # Receive UDP packet
        data, addr = sock.recvfrom(BUFFER_SIZE)

        print("\n==============================")
        print(f"Packet from: {addr[0]}:{addr[1]}")
        print(f"Bytes received: {len(data)}")

        # Decode bytes to text
        text = data.decode("utf-8")

        print("\nRaw JSON:")
        print(text)

        # Parse JSON
        parsed = json.loads(text)

        print("\nParsed JSON:")
        print(json.dumps(parsed, indent=4))

    except json.JSONDecodeError as e:
        print(f"\nJSON parse error: {e}")

    except Exception as e:
        print(f"\nServer error: {e}")
