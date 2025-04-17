#ifndef _PHYSIM_H_
#define _PHYSIM_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_check.h"

// Internal configuration variables
#define PS_CFG_BASE                 0xFFFFFF00

// Simulation device ID
#define PS_CFG_DEVICE_ID            (PS_CFG_BASE + 0x00)

/// Establishes a connection to the simulation controller. 
///
/// Will hold execution until the simulation controller is ready to launch it.
void physim_setup();

/// @brief Retrieves a configuration word from the simulation controller.
/// @param word_id ID of custom configuration word or any of PS_CFG_* constants.
/// @return Value of configuration word or 0xFFFF_FFFF if it's not set
uint32_t physim_get_config_word(uint32_t word_id);

/// @brief Sends a packet through a link.
/// @param link_id ID of link to send data
/// @param data Data to send, should be smaller than 1600 bytes
/// @param data_sz Size of data to send, in bytes.
void physim_link_send(uint16_t link_id, const void *data, uint32_t data_sz);

/// @brief Blocks until a packet arrives from a link
/// @param link_id ID of link to read. Must be connected to this device in the simulation.
/// @param size Size of payload will be returned in this variable. Must be non-null.
/// @return Payload received. Will return NULL in case the link is not connected to this device.
uint8_t *physim_link_recv(uint16_t link_id, uint32_t *size);


#define PHYSIM_BUFFER_SIZE 100

#define SPI_IN  0x00000001  // arbitrary, defined in basic.py
#define SPI_OUT 0x00000002  // arbitrary, defined in basic.py

esp_err_t physim_init(void);
esp_err_t physim_transmit(void *p, size_t len);
esp_err_t physim_receive(void *p, size_t len);


#endif // _PHYSIM_H_