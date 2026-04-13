#include "../usb_webusb_cmds_internal.h"
#include "spi_link.h"

uint8_t webusb_cmd_capture_poll(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    (void)buf;
    (void)len;

    if (!resp || !resp_len) {
        return 1;
    }

    spi_link_capture_read(resp, WEBUSB_MAX_PACKET);
    *resp_len = WEBUSB_MAX_PACKET;
    return 0;
}
