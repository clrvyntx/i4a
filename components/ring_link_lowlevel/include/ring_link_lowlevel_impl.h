#ifndef __RING_LINK_LOWLEVEL_IMPL_H
#define __RING_LINK_LOWLEVEL_IMPL_H

#ifdef CONFIG_RING_LINK_LOWLEVEL_IMPL_SPI
#include "spi.h"

#define RING_LINK_LOWLEVEL_BUFFER_SIZE      SPI_BUFFER_SIZE
#define RING_LINK_LOWLEVEL_IMPL_INIT        spi_init
#define RING_LINK_LOWLEVEL_IMPL_TRANSMIT    spi_transmit
#define RING_LINK_LOWLEVEL_IMPL_RECEIVE     spi_receive
#endif

#ifdef CONFIG_RING_LINK_LOWLEVEL_IMPL_PHYSIM
#include "physim.h"

#define RING_LINK_LOWLEVEL_BUFFER_SIZE      PHYSIM_BUFFER_SIZE
#define RING_LINK_LOWLEVEL_IMPL_INIT        physim_init
#define RING_LINK_LOWLEVEL_IMPL_TRANSMIT    physim_transmit
#define RING_LINK_LOWLEVEL_IMPL_RECEIVE     physim_receive
#endif

#endif
