#include "usb_webusb_cmds.h"
#include "usb_webusb_cmds_internal.h"
#include "../debug_log.h"

typedef struct {
    uint8_t cmd;
    webusb_cmd_fn fn;
} webusb_cmd_entry_t;

static const webusb_cmd_entry_t cmd_table[] = {
    { WEBUSB_CMD_EEPROM_READ, webusb_cmd_eeprom_read },
    { WEBUSB_CMD_EEPROM_WRITE, webusb_cmd_eeprom_write },
    { WEBUSB_CMD_CH32_RESET, webusb_cmd_ch32_reset },
    { WEBUSB_CMD_GET_VERSIONS, webusb_cmd_get_versions },
    { WEBUSB_CMD_GET_ACTIVE_USB_INFO, webusb_cmd_get_active_usb_info },
    { WEBUSB_CMD_CAPTURE_POLL, webusb_cmd_capture_poll },
    { WEBUSB_CMD_SPI_TEXT, webusb_cmd_spi_text },
    { WEBUSB_CMD_SPI_PINGPONG, webusb_cmd_spi_pingpong },
};

static usb_webusb_send_fn send_resp = 0;

void usb_webusb_cmds_init(usb_webusb_send_fn send_fn)
{
    send_resp = send_fn;
}

void usb_webusb_cmds_handle(const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t resp[WEBUSB_MAX_PACKET];
    uint8_t resp_len = 0;
    uint8_t rc;
    uint8_t suppress_log;

    if (!buf || len == 0 || !send_resp) {
        return;
    }

    suppress_log = (buf[0] == WEBUSB_CMD_CAPTURE_POLL) ? 1U : 0U;
    if (!suppress_log) {
        LOG_DEBUG("WEBUSB rx cmd=%02X len=%u", buf[0], len);
    }

    for (i = 0; i < (uint8_t)(sizeof(cmd_table) / sizeof(cmd_table[0])); i++) {
        if (cmd_table[i].cmd == buf[0]) {
            rc = cmd_table[i].fn(buf, len, resp, &resp_len);
            if (!suppress_log) {
                LOG_DEBUG("WEBUSB tx cmd=%02X rc=%u resp_len=%u", buf[0], rc, resp_len);
            }
            if (rc == 0 && resp_len) {
                send_resp(resp, resp_len);
            }
            return;
        }
    }

    LOG_WARN("WEBUSB unknown cmd=%02X", buf[0]);
}
