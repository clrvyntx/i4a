#ifndef __RING_LINK_LOWLEVEL_IMPL_H
#define __RING_LINK_LOWLEVEL_IMPL_H

#include "spi.h"

#define RING_LINK_LOWLEVEL_BUFFER_SIZE      SPI_BUFFER_SIZE
#define RING_LINK_LOWLEVEL_IMPL_INIT        spi_init
#define RING_LINK_LOWLEVEL_IMPL_TRANSMIT    spi_transmit
#define RING_LINK_LOWLEVEL_IMPL_RECEIVE     spi_receive

#endif
