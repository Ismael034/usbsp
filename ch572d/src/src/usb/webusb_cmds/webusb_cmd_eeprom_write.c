#include "../usb_webusb_cmds_internal.h"
#include "../../debug_log.h"
#include "eeprom/eeprom.h"
#include <string.h>

// EEPROM is treated as a TLV store.
// Encoding: <size: uint16_le><type: uint8><value[size]>...
// Terminator: <0x0000><0x00> (3 bytes)
//
// Update strategy (bounded growth):
// - Parse existing store and incoming TLVs.
// - Rebuild a compact store containing at most 1 record per type (incoming overrides existing).
// - Write only EEPROM pages (8 bytes) that actually changed.
//
// Request:  [0]=CMD, [1..]=TLV records
// Response: [0]=CMD, [1]=status (0=OK, 1=ERR), [2]=applied_count, [3]=err_code

#define EEPROM_SIZE 256
#define TLV_HDR_LEN 3
#define EEPROM_PAGE 8

static uint16_t rd_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void wr_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static uint8_t tlv_is_terminator(uint16_t sz, uint8_t t)
{
    return (sz == 0 && t == 0) ? 1 : 0;
}

// Collect last-seen TLV record per type from a TLV stream.
// rec_off/rec_len store the record start and record length (hdr+value).
// Returns 0 on success, 1 on error (malformed stream).
static uint8_t tlv_collect_latest(const uint8_t *buf,
                                  uint16_t len,
                                  uint16_t rec_off[256],
                                  uint16_t rec_len[256],
                                  uint8_t present[256],
                                  uint8_t stop_on_terminator,
                                  uint8_t skip_tombstones)
{
    uint16_t i = 0;

    if (!buf || !rec_off || !rec_len || !present) {
        return 1;
    }

    memset(present, 0, 256);
    while (i + TLV_HDR_LEN <= len) {
        uint16_t sz = rd_u16_le(&buf[i]);
        uint8_t t = buf[i + 2];

        if (stop_on_terminator && tlv_is_terminator(sz, t)) {
            return 0;
        }

        i += TLV_HDR_LEN;
        if (i + sz > len) {
            return 1;
        }

        // Tombstones are stored as type 0xFF in EEPROM. Never carry them forward.
        if (!(skip_tombstones && t == 0xFF)) {
            present[t] = 1;
            rec_off[t] = (uint16_t)(i - TLV_HDR_LEN);
            rec_len[t] = (uint16_t)(TLV_HDR_LEN + sz);
        }

        i = (uint16_t)(i + sz);
    }

    return (i == len) ? 0 : 1;
}

// Validate a TLV stream inside a byte array.
// Returns 0 on success, 1 on error.
static uint8_t tlv_validate_stream(const uint8_t *buf, uint16_t len, uint8_t allow_terminator)
{
    uint16_t i = 0;

    while (i + TLV_HDR_LEN <= len) {
        uint16_t sz = rd_u16_le(&buf[i]);
        uint8_t t = buf[i + 2];

        if (allow_terminator && tlv_is_terminator(sz, t)) {
            return 0;
        }

        i += TLV_HDR_LEN;
        if (i + sz > len) {
            return 1;
        }
        i += sz;
    }

    return (i == len) ? 0 : 1;
}

static void tlv_collect_types(const uint8_t *buf, uint16_t len, uint8_t types[256], uint8_t *count)
{
    uint16_t i = 0;
    uint8_t c = 0;

    memset(types, 0, 256);

    while (i + TLV_HDR_LEN <= len) {
        uint16_t sz = rd_u16_le(&buf[i]);
        uint8_t t = buf[i + 2];

        if (tlv_is_terminator(sz, t)) {
            break;
        }

        types[t] = 1;
        c++;
        i = (uint16_t)(i + TLV_HDR_LEN + sz);
    }

    if (count) {
        *count = c;
    }
}

// Returns the length (in bytes) of TLVs up to, but not including, an optional terminator.
static uint16_t tlv_stream_len_no_term(const uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;

    while (i + TLV_HDR_LEN <= len) {
        uint16_t sz = rd_u16_le(&buf[i]);
        uint8_t t = buf[i + 2];

        if (tlv_is_terminator(sz, t)) {
            return i;
        }

        i += TLV_HDR_LEN;
        if (i + sz > len) {
            return 0xFFFF;
        }
        i += sz;
    }

    return (i == len) ? i : 0xFFFF;
}

uint8_t webusb_cmd_eeprom_write(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    uint8_t orig[EEPROM_SIZE];
    uint8_t newimg[EEPROM_SIZE];
    uint16_t e_off[256];
    uint16_t e_len[256];
    uint8_t e_present[256];
    uint16_t r_off[256];
    uint16_t r_len[256];
    uint8_t r_present[256];
    uint8_t applied_count = 0;
    uint8_t status = 0;
    uint8_t err = 0; // 0=OK, 1=BAD_TLV, 2=NO_SPACE, 3=I2C_WRITE, 4=VERIFY

    uint16_t req_len_no_term;
    uint8_t verify_page[EEPROM_PAGE];
    uint16_t out_off;
    uint16_t t;
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

    // Validate incoming TLVs.
    if (tlv_validate_stream(&buf[1], (uint16_t)(len - 1), 1)) {
        status = 1;
        err = 1;
        goto done;
    }

    tlv_collect_types(&buf[1], (uint16_t)(len - 1), r_present, &applied_count);

    req_len_no_term = tlv_stream_len_no_term(&buf[1], (uint16_t)(len - 1));
    if (req_len_no_term == 0xFFFF) {
        status = 1;
        applied_count = 0;
        err = 1;
        goto done;
    }

    // Read current store.
    memset(orig, 0xFF, sizeof(orig));
    AT24C02_read(0x00, orig, EEPROM_SIZE);

    // Parse EEPROM store (best-effort). If malformed, treat as empty.
    if (tlv_collect_latest(orig, EEPROM_SIZE, e_off, e_len, e_present, 1, 1)) {
        memset(e_present, 0, 256);
    }

    // Parse incoming TLVs (no tombstones, stop on optional terminator).
    if (tlv_collect_latest(&buf[1], (uint16_t)(len - 1), r_off, r_len, r_present, 1, 0)) {
        status = 1;
        applied_count = 0;
        err = 1;
        goto done;
    }

    // Rebuild a compact TLV store: incoming overrides existing (1 record per type).
    memset(newimg, 0xFF, sizeof(newimg));
    out_off = 0;
    for (t = 0; t < 256; t++) {
        const uint8_t *src = NULL;
        uint16_t slen = 0;

        if (r_present[t]) {
            src = &buf[1 + r_off[t]];
            slen = r_len[t];
        } else if (e_present[t]) {
            src = &orig[e_off[t]];
            slen = e_len[t];
        } else {
            continue;
        }

        if ((uint32_t)out_off + (uint32_t)slen + (uint32_t)TLV_HDR_LEN > EEPROM_SIZE) {
            status = 1;
            applied_count = 0;
            err = 2;
            goto done;
        }
        memcpy(&newimg[out_off], src, slen);
        out_off = (uint16_t)(out_off + slen);
    }
    // Terminator.
    if ((uint32_t)out_off + (uint32_t)TLV_HDR_LEN > EEPROM_SIZE) {
        status = 1;
        applied_count = 0;
        err = 2;
        goto done;
    }
    wr_u16_le(&newimg[out_off], 0);
    newimg[out_off + 2] = 0;
    LOG_DEBUG("EEPROM compact bytes=%u", out_off);

    // Write only changed EEPROM pages.
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
                // Read-back verify the written page. Full-EEPROM verify is too slow for WebUSB timing.
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
