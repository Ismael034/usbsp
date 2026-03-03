#include "debug.h"
#include "debug_log.h"
#include "eeprom.h"
#include "external/microtlv/tlv.h"
#include <stdlib.h>
#include <string.h>

uint16_t vid = 0;
uint16_t pid = 0;

#define EEPROM_TOTAL_SIZE        256u
#define TLV_TYPE_VID             0x01u
#define TLV_TYPE_PID             0x02u
#define TLV_TYPE_BCD_DEVICE      0x03u
#define TLV_TYPE_MAX_POWER_MA    0x04u
#define TLV_TYPE_FLAGS           0x05u
#define TLV_TYPE_ATTACH_DELAY_MS 0x06u
#define TLV_TYPE_CAPTURE_MAX_B   0x07u
#define TLV_TYPE_MANUFACTURER    0x08u
#define TLV_TYPE_PRODUCT         0x09u
#define TLV_TYPE_SERIAL          0x0Au

#define TLV_FLAG_SELF_POWERED    (1u << 0)
#define TLV_FLAG_REMOTE_WAKEUP   (1u << 1)
#define TLV_FLAG_BOOT_CONNECTED  (1u << 2)
#define TLV_FLAG_CAPTURE_ON_BOOT (1u << 3)

#define TLV_TEXT_MAX_LEN         60u

typedef struct
{
    uint8_t parsed;
    uint8_t record_count;
    uint8_t unknown_count;
    uint8_t has_vid;
    uint8_t has_pid;
    uint8_t has_bcd_device;
    uint8_t has_max_power_ma;
    uint8_t has_flags;
    uint8_t has_attach_delay_ms;
    uint8_t has_capture_max_bytes;
    uint8_t has_manufacturer;
    uint8_t has_product;
    uint8_t has_serial;
    uint16_t vid;
    uint16_t pid;
    uint16_t bcd_device;
    uint16_t max_power_ma;
    uint8_t flags;
    uint16_t attach_delay_ms;
    uint16_t capture_max_bytes;
    char manufacturer[TLV_TEXT_MAX_LEN + 1];
    char product[TLV_TEXT_MAX_LEN + 1];
    char serial[TLV_TEXT_MAX_LEN + 1];
} eeprom_tlv_info_t;

static void tlv_copy_text(char *dst, uint16_t dst_len, const uint8_t *src, uint16_t src_len)
{
    uint16_t i = 0;

    if (!dst || dst_len == 0u) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (i < src_len && (i + 1u) < dst_len && src[i] != '\0') {
        dst[i] = (char)src[i];
        i++;
    }
    dst[i] = '\0';
}

static uint16_t tlv_u16_le(const uint8_t *value)
{
    return (uint16_t)(((uint16_t)value[0]) | ((uint16_t)value[1] << 8));
}

static uint8_t eeprom_parse_tlv(const uint8_t *raw, uint16_t raw_len, eeprom_tlv_info_t *info)
{
    uint8_t *p = (uint8_t *)raw;
    uint32_t left = raw_len;
    uint8_t saw_terminator_or_erased_tail = 0;
    uint8_t seen_known_field = 0;

    if (!raw || !info) {
        return 1;
    }

    memset(info, 0, sizeof(*info));

    while (left > 0u) {
        uint32_t type = 0;
        uint32_t length = 0;
        uint8_t *value = 0;
        int rc;

        if (p[0] == 0xFFu) {
            saw_terminator_or_erased_tail = 1;
            break;
        }

        rc = tlv_parse(&p, &left, &type, &length, &value);
        if (rc != TLV_RESULT_SUCCESS) {
            return 1;
        }
        if (type == 0u && length == 0u) {
            saw_terminator_or_erased_tail = 1;
            break;
        }
        info->record_count++;

        switch (type) {
            case TLV_TYPE_VID:
                if (length >= 2u) {
                    info->vid = tlv_u16_le(value);
                    info->has_vid = 1;
                    seen_known_field = 1;
                }
                break;
            case TLV_TYPE_PID:
                if (length >= 2u) {
                    info->pid = tlv_u16_le(value);
                    info->has_pid = 1;
                    seen_known_field = 1;
                }
                break;
            case TLV_TYPE_BCD_DEVICE:
                if (length >= 2u) {
                    info->bcd_device = tlv_u16_le(value);
                    info->has_bcd_device = 1;
                    seen_known_field = 1;
                }
                break;
            case TLV_TYPE_MAX_POWER_MA:
                if (length >= 2u) {
                    info->max_power_ma = tlv_u16_le(value);
                    info->has_max_power_ma = 1;
                    seen_known_field = 1;
                }
                break;
            case TLV_TYPE_FLAGS:
                if (length >= 1u) {
                    info->flags = value[0];
                    info->has_flags = 1;
                    seen_known_field = 1;
                }
                break;
            case TLV_TYPE_ATTACH_DELAY_MS:
                if (length >= 2u) {
                    info->attach_delay_ms = tlv_u16_le(value);
                    info->has_attach_delay_ms = 1;
                    seen_known_field = 1;
                }
                break;
            case TLV_TYPE_CAPTURE_MAX_B:
                if (length >= 2u) {
                    info->capture_max_bytes = tlv_u16_le(value);
                    info->has_capture_max_bytes = 1;
                    seen_known_field = 1;
                }
                break;
            case TLV_TYPE_MANUFACTURER:
                tlv_copy_text(info->manufacturer, (uint16_t)sizeof(info->manufacturer), value, (uint16_t)length);
                info->has_manufacturer = 1;
                seen_known_field = 1;
                break;
            case TLV_TYPE_PRODUCT:
                tlv_copy_text(info->product, (uint16_t)sizeof(info->product), value, (uint16_t)length);
                info->has_product = 1;
                seen_known_field = 1;
                break;
            case TLV_TYPE_SERIAL:
                tlv_copy_text(info->serial, (uint16_t)sizeof(info->serial), value, (uint16_t)length);
                info->has_serial = 1;
                seen_known_field = 1;
                break;
            default:
                info->unknown_count++;
                break;
        }
    }

    if (!saw_terminator_or_erased_tail || !seen_known_field) {
        return 1;
    }

    info->parsed = 1;
    return 0;
}

static void log_parsed_eeprom_tlv(const eeprom_tlv_info_t *info)
{
    if (!info) {
        return;
    }

    LOG_INFO("EEPROM TLV: records=%u unknown=%u", info->record_count, info->unknown_count);
    if (info->has_vid) LOG_INFO("EEPROM TLV: vid=0x%04X (%u)", info->vid, info->vid);
    if (info->has_pid) LOG_INFO("EEPROM TLV: pid=0x%04X (%u)", info->pid, info->pid);
    if (info->has_bcd_device) LOG_INFO("EEPROM TLV: bcdDevice=0x%04X", info->bcd_device);
    if (info->has_max_power_ma) LOG_INFO("EEPROM TLV: maxPowerMa=%u", info->max_power_ma);
    if (info->has_flags) {
        LOG_INFO("EEPROM TLV: flags=0x%02X", info->flags);
        LOG_INFO("EEPROM TLV: selfPowered=%u remoteWakeup=%u bootConnected=%u captureOnBoot=%u",
                 (info->flags & TLV_FLAG_SELF_POWERED) ? 1u : 0u,
                 (info->flags & TLV_FLAG_REMOTE_WAKEUP) ? 1u : 0u,
                 (info->flags & TLV_FLAG_BOOT_CONNECTED) ? 1u : 0u,
                 (info->flags & TLV_FLAG_CAPTURE_ON_BOOT) ? 1u : 0u);
    }
    if (info->has_attach_delay_ms) LOG_INFO("EEPROM TLV: attachDelayMs=%u", info->attach_delay_ms);
    if (info->has_capture_max_bytes) LOG_INFO("EEPROM TLV: captureMaxBytes=%u", info->capture_max_bytes);
    if (info->has_manufacturer) LOG_INFO("EEPROM TLV: manufacturer=\"%s\"", info->manufacturer);
    if (info->has_product) LOG_INFO("EEPROM TLV: product=\"%s\"", info->product);
    if (info->has_serial) LOG_INFO("EEPROM TLV: serial=\"%s\"", info->serial);
}

/*********************************************************************
 * @fn      i2c_init
 *
 * @brief   Initializes the I2C peripheral.
 *
 * @param   clock_speed - I2C clock speed in Hz
 * @param   own_address - I2C own address
 *
 * @return  none
 */
void i2c_init(uint32_t clock_speed, uint16_t own_address)
{
    GPIO_InitTypeDef gpio_config = {0};
    I2C_InitTypeDef i2c_config = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);

    /* Configure I2C pins as input with pull-up temporarily */
    gpio_config.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    gpio_config.GPIO_Mode = GPIO_Mode_IPU; /* Input with pull-up */
    GPIO_Init(GPIOB, &gpio_config);

    /* Short delay to activate internal pull-ups */
    Delay_Ms(1);

    /* Now reconfigure to alternate function open-drain */
    gpio_config.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    gpio_config.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio_config);

    /* I2C peripheral init */
    i2c_config.I2C_ClockSpeed = clock_speed;
    i2c_config.I2C_Mode = I2C_Mode_I2C;
    i2c_config.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c_config.I2C_OwnAddress1 = own_address;
    i2c_config.I2C_Ack = I2C_Ack_Enable;
    i2c_config.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C2, &i2c_config);

    I2C_Cmd(I2C2, ENABLE);
}

/*********************************************************************
 * @fn      tim2_init
 *
 * @brief   Initializes the I2C Timer.
 *
 * @param   prescaler - Timer prescaler value
 *
 * @return  none
 */
void tim2_init(uint16_t prescaler)
{
    TIM_TimeBaseInitTypeDef timer_config = {0};

    /* Enable timer2 clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* Initialize timer2 */
    timer_config.TIM_Period = 0xFFFF;
    timer_config.TIM_Prescaler = prescaler;
    timer_config.TIM_ClockDivision = TIM_CKD_DIV1;
    timer_config.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &timer_config);

    /* Enable timer2 */
    TIM_Cmd(TIM2, ENABLE);
}

/*********************************************************************
 * @fn      AT24C02_init
 *
 * @brief   Initializes AT24C02 EEPROM.
 *
 * @return  none
 */
void AT24C02_init(void)
{
    i2c_init(100000, 0xA1);
}

/*********************************************************************
 * @fn      AT24C02_read_one_byte
 *
 * @brief   Read one data byte from EEPROM.
 *
 * @param   read_address - Read first address
 *
 * @return  data - Read data byte, or -1 on error
 */
uint8_t AT24C02_read_one_byte(uint16_t read_address)
{
    uint8_t data = 0;

    if (I2C_WaitFlagStatusUntilTimeout(I2C2, I2C_FLAG_BUSY, RESET, I2C_MAX_TIMEOUT) != I2C_ERR_SUCCESS)
        return -1;
    I2C_GenerateSTART(I2C2, ENABLE);

    if (I2C_WaitCheckEventUntilTimeout(I2C2, I2C_EVENT_MASTER_MODE_SELECT, I2C_MAX_TIMEOUT) == I2C_ERR_TIMEOUT)
        return -1;
    I2C_Send7bitAddress(I2C2, 0xA1, I2C_Direction_Transmitter);

    if (I2C_WaitCheckEventUntilTimeout(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, I2C_MAX_TIMEOUT) == I2C_ERR_TIMEOUT)
        return -1;

#if (Address_Lenth == Address_8bit)
    I2C_SendData(I2C2, (uint8_t)(read_address & 0x00FF));
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

#elif (Address_Lenth == Address_16bit)
    I2C_SendData(I2C2, (uint8_t)(read_address >> 8));
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C2, (uint8_t)(read_address & 0x00FF));
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

#endif

    I2C_GenerateSTART(I2C2, ENABLE);

    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));
    I2C_Send7bitAddress(I2C2, 0xA1, I2C_Direction_Receiver);

    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));
    while (I2C_GetFlagStatus(I2C2, I2C_FLAG_RXNE) == RESET)
        I2C_AcknowledgeConfig(I2C2, DISABLE);

    data = I2C_ReceiveData(I2C2);
    I2C_GenerateSTOP(I2C2, ENABLE);

    return data;
}

/*********************************************************************
 * @fn      AT24C02_write_one_byte
 *
 * @brief   Write one data byte to EEPROM.
 *
 * @param   write_address - Write first address
 * @param   data_to_write - Data to write
 *
 * @return  none
 */
void AT24C02_write_one_byte(uint16_t write_address, uint8_t data_to_write)
{
    if (I2C_WaitFlagStatusUntilTimeout(I2C2, I2C_FLAG_BUSY, RESET, I2C_MAX_TIMEOUT) != I2C_ERR_SUCCESS)
        return;
    I2C_GenerateSTART(I2C2, ENABLE);
    if (I2C_WaitCheckEventUntilTimeout(I2C2, I2C_EVENT_MASTER_MODE_SELECT, I2C_MAX_TIMEOUT) == I2C_ERR_TIMEOUT)
        return;

    I2C_Send7bitAddress(I2C2, 0xA1, I2C_Direction_Transmitter);

    if (I2C_WaitCheckEventUntilTimeout(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, I2C_MAX_TIMEOUT) == I2C_ERR_TIMEOUT)
        return;

#if (Address_Lenth == Address_8bit)
    I2C_SendData(I2C2, (uint8_t)(write_address & 0x00FF));
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
#elif (Address_Lenth == Address_16bit)
    I2C_SendData(I2C2, (uint8_t)(write_address >> 8));
    LOG_DEBUG("EEPROM: sent high byte of address: 0x%02X", (uint8_t)(write_address >> 8));
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C2, (uint8_t)(write_address & 0x00FF));
    LOG_DEBUG("EEPROM: sent low byte of address: 0x%02X", (uint8_t)(write_address & 0x00FF));
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
#endif

    if (I2C_GetFlagStatus(I2C2, I2C_FLAG_TXE) != RESET)
    {
        I2C_SendData(I2C2, data_to_write);
    }

    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C2, ENABLE);
}

/*********************************************************************
 * @fn      AT24C02_read
 *
 * @brief   Read multiple data bytes from EEPROM.
 *
 * @param   read_address - Read first address (AT24C02: 0~255)
 * @param   buffer - Buffer to store read data
 * @param   num_to_read - Number of bytes to read
 *
 * @return  none
 */
void AT24C02_read(uint16_t read_address, uint8_t *buffer, uint16_t num_to_read)
{
    while (num_to_read)
    {
        *buffer++ = AT24C02_read_one_byte(read_address++);
        num_to_read--;
    }
}

/*********************************************************************
 * @fn      AT24C02_write
 *
 * @brief   Write multiple data bytes to EEPROM.
 *
 * @param   write_address - Write first address (AT24C02: 0~255)
 * @param   buffer - Buffer containing data to write
 * @param   num_to_write - Number of bytes to write
 *
 * @return  none
 */
void AT24C02_write(uint16_t write_address, uint8_t *buffer, uint16_t num_to_write)
{
    while (num_to_write--)
    {
        AT24C02_write_one_byte(write_address, *buffer);
        write_address++;
        buffer++;
        Delay_Ms(2);
    }
}

/*********************************************************************
 * @fn      AT24C02_test
 *
 * @brief   Reads PID and VID from AT24C02 EEPROM.
 *
 * @return  void
 */
void AT24C02_read_usb_info()
{
    uint8_t raw[EEPROM_TOTAL_SIZE];
    eeprom_tlv_info_t info;

    AT24C02_read(0x00, raw, EEPROM_TOTAL_SIZE);

    // microTLV format verified against:
    // - ch572d/src/src/external/microtlv/tlv.c
    // - website/src/lib/usbsp/tlv.js
    if (eeprom_parse_tlv(raw, EEPROM_TOTAL_SIZE, &info) == 0) {
        log_parsed_eeprom_tlv(&info);
        vid = info.has_vid ? info.vid : 0;
        pid = info.has_pid ? info.pid : 0;
        LOG_INFO("EEPROM: effective VID=0x%04X PID=0x%04X", vid, pid);
        return;
    }

    LOG_WARN("EEPROM: TLV parse failed, using legacy fixed VID/PID layout");
    vid = ((uint16_t)raw[EEPROM_ADDR_DIV] << 8) | raw[EEPROM_ADDR_DIV + 1];
    pid = ((uint16_t)raw[EEPROM_ADDR_PID] << 8) | raw[EEPROM_ADDR_PID + 1];
    LOG_INFO("EEPROM legacy: VID=0x%04X (%u)", vid, vid);
    LOG_INFO("EEPROM legacy: PID=0x%04X (%u)", pid, pid);
}

/*********************************************************************
 * @fn      AT24C02_test
 *
 * @brief   Tests AT24C02 EEPROM by writing and reading data.
 *
 * @return  0 on success, non-zero on failure
 */
uint8_t AT24C02_test(void)
{
    uint8_t result;
    uint8_t write_buffer[] = "This is a test for the AT24C02!";
    uint8_t *read_buffer;
    read_buffer = malloc(sizeof(write_buffer));

    AT24C02_write(0x00, write_buffer, sizeof(write_buffer));

    memset(read_buffer, 0, sizeof(write_buffer));
    AT24C02_read(0x00, read_buffer, sizeof(write_buffer));

    result = memcmp(write_buffer, read_buffer, sizeof(write_buffer));

    free(read_buffer);
    return result == 0;
}
