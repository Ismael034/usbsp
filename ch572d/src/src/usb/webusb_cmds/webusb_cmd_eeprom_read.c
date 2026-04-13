#include "../usb_webusb_cmds_internal.h"
#include "../../debug_log.h"
#include "eeprom/eeprom.h"

uint8_t webusb_cmd_eeprom_read(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    uint8_t read_len;

    if (len < 3) {
        return 1;
    }

    read_len = buf[2];
    if (read_len > (WEBUSB_MAX_PACKET - 3)) {
        read_len = WEBUSB_MAX_PACKET - 3;
    }

    AT24C02_bus_claim();
    AT24C02_init();

    resp[0] = WEBUSB_CMD_EEPROM_READ;
    resp[1] = 0x00;
    resp[2] = read_len;
    AT24C02_read(buf[1], &resp[3], read_len);
    AT24C02_bus_release();
    LOG_DEBUG("EEPROM read a=%u l=%u", buf[1], read_len);
    *resp_len = (uint8_t)(3 + read_len);
    return 0;
}
