#ifndef SPI_LINK_H
#define SPI_LINK_H

#include <stdint.h>

void spi_link_init(void);
void spi_link_task(void);
void spi_link_capture_packet(uint8_t direction_in, uint8_t endpoint, uint8_t endpoint_type, const uint8_t *data, uint16_t len);

#endif
