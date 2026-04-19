#include "spi_link.h"
#include "CH57x_common.h"
#include <string.h>

#define SPI_CMD_PINGPONG 0x03U
#define SPI_CMD_GET_VERSIONS 0x10U
#define SPI_CMD_CAPTURE_POLL 0x20U

__attribute__((aligned(4))) static uint8_t spi_ping_req[SPI_LINK_FRAME_LEN];
__attribute__((aligned(4))) static uint8_t spi_versions_req[SPI_LINK_FRAME_LEN];
__attribute__((aligned(4))) static uint8_t spi_capture_req[SPI_LINK_FRAME_LEN];

static void spi_link_exchange(const uint8_t *tx, uint8_t *rx, uint8_t len, uint16_t turnaround_us)
{
    GPIOA_ResetBits(GPIO_Pin_4);
    DelayUs(2);
    for (uint8_t i = 0; i < len; i++) {
        SPI_MasterSendByte(tx ? tx[i] : 0U);
    }
    GPIOA_SetBits(GPIO_Pin_4);

    DelayUs(turnaround_us);

    GPIOA_ResetBits(GPIO_Pin_4);
    DelayUs(2);
    for (uint8_t i = 0; i < len; i++) {
        rx[i] = SPI_MasterRecvByte();
    }
    GPIOA_SetBits(GPIO_Pin_4);
}

void spi_link_init(void)
{
    memset(spi_ping_req, 0, sizeof(spi_ping_req));
    memset(spi_versions_req, 0, sizeof(spi_versions_req));
    memset(spi_capture_req, 0, sizeof(spi_capture_req));
    spi_ping_req[0] = SPI_CMD_PINGPONG;
    spi_versions_req[0] = SPI_CMD_GET_VERSIONS;
    spi_capture_req[0] = SPI_CMD_CAPTURE_POLL;

    GPIOA_SetBits(GPIO_Pin_4);
    GPIOA_ModeCfg(GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7, GPIO_ModeOut_PP_20mA);
    GPIOA_ModeCfg(GPIO_Pin_6, GPIO_ModeIN_Floating);
    SPI_MasterDefInit();
    SPI_DataMode(Mode0_HighBitINFront);
    SPI_CLKCfg(2);
}

void spi_link_send_test(void)
{
    uint8_t rx[SPI_LINK_FRAME_LEN];
    spi_link_exchange(spi_ping_req, rx, SPI_LINK_FRAME_LEN, 10);
}

void spi_link_send_text(const uint8_t *text, uint8_t len)
{
    uint8_t buf[SPI_LINK_FRAME_LEN];
    uint8_t rx[SPI_LINK_FRAME_LEN];
    uint8_t copy_len = len;

    if (copy_len > SPI_LINK_FRAME_LEN) {
        copy_len = SPI_LINK_FRAME_LEN;
    }

    memset(buf, 0, sizeof(buf));
    memcpy(buf, text, copy_len);
    spi_link_exchange(buf, rx, SPI_LINK_FRAME_LEN, 10);
}

void spi_link_pingpong(uint8_t *rx, uint8_t len)
{
    uint8_t rx_len = len;

    if (rx_len > SPI_LINK_FRAME_LEN) {
        rx_len = SPI_LINK_FRAME_LEN;
    }

    memset(rx, 0, rx_len);
    spi_link_exchange(spi_ping_req, rx, rx_len, 10);
}

void spi_link_get_versions(uint8_t *rx, uint8_t len)
{
    uint8_t rx_len = len;

    if (rx_len > SPI_LINK_FRAME_LEN) {
        rx_len = SPI_LINK_FRAME_LEN;
    }

    memset(rx, 0, rx_len);
    spi_link_exchange(spi_versions_req, rx, rx_len, 10);
}

void spi_link_get_active_usb_info(uint16_t offset, uint8_t req_len, uint8_t *rx, uint8_t len)
{
    uint8_t req[SPI_LINK_FRAME_LEN];
    uint8_t rx_len = len;

    if (rx_len > SPI_LINK_FRAME_LEN) {
        rx_len = SPI_LINK_FRAME_LEN;
    }
    if (req_len > (SPI_LINK_FRAME_LEN - 6U)) {
        req_len = SPI_LINK_FRAME_LEN - 6U;
    }

    memset(req, 0, sizeof(req));
    req[0] = 0x11U;
    req[1] = (uint8_t)(offset & 0xFFU);
    req[2] = (uint8_t)((offset >> 8) & 0xFFU);
    req[3] = req_len;

    memset(rx, 0, rx_len);
    spi_link_exchange(req, rx, rx_len, 1000);
}

void spi_link_capture_read(uint8_t *rx, uint8_t len)
{
    uint8_t rx_len = len;

    if (rx_len > SPI_LINK_FRAME_LEN) {
        rx_len = SPI_LINK_FRAME_LEN;
    }

    memset(rx, 0, rx_len);
    spi_link_exchange(spi_capture_req, rx, rx_len, 10);
}
