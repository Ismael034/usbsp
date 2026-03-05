#include "../usb_webusb_cmds_internal.h"
#include "../../debug_log.h"
#include "eeprom/eeprom.h"
#include "external/microtlv/tlv.h"
#include <string.h>

#define EEPROM_SIZE 256
#define EEPROM_PAGE 8

// Validates stream and returns:
// - out_count: TLV record count (excluding terminator)
// - out_len_no_term: bytes before optional terminator (or full len if no terminator)
// Returns 0 on success, 1 on malformed stream.
static uint8_t tlv_validate_and_measure(const uint8_t *buf,
                                        uint16_t len,
                                        uint8_t *out_count,
                                        uint16_t *out_len_no_term)
{
    uint8_t *p = (uint8_t *)buf;
    uint32_t left = len;
    uint8_t c = 0;

    if (!buf || !out_count || !out_len_no_term) {
        return 1;
    }

    while (left > 0) {
        uint8_t *rec = p;
        uint32_t rec_left = left;
        uint32_t t = 0;
        uint32_t sz = 0;
        uint8_t *v = 0;
        int rc = tlv_parse(&p, &left, &t, &sz, &v);

        (void)v;
        if (rc != TLV_RESULT_SUCCESS) {
            return 1;
        }

        if (t == 0 && sz == 0) {
            *out_count = c;
            *out_len_no_term = (uint16_t)(rec - buf);
            return 0;
        }

        if (t > 255) {
            return 1;
        }

        if (left >= rec_left) {
            return 1;
        }

        c++;
    }

    *out_count = c;
    *out_len_no_term = len;
    return 0;
}

uint8_t webusb_cmd_eeprom_write(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    uint8_t orig[EEPROM_SIZE];
    uint8_t newimg[EEPROM_SIZE];
    uint8_t applied_count = 0;
    uint8_t status = 0;
    uint8_t err = 0; // 0=OK, 1=BAD_TLV, 2=NO_SPACE, 3=I2C_WRITE, 4=VERIFY

    uint16_t write_len = 0;
    uint8_t verify_page[EEPROM_PAGE];
    uint8_t pages_written = 0;

    if (!buf || !resp || !resp_len) {
        return 1;
    }

    LOG_DEBUG("EEPROM write rx len=%u", len);

    if (len <= 1) {
        resp[0] = WEBUSB_CMD_EEPROM_WRITE;
        resp[1] = 1;
        resp[2] = 0;
        resp[3] = 1;
        *resp_len = 4;
        return 1;
    }

    if (tlv_validate_and_measure(&buf[1], (uint16_t)(len - 1), &applied_count, &write_len)) {
        status = 1;
        err = 1;
        goto done;
    }

    if ((uint32_t)write_len + 1u > EEPROM_SIZE) {
        status = 1;
        applied_count = 0;
        err = 2;
        goto done;
    }

    memset(orig, 0xFF, sizeof(orig));
    AT24C02_read(0x00, orig, EEPROM_SIZE);

    memset(newimg, 0xFF, sizeof(newimg));
    if (write_len) {
        memcpy(newimg, &buf[1], write_len);
    }
    {
        uint8_t *w = &newimg[write_len];
        uint32_t wleft = (uint32_t)EEPROM_SIZE - (uint32_t)write_len;
        if (tlv_format(&w, &wleft, 0, 0, 0) != TLV_RESULT_SUCCESS) {
            status = 1;
            applied_count = 0;
            err = 2;
            goto done;
        }
    }
    LOG_DEBUG("EEPROM write bytes=%u", write_len);

    {
        uint16_t addr;
        for (addr = 0; addr < EEPROM_SIZE; addr += EEPROM_PAGE) {
            if (memcmp(&orig[addr], &newimg[addr], EEPROM_PAGE) != 0) {
                if (AT24C02_write(addr, &newimg[addr], EEPROM_PAGE)) {
                    status = 1;
                    applied_count = 0;
                    err = 3;
                    goto done;
                }
                pages_written++;
                AT24C02_read(addr, verify_page, EEPROM_PAGE);
                if (memcmp(verify_page, &newimg[addr], EEPROM_PAGE) != 0) {
                    status = 1;
                    applied_count = 0;
                    err = 4;
                    goto done;
                }
            }
        }
    }
    LOG_INFO("EEPROM write ok pages=%u", pages_written);

done:
    resp[0] = WEBUSB_CMD_EEPROM_WRITE;
    resp[1] = status;
    resp[2] = applied_count;
    resp[3] = err;
    *resp_len = 4;

    if (status) {
        LOG_ERROR("EEPROM write err=%u pages=%u", err, pages_written);
    }
    return status;
}
