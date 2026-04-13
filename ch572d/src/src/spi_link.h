#ifndef SPI_LINK_H
#define SPI_LINK_H

#include <stdint.h>

#define SPI_LINK_FRAME_LEN 64U

void spi_link_init(void);
void spi_link_send_test(void);
void spi_link_send_text(const uint8_t *text, uint8_t len);
void spi_link_pingpong(uint8_t *rx, uint8_t len);
void spi_link_get_versions(uint8_t *rx, uint8_t len);
void spi_link_get_active_usb_info(uint16_t offset, uint8_t req_len, uint8_t *rx, uint8_t len);
void spi_link_capture_read(uint8_t *rx, uint8_t len);

#endif
