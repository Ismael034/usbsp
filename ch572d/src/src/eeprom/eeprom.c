#include "CH57x_common.h"
#include "CH57x_gpio.h"
#include "CH57x_i2c.h"
#include "../debug_log.h"
#include "eeprom.h"
#include <string.h>

#define EEPROM_I2C_ADDR     0xA1
#define I2C_MAX_TIMEOUT     0x8000
#define I2C_ACKPOLL_TIMEOUT 0x0100
#define EEPROM_CTRL_PIN     GPIO_Pin_10
#define EEPROM_SCL_PIN      GPIO_Pin_3
#define EEPROM_SDA_PIN      GPIO_Pin_2

uint16_t vid = 0;
uint16_t pid = 0;

static uint8_t i2c_wait_flag(uint32_t flag, FlagStatus status)
{
    uint32_t timeout = I2C_MAX_TIMEOUT;
    while (timeout--) {
        if (I2C_GetFlagStatus(flag) == status) {
            return 0;
        }
    }
    LOG_ERROR("EEPROM: wait flag timeout flag=0x%08lX", (unsigned long)flag);
    return 1;
}

static uint8_t i2c_wait_event_opt(uint32_t event, uint32_t timeout, uint8_t log_timeout)
{
    while (timeout--) {
        if (I2C_CheckEvent(event)) {
            return 0;
        }
        // If the slave NACKs (e.g. during ACK polling while internal write is in progress),
        // the AF flag is raised. Clear it and fail fast rather than spinning.
        if (I2C_GetFlagStatus(I2C_FLAG_AF) == SET) {
            I2C_ClearFlag(I2C_FLAG_AF);
            return 1;
        }
    }
    if (log_timeout) {
        LOG_ERROR("EEPROM: wait event timeout event=0x%08lX", (unsigned long)event);
    }
    return 1;
}

static uint8_t i2c_wait_event(uint32_t event)
{
    return i2c_wait_event_opt(event, I2C_MAX_TIMEOUT, 1);
}

void i2c_init(uint32_t clock_speed, uint16_t own_address)
{
    GPIOPinRemap(ENABLE, REMAP_I2C_MODE2);
    I2C_Init(I2C_Mode_I2C,
             clock_speed,
             I2C_DutyCycle_2,
             I2C_Ack_Enable,
             I2C_AckAddr_7bit,
             own_address);
    I2C_Cmd(ENABLE);
}

void AT24C02_bus_release(void)
{
    I2C_Cmd(DISABLE);
    GPIOA_ModeCfg(EEPROM_SCL_PIN | EEPROM_SDA_PIN, GPIO_ModeIN_Floating);
    GPIOA_ModeCfg(EEPROM_CTRL_PIN, GPIO_ModeIN_Floating);
}

void AT24C02_bus_claim(void)
{
    GPIOA_SetBits(EEPROM_CTRL_PIN);
    GPIOA_ModeCfg(EEPROM_CTRL_PIN, GPIO_ModeOut_PP_5mA);
}

void AT24C02_init(void)
{
    i2c_init(100000, 0xA1);
}

uint8_t AT24C02_read_one_byte(uint16_t read_address)
{
    uint8_t data = 0;

    if (i2c_wait_flag(I2C_FLAG_BUSY, RESET)) {
        LOG_ERROR("EEPROM: I2C busy timeout");
        return (uint8_t)-1;
    }

    I2C_GenerateSTART(ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) {
        LOG_ERROR("EEPROM: master mode select timeout");
        return (uint8_t)-1;
    }

    I2C_Send7bitAddress(EEPROM_I2C_ADDR, I2C_Direction_Transmitter);
    if (i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        LOG_ERROR("EEPROM: tx mode select timeout");
        return (uint8_t)-1;
    }

#if (Address_Lenth == Address_8bit)
    I2C_SendData((uint8_t)(read_address & 0x00FF));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        LOG_ERROR("EEPROM: addr (8-bit) tx timeout");
        return (uint8_t)-1;
    }
#elif (Address_Lenth == Address_16bit)
    I2C_SendData((uint8_t)(read_address >> 8));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        log_printf("EEPROM: addr high byte tx timeout\r\n");
        return (uint8_t)-1;
    }
    log_printf("EEPROM: addr high=0x%02X\r\n", (uint8_t)(read_address >> 8));

    I2C_SendData((uint8_t)(read_address & 0x00FF));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        log_printf("EEPROM: addr low byte tx timeout\r\n");
        return (uint8_t)-1;
    }
    log_printf("EEPROM: addr low=0x%02X\r\n", (uint8_t)(read_address & 0x00FF));
#endif

    I2C_GenerateSTART(ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) {
        LOG_ERROR("EEPROM: repeated start timeout");
        return (uint8_t)-1;
    }

    I2C_Send7bitAddress(EEPROM_I2C_ADDR, I2C_Direction_Receiver);
    if (i2c_wait_event(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) {
        LOG_ERROR("EEPROM: rx mode select timeout");
        return (uint8_t)-1;
    }

    I2C_AcknowledgeConfig(DISABLE);
    if (i2c_wait_flag(I2C_FLAG_RXNE, SET)) {
        I2C_AcknowledgeConfig(ENABLE);
        LOG_ERROR("EEPROM: rxne timeout");
        return (uint8_t)-1;
    }

    data = I2C_ReceiveData();
    I2C_GenerateSTOP(ENABLE);
    I2C_AcknowledgeConfig(ENABLE);

    return data;
}

void AT24C02_write_one_byte(uint16_t write_address, uint8_t data_to_write)
{
    if (i2c_wait_flag(I2C_FLAG_BUSY, RESET)) {
        LOG_ERROR("EEPROM: I2C busy timeout");
        return;
    }

    I2C_GenerateSTART(ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) {
        LOG_ERROR("EEPROM: master mode select timeout");
        return;
    }

    I2C_Send7bitAddress(EEPROM_I2C_ADDR, I2C_Direction_Transmitter);
    if (i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        LOG_ERROR("EEPROM: tx mode select timeout");
        return;
    }

#if (Address_Lenth == Address_8bit)
    I2C_SendData((uint8_t)(write_address & 0x00FF));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        LOG_ERROR("EEPROM: addr (8-bit) tx timeout");
        return;
    }
#elif (Address_Lenth == Address_16bit)
    I2C_SendData((uint8_t)(write_address >> 8));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        log_printf("EEPROM: addr high byte tx timeout\r\n");
        return;
    }
    log_printf("EEPROM: addr high=0x%02X\r\n", (uint8_t)(write_address >> 8));

    I2C_SendData((uint8_t)(write_address & 0x00FF));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        log_printf("EEPROM: addr low byte tx timeout\r\n");
        return;
    }
    log_printf("EEPROM: addr low=0x%02X\r\n", (uint8_t)(write_address & 0x00FF));
#endif

    I2C_SendData(data_to_write);
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        LOG_ERROR("EEPROM: data tx timeout");
        return;
    }

    I2C_GenerateSTOP(ENABLE);
}

void AT24C02_read(uint16_t read_address, uint8_t *buffer, uint16_t num_to_read)
{
    while (num_to_read) {
        *buffer++ = AT24C02_read_one_byte(read_address++);
        num_to_read--;
    }
}

static uint8_t AT24C02_ack_poll(void)
{
    // ACK polling: the EEPROM NACKs while internal write-cycle is in progress.
    // Return 0 when ready, 1 on timeout.
    uint16_t tries = 50; // ~50ms worst-case (DelayMs(1) per try)

    while (tries--) {
        // Wait for bus idle.
        if (i2c_wait_flag(I2C_FLAG_BUSY, RESET)) {
            DelayMs(1);
            continue;
        }

        I2C_GenerateSTART(ENABLE);
        if (i2c_wait_event_opt(I2C_EVENT_MASTER_MODE_SELECT, I2C_ACKPOLL_TIMEOUT, 0)) {
            I2C_GenerateSTOP(ENABLE);
            DelayMs(1);
            continue;
        }

        I2C_Send7bitAddress(EEPROM_I2C_ADDR, I2C_Direction_Transmitter);
        if (i2c_wait_event_opt(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, I2C_ACKPOLL_TIMEOUT, 0) == 0) {
            I2C_GenerateSTOP(ENABLE);
            return 0;
        }

        I2C_GenerateSTOP(ENABLE);
        DelayMs(1);
    }

    LOG_ERROR("EEPROM: ack poll timeout");
    return 1;
}


static uint8_t AT24C02_write_page(uint16_t write_address, const uint8_t *buffer, uint8_t count)
{
    uint8_t i;

    if (!buffer || count == 0) {
        return 1;
    }

    if (i2c_wait_flag(I2C_FLAG_BUSY, RESET)) {
        LOG_ERROR("EEPROM: I2C busy timeout");
        return 1;
    }

    I2C_GenerateSTART(ENABLE);
    if (i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT)) {
        LOG_ERROR("EEPROM: master mode select timeout");
        return 1;
    }

    I2C_Send7bitAddress(EEPROM_I2C_ADDR, I2C_Direction_Transmitter);
    if (i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        LOG_ERROR("EEPROM: tx mode select timeout");
        return 1;
    }

#if (Address_Lenth == Address_8bit)
    I2C_SendData((uint8_t)(write_address & 0x00FF));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        LOG_ERROR("EEPROM: addr (8-bit) tx timeout");
        return 1;
    }
#elif (Address_Lenth == Address_16bit)
    I2C_SendData((uint8_t)(write_address >> 8));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        log_printf("EEPROM: addr high byte tx timeout\r\n");
        return 1;
    }

    I2C_SendData((uint8_t)(write_address & 0x00FF));
    if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        log_printf("EEPROM: addr low byte tx timeout\r\n");
        return 1;
    }
#endif

    for (i = 0; i < count; i++) {
        I2C_SendData(buffer[i]);
        if (i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
            LOG_ERROR("EEPROM: data tx timeout");
            I2C_GenerateSTOP(ENABLE);
            return 1;
        }
    }

    I2C_GenerateSTOP(ENABLE);

    if (AT24C02_ack_poll()) {
        return 1;
    }
    return 0;
}

uint8_t AT24C02_write(uint16_t write_address, uint8_t *buffer, uint16_t num_to_write)
{
    const uint8_t page_sz = 8;

    while (num_to_write) {
        uint8_t page_off = (uint8_t)(write_address % page_sz);
        uint8_t chunk = (uint8_t)page_sz - page_off;
        if (chunk > num_to_write) {
            chunk = (uint8_t)num_to_write;
        }

        if (AT24C02_write_page(write_address, buffer, chunk)) {
            return 1;
        }
        write_address = (uint16_t)(write_address + chunk);
        buffer += chunk;
        num_to_write = (uint16_t)(num_to_write - chunk);
    }

    return 0;
}


void AT24C02_read_usb_info(void)
{
    uint8_t buffer[4];

    LOG_INFO("EEPROM: read USB VID/PID");
    AT24C02_read(0x00, buffer, 4);

    vid = ((uint16_t)buffer[EEPROM_ADDR_DIV] << 8) | buffer[EEPROM_ADDR_DIV + 1];
    pid = ((uint16_t)buffer[EEPROM_ADDR_PID] << 8) | buffer[EEPROM_ADDR_PID + 1];

    LOG_INFO("EEPROM: VID=0x%04X", vid);
    LOG_INFO("EEPROM: PID=0x%04X", pid);
}

uint8_t AT24C02_test(void)
{
    uint8_t write_buffer[] = "This is a test for the AT24C02!";
    uint8_t read_buffer[sizeof(write_buffer)];

    if (AT24C02_write(0x00, write_buffer, sizeof(write_buffer))) {
        return 0;
    }
    memset(read_buffer, 0, sizeof(read_buffer));
    AT24C02_read(0x00, read_buffer, sizeof(read_buffer));

    return (memcmp(write_buffer, read_buffer, sizeof(write_buffer)) == 0);
}
