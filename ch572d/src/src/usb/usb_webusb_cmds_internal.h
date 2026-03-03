#ifndef USB_WEBUSB_CMDS_INTERNAL_H
#define USB_WEBUSB_CMDS_INTERNAL_H

#include <stdint.h>

#define WEBUSB_MAX_PACKET 64

enum {
    WEBUSB_CMD_EEPROM_READ = 0x01,
    WEBUSB_CMD_SPI_TEXT = 0x02,
    WEBUSB_CMD_SPI_PINGPONG = 0x03,
    WEBUSB_CMD_EEPROM_WRITE = 0x04,
    WEBUSB_CMD_GET_VERSIONS = 0x10,
};

typedef uint8_t (*webusb_cmd_fn)(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len);

uint8_t webusb_cmd_eeprom_read(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len);
uint8_t webusb_cmd_eeprom_write(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len);
uint8_t webusb_cmd_get_versions(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len);
uint8_t webusb_cmd_spi_text(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len);
uint8_t webusb_cmd_spi_pingpong(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len);

#endif
