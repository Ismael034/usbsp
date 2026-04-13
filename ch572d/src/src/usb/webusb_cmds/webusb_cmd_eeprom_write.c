#include "../usb_webusb_cmds_internal.h"
#include "../../debug_log.h"
#include "eeprom/eeprom.h"
#include <string.h>

#define EEPROM_SIZE 256

uint8_t webusb_cmd_eeprom_write(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    uint16_t addr;
    uint8_t chunk_len;
    uint8_t verify[60];
    uint8_t status = 0;
    uint8_t err = 0; /* 0=OK, 1=BAD_ARGS, 2=RANGE, 3=I2C_WRITE, 4=VERIFY */

    if (!buf || !resp || !resp_len) {
        return 1;
    }

    LOG_DEBUG("eeprom: write rx len=%u", len);

    if (len < 4) {
        status = 1;
        err = 1;
        goto done;
    }

    addr = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
    chunk_len = buf[3];

    if ((uint16_t)len != (uint16_t)(4u + chunk_len)) {
        status = 1;
        err = 1;
        goto done;
    }

    if ((addr >= EEPROM_SIZE) || ((uint16_t)addr + (uint16_t)chunk_len > EEPROM_SIZE)) {
        status = 1;
        err = 2;
        goto done;
    }

    LOG_DEBUG("eeprom: chunk addr=%u bytes=%u", (unsigned)addr, (unsigned)chunk_len);

    AT24C02_bus_claim();
    AT24C02_init();

    if (chunk_len != 0u) {
        if (AT24C02_write(addr, (uint8_t *)&buf[4], chunk_len)) {
            status = 1;
            err = 3;
            goto release_and_done;
        }

        memset(verify, 0, sizeof(verify));
        AT24C02_read(addr, verify, chunk_len);
        if (memcmp(verify, &buf[4], chunk_len) != 0) {
            status = 1;
            err = 4;
            goto release_and_done;
        }
    }

    LOG_INFO("eeprom: write OK addr=%u bytes=%u", (unsigned)addr, (unsigned)chunk_len);

release_and_done:
    AT24C02_bus_release();

done:
    resp[0] = WEBUSB_CMD_EEPROM_WRITE;
    resp[1] = status;
    resp[2] = (uint8_t)(addr & 0xFFu);
    resp[3] = err;
    *resp_len = 4;

    if (status != 0u) {
        LOG_ERROR("eeprom: write failed err=%u addr=%u bytes=%u", (unsigned)err, (unsigned)addr, (unsigned)chunk_len);
    }
    return status;
}
