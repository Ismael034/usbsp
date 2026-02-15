#include "../usb_webusb_cmds_internal.h"
#include "spi_link.h"
#include "../../debug_log.h"

uint8_t webusb_cmd_spi_pingpong(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    uint8_t rx_buf[22];

    (void)buf;
    (void)len;

    spi_link_init();
    spi_link_pingpong(rx_buf, (uint8_t)sizeof(rx_buf));

    resp[0] = WEBUSB_CMD_SPI_PINGPONG;
    resp[1] = 0x00;
    resp[2] = (uint8_t)sizeof(rx_buf);
    for (uint8_t i = 0; i < (uint8_t)sizeof(rx_buf); i++) {
        resp[3 + i] = rx_buf[i];
    }
    *resp_len = (uint8_t)(3 + sizeof(rx_buf));
    LOG_DEBUG("SPI pingpong len=%u", (unsigned)sizeof(rx_buf));
    return 0;
}
