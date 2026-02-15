#include "spi_link.h"
#include "CH57x_common.h"

#define SPI_LINK_TEST_LEN 22

__attribute__((aligned(4))) static uint8_t spi_test_msg[SPI_LINK_TEST_LEN] =
    "SPI_TEST_CH572_TO_CH32";

void spi_link_init(void)
{
    GPIOA_SetBits(GPIO_Pin_4);
    GPIOA_ModeCfg(GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7, GPIO_ModeOut_PP_5mA);
    GPIOA_ModeCfg(GPIO_Pin_6, GPIO_ModeIN_Floating);
    SPI_MasterDefInit();
}

void spi_link_send_test(void)
{
    GPIOA_ResetBits(GPIO_Pin_4);
    SPI_MasterDMATrans(spi_test_msg, SPI_LINK_TEST_LEN);
    GPIOA_SetBits(GPIO_Pin_4);
}

void spi_link_send_text(const uint8_t *text, uint8_t len)
{
    uint8_t buf[SPI_LINK_TEST_LEN];
    uint8_t copy_len = len;

    if (copy_len > SPI_LINK_TEST_LEN) {
        copy_len = SPI_LINK_TEST_LEN;
    }

    for (uint8_t i = 0; i < SPI_LINK_TEST_LEN; i++) {
        buf[i] = 0x00;
    }
    for (uint8_t i = 0; i < copy_len; i++) {
        buf[i] = text[i];
    }

    GPIOA_ResetBits(GPIO_Pin_4);
    SPI_MasterDMATrans(buf, SPI_LINK_TEST_LEN);
    GPIOA_SetBits(GPIO_Pin_4);
}

void spi_link_pingpong(uint8_t *rx, uint8_t len)
{
    uint8_t rx_len = len;

    if (rx_len > SPI_LINK_TEST_LEN) {
        rx_len = SPI_LINK_TEST_LEN;
    }

    GPIOA_ResetBits(GPIO_Pin_4);
    SPI_MasterDMATrans(spi_test_msg, SPI_LINK_TEST_LEN);
    DelayMs(1);
    SPI_MasterDMARecv(rx, rx_len);
    GPIOA_SetBits(GPIO_Pin_4);
}
