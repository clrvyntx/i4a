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
import struct
import json

UDP_IP = "10.255.255.254"
UDP_PORT = 8000

FMT = "<BBbIIII12s12s"
SIZE = struct.calcsize(FMT)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening on {UDP_IP}:{UDP_PORT}")

while True:
    data, addr = sock.recvfrom(1024)

    print("\n====================")
    print(f"From: {addr}")

    nodes = []

    for i in range(0, len(data), SIZE):
        chunk = data[i:i+SIZE]
        if len(chunk) != SIZE:
            continue

        (orientation,
         channel,
         rssi,
         subnet,
         mask,
         rx,
         tx,
         uuid,
         link) = struct.unpack(FMT, chunk)

        node = {
            "orientation": orientation,
            "channel": channel,
            "rssi": rssi,
            "subnet": subnet,
            "mask": mask,
            "rx_bytes": rx,
            "tx_bytes": tx,
            "uuid": uuid.decode(errors="ignore").strip("\x00"),
            "link": link.decode(errors="ignore").strip("\x00"),
        }

        nodes.append(node)

    payload = {
        "nodes": nodes,
        "count": len(nodes)
    }

    print(json.dumps(payload, indent=2))
