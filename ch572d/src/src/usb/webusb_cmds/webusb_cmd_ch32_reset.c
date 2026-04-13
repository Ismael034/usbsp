#include "../usb_webusb_cmds_internal.h"
#include "../../debug_log.h"
#include "CH57x_common.h"

#define CH32_RESET_PIN GPIO_Pin_9

static void ch32_reset_release(void)
{
    GPIOA_ModeCfg(CH32_RESET_PIN, GPIO_ModeIN_Floating);
}

static void ch32_reset_assert(void)
{
    GPIOA_ResetBits(CH32_RESET_PIN);
    GPIOA_ModeCfg(CH32_RESET_PIN, GPIO_ModeOut_PP_5mA);
}

uint8_t webusb_cmd_ch32_reset(const uint8_t *buf, uint8_t len, uint8_t *resp, uint8_t *resp_len)
{
    (void)buf;
    (void)len;

    if (!resp || !resp_len) {
        return 1;
    }

    ch32_reset_assert();
    DelayMs(10);
    ch32_reset_release();

    resp[0] = WEBUSB_CMD_CH32_RESET;
    resp[1] = 0x00;
    *resp_len = 2;

    LOG_INFO("ch32: reset pulse");
    return 0;
}
