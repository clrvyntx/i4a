# ESP32 Ring Network - Technical Documentation

## 1. Component Structure
- **config**: Role and network configuration
- **ring_link**: Main communication layer
- **ring_link_internal**: Ring internal management 
- **ring_link_lowlevel**: Low-level operations
- **ring_link_netif**: Network interface
- **spi**: Custom SPI driver
- **wifi**: WiFi connection management
- **lwip_custom_hooks**: Custom lwIP hooks
- **physim**: Physical layer simulation
- **utils**: General utilities
- **route**: Static routes

## 2. Internal Communication

## 2.1 Low-Level Protocol
At the very bottom, this project relies on SPI for communication, although it can be changed as long as the primitives for the new low-level communication protocol get implemented.

Message format structure:

**Header**: 6 bytes

![alt text](./arquitectura-payload.svg)

**Buffer**

![alt text](./arquitectura-payload.buffer.svg)


```c
typedef struct {
    ring_link_payload_id_t id;        // 8-bit message identifier
    ring_link_payload_buffer_type_t buffer_type;
    uint8_t len;
    uint8_t ttl;                      // Time To Live to prevent infinite message loops
    config_id_t src_id;               // Source node identifier (North, South, etc.)
    config_id_t dst_id;               // Destination node identifier
    char buffer[RING_LINK_PAYLOAD_BUFFER_SIZE];
} ring_link_payload_t;
```


#### Field Descriptions
- **id** (8 bits): Message identifier. Simple counter implementation sufficient for ring network scale.
- **buffer_type**: Defines the message type in the ring:
  - `INTERNAL (0x11)`: Ring internal messages
  - `INTERNAL_HEARTBEAT (0x12)`: Monitoring heartbeat
  - `ESP_NETIF (0x80)`: Network communication
- **ttl**: Time To Live counter to prevent messages from circulating indefinitely in the ring
- **src_id**: Identifies the source node (North, South, East, West, AP)
- **dst_id**: Identifies the destination node
- **buffer**: Payload content, which can contain either:
  - Internal ring management data
  - IP payload information
  - The interpretation depends on buffer_type


Payload Types:
- `INTERNAL (0x11)`: Ring internal messages
- `INTERNAL_HEARTBEAT (0x12)`: Monitoring heartbeat
- `ESP_NETIF (0x80)`: Network communication

### 3. Node Configuration

GPIO-based configuration system:
```c
static config_t s_config = {
    .id          = CONFIG_ID_NONE,
    .mode        = CONFIG_MODE_NONE,
    .orientation = CONFIG_ORIENTATION_NONE,
    .rx_ip_addr  = 0,
    .tx_ip_addr  = 0,
};
```
