# Gateway

This folder contains scripts and configurations for a **generic LAN-to-WAN gateway device**.
The gateway connects the local network of node devices to an external network (WAN/Internet) using **NAT**, while providing a local HTTP server to receive JSON data from ESP devices on the LAN.

## Features

- **LAN-to-WAN Routing**: Forwards traffic from the LAN interface to the WAN interface using NAT.
- **Automatic Network Setup**: Bash dispatcher script configures IP forwarding, routing, and MASQUERADE NAT when the LAN interface connects.
- **JSON Receiver**: Flask server listens on LAN for ESP devices to send data.
- **Flexible Device**: Can run on any device with two network interfaces (LAN + WAN).
