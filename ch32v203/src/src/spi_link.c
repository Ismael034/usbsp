#include "spi_link.h"

#include "ch32v20x_conf.h"
#include "debug_log.h"
#include "eeprom.h"
#include "user.h"
#include <string.h>

#define SPI_LINK_FRAME_LEN 64U
#define SPI_CMD_GET_VERSIONS 0x10U
#define SPI_CMD_PINGPONG 0x03U
#define SPI_CMD_GET_ACTIVE_USB_INFO 0x11U
#define SPI_CMD_CAPTURE_POLL 0x20U
#define SPI_VERSION_MAGIC 0x81U
#define SPI_CAPTURE_MAGIC 0xC2U
#define SPI_USB_INFO_MAGIC 0x83U
#define SPI_USB_INFO_HEADER_LEN 6U
#define SPI_CAPTURE_RECORD_HEADER_LEN 3U
#define SPI_CAPTURE_PAYLOAD_MAX (SPI_LINK_FRAME_LEN - 4U - SPI_CAPTURE_RECORD_HEADER_LEN)
#define SPI_CAPTURE_PAYLOAD_INT_MAX SPI_CAPTURE_PAYLOAD_MAX
#define SPI_CAPTURE_PAYLOAD_INT_MIN 8U
#define SPI_CAPTURE_PAYLOAD_OTHER_MAX SPI_CAPTURE_PAYLOAD_MAX
#define SPI_CAPTURE_PAYLOAD_OTHER_MIN 4U
#define SPI_CAPTURE_PAYLOAD_BULK_MAX SPI_CAPTURE_PAYLOAD_MAX
#define SPI_CAPTURE_PAYLOAD_BULK_MID 4U
#define SPI_CAPTURE_PAYLOAD_BULK_MIN 0U
#define SPI_CAPTURE_RING_SIZE 1024U

static const uint8_t spi_ping_rsp[] = "SPI_PONG_CH32_TO_CH572";

static volatile uint8_t spi_rx_buf[SPI_LINK_FRAME_LEN];
static volatile uint8_t spi_rx_count = 0U;
static volatile uint8_t spi_tx_buf[SPI_LINK_FRAME_LEN];
static volatile uint8_t spi_tx_index = 0U;
static volatile uint8_t spi_reply_active = 0U;
static uint8_t spi_usb_info_buf[256];

static volatile uint8_t spi_capture_ring[SPI_CAPTURE_RING_SIZE];
static volatile uint16_t spi_capture_head = 0U;
static volatile uint16_t spi_capture_tail = 0U;
static volatile uint16_t spi_capture_used = 0U;
static volatile uint16_t spi_capture_dropped = 0U;
static volatile uint8_t spi_capture_seq = 0U;

void SPI1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI4_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static void spi_link_prime_idle_byte(void)
{
    SPI_I2S_SendData(SPI1, 0x00);
}

static void spi_link_prime_reply(void)
{
    spi_reply_active = 1U;
    spi_tx_index = 1U;
    SPI_I2S_SendData(SPI1, spi_tx_buf[0]);
}

static void spi_link_send_next_byte(void)
{
    if (spi_reply_active != 0U && spi_tx_index < SPI_LINK_FRAME_LEN) {
        SPI_I2S_SendData(SPI1, spi_tx_buf[spi_tx_index]);
        spi_tx_index++;
        if (spi_tx_index >= SPI_LINK_FRAME_LEN) {
            spi_reply_active = 0U;
        }
        return;
    }

    spi_link_prime_idle_byte();
}

static void spi_link_prepare_pingpong_reply(void)
{
    memset((void *)spi_tx_buf, 0, sizeof(spi_tx_buf));
    memcpy((void *)spi_tx_buf, spi_ping_rsp, sizeof(spi_ping_rsp) - 1U);
}

static void spi_link_prepare_version_reply(void)
{
    memset((void *)spi_tx_buf, 0, sizeof(spi_tx_buf));
    spi_tx_buf[0] = SPI_VERSION_MAGIC;
    spi_tx_buf[1] = 0U;
    spi_tx_buf[2] = 1U;
    spi_tx_buf[3] = 0U;
}

static void spi_link_prepare_usb_info_reply(void)
{
    uint16_t total_len = 0U;
    uint16_t offset = (uint16_t)spi_rx_buf[1] | ((uint16_t)spi_rx_buf[2] << 8);
    uint8_t req_len = spi_rx_buf[3];
    uint8_t copy_len = 0U;

    memset((void *)spi_tx_buf, 0, sizeof(spi_tx_buf));

    if (user_build_current_tlv_image(spi_usb_info_buf, (uint16_t)sizeof(spi_usb_info_buf), &total_len) != 0U) {
        return;
    }

    if (req_len == 0U || req_len > (SPI_LINK_FRAME_LEN - SPI_USB_INFO_HEADER_LEN)) {
        req_len = (uint8_t)(SPI_LINK_FRAME_LEN - SPI_USB_INFO_HEADER_LEN);
    }

    if (offset < total_len) {
        uint16_t remain = (uint16_t)(total_len - offset);
        copy_len = (remain > req_len) ? req_len : (uint8_t)remain;
        if (copy_len > (SPI_LINK_FRAME_LEN - SPI_USB_INFO_HEADER_LEN)) {
            copy_len = (uint8_t)(SPI_LINK_FRAME_LEN - SPI_USB_INFO_HEADER_LEN);
        }
    }

    spi_tx_buf[0] = SPI_USB_INFO_MAGIC;
    spi_tx_buf[1] = (uint8_t)(total_len & 0xFFU);
    spi_tx_buf[2] = (uint8_t)((total_len >> 8) & 0xFFU);
    spi_tx_buf[3] = copy_len;
    spi_tx_buf[4] = (uint8_t)(offset & 0xFFU);
    spi_tx_buf[5] = (uint8_t)((offset >> 8) & 0xFFU);
    if (copy_len != 0U) {
        memcpy((void *)&spi_tx_buf[SPI_USB_INFO_HEADER_LEN], &spi_usb_info_buf[offset], copy_len);
    }
}

static void spi_link_prepare_capture_reply(void)
{
    uint8_t out_index = 4U;
    uint8_t packed = 0U;
    uint16_t local_head = spi_capture_head;
    uint16_t local_used = spi_capture_used;
    uint16_t dropped = spi_capture_dropped;
    uint8_t dropped_report = (dropped > 0xFFU) ? 0xFFU : (uint8_t)dropped;
    uint8_t meta;
    uint8_t len;
    uint8_t original_len;
    uint16_t rec_len;

    memset((void *)spi_tx_buf, 0, sizeof(spi_tx_buf));

    if (local_used == 0U && dropped == 0U) {
        return;
    }

    spi_tx_buf[0] = SPI_CAPTURE_MAGIC;
    spi_tx_buf[1] = spi_capture_seq++;
    spi_tx_buf[2] = dropped_report;
    spi_capture_dropped = (uint16_t)(dropped - dropped_report);

    while (local_used >= SPI_CAPTURE_RECORD_HEADER_LEN) {
        meta = spi_capture_ring[local_head];
        len = spi_capture_ring[(uint16_t)((local_head + 1U) % SPI_CAPTURE_RING_SIZE)];
        original_len = spi_capture_ring[(uint16_t)((local_head + 2U) % SPI_CAPTURE_RING_SIZE)];
        rec_len = (uint16_t)(SPI_CAPTURE_RECORD_HEADER_LEN + len);

        if (local_used < rec_len) {
            break;
        }

        if ((uint16_t)out_index + rec_len > SPI_LINK_FRAME_LEN) {
            break;
        }

        spi_tx_buf[out_index++] = meta;
        spi_tx_buf[out_index++] = len;
        spi_tx_buf[out_index++] = original_len;
        for (uint8_t i = 0U; i < len; i++) {
            uint16_t src = (uint16_t)((local_head + SPI_CAPTURE_RECORD_HEADER_LEN + i) % SPI_CAPTURE_RING_SIZE);
            spi_tx_buf[out_index++] = spi_capture_ring[src];
        }

        local_head = (uint16_t)((local_head + rec_len) % SPI_CAPTURE_RING_SIZE);
        local_used = (uint16_t)(local_used - rec_len);
        packed++;
    }

    spi_tx_buf[3] = packed;
    spi_capture_head = local_head;
    spi_capture_used = local_used;
}

static uint8_t spi_link_capture_limit(uint8_t endpoint_type, uint16_t len, uint16_t used)
{
    uint16_t limit = SPI_CAPTURE_PAYLOAD_MAX;
    uint8_t transfer_type = endpoint_type & 0x03U;

    if (transfer_type == 0x02U) {
        if (limit > SPI_CAPTURE_PAYLOAD_BULK_MAX) {
            limit = SPI_CAPTURE_PAYLOAD_BULK_MAX;
        }
        if (used >= (SPI_CAPTURE_RING_SIZE * 3U / 4U)) {
            if (limit > SPI_CAPTURE_PAYLOAD_BULK_MIN) {
                limit = SPI_CAPTURE_PAYLOAD_BULK_MIN;
            }
        } else if (used >= (SPI_CAPTURE_RING_SIZE / 2U)) {
            if (limit > SPI_CAPTURE_PAYLOAD_BULK_MID) {
                limit = SPI_CAPTURE_PAYLOAD_BULK_MID;
            }
        }
    } else if (transfer_type == 0x03U) {
        if (limit > SPI_CAPTURE_PAYLOAD_INT_MAX) {
            limit = SPI_CAPTURE_PAYLOAD_INT_MAX;
        }
        if (used >= (SPI_CAPTURE_RING_SIZE * 3U / 4U) && limit > SPI_CAPTURE_PAYLOAD_INT_MIN) {
            limit = SPI_CAPTURE_PAYLOAD_INT_MIN;
        }
    } else {
        if (limit > SPI_CAPTURE_PAYLOAD_OTHER_MAX) {
            limit = SPI_CAPTURE_PAYLOAD_OTHER_MAX;
        }
        if (used >= (SPI_CAPTURE_RING_SIZE * 3U / 4U) && limit > SPI_CAPTURE_PAYLOAD_OTHER_MIN) {
            limit = SPI_CAPTURE_PAYLOAD_OTHER_MIN;
        }
    }

    if (len < limit) {
        limit = len;
    }

    return (uint8_t)limit;
}

void spi_link_capture_packet(uint8_t direction_in, uint8_t endpoint, uint8_t endpoint_type, const uint8_t *data, uint16_t len)
{
    uint8_t copy_len;
    uint8_t original_len;
    uint16_t rec_len;
    uint8_t meta;

    if (endpoint >= 0x80U) {
        endpoint &= 0x7FU;
    }

    __disable_irq();

    copy_len = spi_link_capture_limit(endpoint_type, len, spi_capture_used);
    original_len = (len > 0xFFU) ? 0xFFU : (uint8_t)len;
    rec_len = (uint16_t)(SPI_CAPTURE_RECORD_HEADER_LEN + copy_len);
    meta = (uint8_t)((direction_in != 0U ? 0x80U : 0x00U) | (endpoint & 0x7FU));

    if ((uint16_t)(SPI_CAPTURE_RING_SIZE - spi_capture_used) < rec_len) {
        spi_capture_dropped++;
        __enable_irq();
        return;
    }

    spi_capture_ring[spi_capture_tail] = meta;
    spi_capture_tail = (uint16_t)((spi_capture_tail + 1U) % SPI_CAPTURE_RING_SIZE);
    spi_capture_ring[spi_capture_tail] = copy_len;
    spi_capture_tail = (uint16_t)((spi_capture_tail + 1U) % SPI_CAPTURE_RING_SIZE);
    spi_capture_ring[spi_capture_tail] = original_len;
    spi_capture_tail = (uint16_t)((spi_capture_tail + 1U) % SPI_CAPTURE_RING_SIZE);

    for (uint8_t i = 0U; i < copy_len; i++) {
        spi_capture_ring[spi_capture_tail] = (data != 0) ? data[i] : 0U;
        spi_capture_tail = (uint16_t)((spi_capture_tail + 1U) % SPI_CAPTURE_RING_SIZE);
    }

    spi_capture_used = (uint16_t)(spi_capture_used + rec_len);
    __enable_irq();
}

void spi_link_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    SPI_InitTypeDef spi = {0};
    EXTI_InitTypeDef exti = {0};
    NVIC_InitTypeDef nvic = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_SPI1, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    SPI_I2S_DeInit(SPI1);
    SPI_StructInit(&spi);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Slave;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Hard;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_Init(SPI1, &spi);

    SPI_I2S_ITConfig(SPI1, SPI_I2S_IT_RXNE, ENABLE);
    SPI_Cmd(SPI1, ENABLE);
    spi_link_prime_idle_byte();
    spi_capture_head = 0U;
    spi_capture_tail = 0U;
    spi_capture_used = 0U;
    spi_capture_dropped = 0U;
    spi_capture_seq = 0U;

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource4);
    exti.EXTI_Line = EXTI_Line4;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = SPI1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 1;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    nvic.NVIC_IRQChannel = EXTI4_IRQn;
    NVIC_Init(&nvic);

    LOG_INFO("spi: link ready");
}

void spi_link_task(void)
{
}

void EXTI4_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line4) != RESET) {
        const uint8_t cs_high = (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) == Bit_SET) ? 1U : 0U;

        EXTI_ClearITPendingBit(EXTI_Line4);

        if (cs_high != 0U) {
            uint8_t started_reply = 0U;

            if (spi_rx_count != 0U) {
                if (spi_rx_buf[0] == SPI_CMD_GET_VERSIONS) {
                    spi_link_prepare_version_reply();
                    spi_link_prime_reply();
                    started_reply = 1U;
                } else if (spi_rx_buf[0] == SPI_CMD_PINGPONG) {
                    spi_link_prepare_pingpong_reply();
                    spi_link_prime_reply();
                    started_reply = 1U;
                } else if (spi_rx_buf[0] == SPI_CMD_GET_ACTIVE_USB_INFO) {
                    spi_link_prepare_usb_info_reply();
                    spi_link_prime_reply();
                    started_reply = 1U;
                } else if (spi_rx_buf[0] == SPI_CMD_CAPTURE_POLL) {
                    spi_link_prepare_capture_reply();
                    spi_link_prime_reply();
                    started_reply = 1U;
                }
            }

            spi_rx_count = 0U;
            memset((void *)spi_rx_buf, 0, sizeof(spi_rx_buf));
            if (started_reply == 0U) {
                spi_tx_index = 0U;
                spi_reply_active = 0U;
                spi_link_prime_idle_byte();
            }
        } else if (spi_reply_active == 0U) {
            spi_rx_count = 0U;
            spi_tx_index = 0U;
            spi_reply_active = 0U;
            spi_link_prime_idle_byte();
        }
    }
}

void SPI1_IRQHandler(void)
{
    if (SPI_I2S_GetITStatus(SPI1, SPI_I2S_IT_RXNE) != RESET) {
        const uint8_t rx = (uint8_t)SPI_I2S_ReceiveData(SPI1);

        if (spi_reply_active == 0U && spi_rx_count < SPI_LINK_FRAME_LEN) {
            spi_rx_buf[spi_rx_count] = rx;
            spi_rx_count++;
        }

        spi_link_send_next_byte();
    }

    if (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_OVR) != RESET) {
        (void)SPI1->DATAR;
        (void)SPI1->STATR;
    }
}
