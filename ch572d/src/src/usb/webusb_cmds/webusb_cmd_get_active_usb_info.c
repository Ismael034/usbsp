#include "../usb_webusb_cmds_internal.h"
#include "../../debug_log.h"
#include "spi_link.h"
#include <string.h>

#define SPI_USB_INFO_MAGIC 0x83U
#define SPI_USB_INFO_HEADER_LEN 6U
#define SPI_USB_INFO_MAX_DATA (64U - SPI_USB_INFO_HEADER_LEN)

uint8_t webusb_cmd_get_active_usb_info(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    uint8_t spi_resp[64];
    uint16_t offset = 0U;
    uint16_t echoed_offset = 0U;
    uint16_t total_len = 0U;
    uint16_t remain = 0U;
    uint8_t req_len = SPI_USB_INFO_MAX_DATA;
    uint8_t copy_len = 0U;
    uint8_t attempt;
    uint8_t offset_match = 0U;

    if (!resp || !resp_len) {
        return 1;
    }

    if (len >= 4) {
        offset = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
        req_len = buf[3];
    }
    if (req_len > SPI_USB_INFO_MAX_DATA) {
        req_len = SPI_USB_INFO_MAX_DATA;
    }

    resp[0] = WEBUSB_CMD_GET_ACTIVE_USB_INFO;
    resp[1] = 0x00;
    resp[2] = 0x00;
    resp[3] = 0x00;
    resp[4] = 0x00;
    *resp_len = 5;

    for (attempt = 0U; attempt < 3U; attempt++) {
        spi_link_get_active_usb_info(offset, req_len, spi_resp, (uint8_t)sizeof(spi_resp));
        if (spi_resp[0] != SPI_USB_INFO_MAGIC) {
            continue;
        }

        echoed_offset = (uint16_t)spi_resp[4] | ((uint16_t)spi_resp[5] << 8);
        if (echoed_offset == offset) {
            offset_match = 1U;
            break;
        }

        LOG_WARN("USB info SPI offset mismatch: want=%u got=%u attempt=%u",
                 offset, echoed_offset, attempt);
    }

    if (spi_resp[0] != SPI_USB_INFO_MAGIC) {
        LOG_WARN("USB info SPI mismatch: %02X %02X %02X %02X %02X %02X %02X %02X",
                 spi_resp[0], spi_resp[1], spi_resp[2], spi_resp[3],
                 spi_resp[4], spi_resp[5], spi_resp[6], spi_resp[7]);
        resp[1] = 0x02;
        return 0;
    }
    if (offset_match == 0U) {
        LOG_WARN("USB info SPI stale response: want=%u got=%u", offset, echoed_offset);
        resp[1] = 0x03;
        return 0;
    }

    total_len = (uint16_t)spi_resp[1] | ((uint16_t)spi_resp[2] << 8);
    if (offset < total_len) {
        remain = (uint16_t)(total_len - offset);
    }

    copy_len = spi_resp[3];
    if (copy_len > req_len) {
        copy_len = req_len;
    }
    if ((uint16_t)copy_len > remain) {
        copy_len = (remain > 0xFFU) ? 0xFFU : (uint8_t)remain;
    }
    if (copy_len > (WEBUSB_MAX_PACKET - 5U)) {
        copy_len = WEBUSB_MAX_PACKET - 5U;
    }

    resp[2] = (uint8_t)(total_len & 0xFFU);
    resp[3] = (uint8_t)((total_len >> 8) & 0xFFU);
    resp[4] = copy_len;
    if (copy_len != 0U) {
        memcpy(&resp[5], &spi_resp[SPI_USB_INFO_HEADER_LEN], copy_len);
    }
    *resp_len = (uint8_t)(5U + copy_len);
    return 0;
}
