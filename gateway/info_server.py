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

UDP_IP = "0.0.0.0"
UDP_PORT = 8000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening on {UDP_IP}:{UDP_PORT}")

while True:
    data, addr = sock.recvfrom(4096)

    print("\n====================")
    print(f"From: {addr}")

    try:
        payload = json.loads(data.decode("utf-8"))
    except Exception as e:
        print("Failed to decode JSON:", e)
        print("Raw data:", data)
        continue

    # Your ESP format:
    # {"fields":[...], "data":[[...]...], "uptime_mins":...}

    nodes = []
    fields = payload.get("fields", [])
    rows = payload.get("data", [])

    for row in rows:
        node = dict(zip(fields, row))
        nodes.append(node)

    result = {
        "nodes": nodes,
        "count": len(nodes),
        "uptime_mins": payload.get("uptime_mins")
    }

    print(json.dumps(result, indent=2))
