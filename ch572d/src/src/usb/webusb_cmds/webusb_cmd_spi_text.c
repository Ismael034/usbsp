#include "../usb_webusb_cmds_internal.h"
#include "../../debug_log.h"
#include "spi_link.h"

uint8_t webusb_cmd_spi_text(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    uint8_t text_len;
    uint8_t max_len = WEBUSB_MAX_PACKET - 3;

    if (len < 2) {
        return 1;
    }

    text_len = buf[1];
    if (text_len > (uint8_t)(len - 2)) {
        text_len = (uint8_t)(len - 2);
    }
    if (text_len > max_len) {
        text_len = max_len;
    }

    resp[0] = WEBUSB_CMD_SPI_TEXT;
    resp[1] = 0x00;
    resp[2] = text_len;
    for (uint8_t i = 0; i < text_len; i++) {
        resp[3 + i] = buf[2 + i];
    }
    *resp_len = (uint8_t)(3 + text_len);

    spi_link_init();
    spi_link_send_text(&buf[2], text_len);
    LOG_DEBUG("SPI text len=%u", text_len);
    return 0;
}
