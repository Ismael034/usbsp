#include "../usb_webusb_cmds_internal.h"
#include "../../debug_log.h"
#include "spi_link.h"
#include <string.h>

uint8_t webusb_cmd_get_active_usb_info(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    uint8_t spi_resp[64];
    uint16_t offset = 0U;
    uint8_t req_len = 59U;
    uint8_t copy_len = 0U;

    if (!resp || !resp_len) {
        return 1;
    }

    if (len >= 4) {
        offset = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
        req_len = buf[3];
    }
    if (req_len > 59U) {
        req_len = 59U;
    }

    spi_link_get_active_usb_info(offset, req_len, spi_resp, (uint8_t)sizeof(spi_resp));

    resp[0] = WEBUSB_CMD_GET_ACTIVE_USB_INFO;
    resp[1] = 0x00;
    resp[2] = 0x00;
    resp[3] = 0x00;
    resp[4] = 0x00;
    *resp_len = 5;

    if (spi_resp[0] != 0x83U) {
        LOG_WARN("USB info SPI mismatch: %02X %02X %02X %02X %02X %02X %02X %02X",
                 spi_resp[0], spi_resp[1], spi_resp[2], spi_resp[3],
                 spi_resp[4], spi_resp[5], spi_resp[6], spi_resp[7]);
        resp[1] = 0x02;
        return 0;
    }

    copy_len = spi_resp[3];
    if (copy_len > (WEBUSB_MAX_PACKET - 5U)) {
        copy_len = WEBUSB_MAX_PACKET - 5U;
    }

    resp[2] = spi_resp[1];
    resp[3] = spi_resp[2];
    resp[4] = copy_len;
    if (copy_len != 0U) {
        memcpy(&resp[5], &spi_resp[4], copy_len);
    }
    *resp_len = (uint8_t)(5U + copy_len);
    return 0;
}
