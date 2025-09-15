#include "debug.h"
#include "eeprom.h"
#include <stdlib.h>
#include <string.h>

uint16_t vid = 0;
uint16_t pid = 0;

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
    printf("Sent high byte of address: 0x%02X\r\n", (uint8_t)(write_address >> 8));
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C2, (uint8_t)(write_address & 0x00FF));
    printf("Sent low byte of address: 0x%02X\r\n", (uint8_t)(write_address & 0x00FF));
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