#include "../usb_webusb_cmds_internal.h"
#include "../../debug_log.h"
#include "spi_link.h"

// Version response layout:
//  [0] cmd
//  [1] status (0=OK)
//  [2] ch572d_fw_major
//  [3] ch572d_fw_minor
//  [4] ch572d_fw_patch
//  [5] ch32v203_fw_major
//  [6] ch32v203_fw_minor
//  [7] ch32v203_fw_patch
//  [8] reserved

#define USBSP_CH572D_FW_MAJOR 0
#define USBSP_CH572D_FW_MINOR 1
#define USBSP_CH572D_FW_PATCH 0

uint8_t webusb_cmd_get_versions(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    (void)buf;
    (void)len;

    if (!resp || !resp_len) {
        return 1;
    }

    {
        uint8_t spi_resp[64];

        spi_link_get_versions(spi_resp, (uint8_t)sizeof(spi_resp));

        resp[0] = WEBUSB_CMD_GET_VERSIONS;
        resp[1] = 0x00;
        resp[2] = USBSP_CH572D_FW_MAJOR;
        resp[3] = USBSP_CH572D_FW_MINOR;
        resp[4] = USBSP_CH572D_FW_PATCH;
        resp[5] = 0x00;
        resp[6] = 0x00;
        resp[7] = 0x00;
        resp[8] = 0x00;

        if (spi_resp[0] == 0x81U) {
            resp[5] = spi_resp[1];
            resp[6] = spi_resp[2];
            resp[7] = spi_resp[3];
        }
        *resp_len = 9;
    }

    LOG_INFO("VERSIONS %u.%u.%u", USBSP_CH572D_FW_MAJOR, USBSP_CH572D_FW_MINOR, USBSP_CH572D_FW_PATCH);
    return 0;
}
