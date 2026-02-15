#ifndef SPI_LINK_H
#define SPI_LINK_H

#include <stdint.h>

void spi_link_init(void);
void spi_link_send_test(void);
void spi_link_send_text(const uint8_t *text, uint8_t len);
void spi_link_pingpong(uint8_t *rx, uint8_t len);

#endif
