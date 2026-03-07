#ifndef __EEPROM_H
#define __EEPROM_H

#define Address_8bit  1
#define Address_16bit  0

#define Address_Lenth   Address_8bit
#define SIZE sizeof(TEXT_Buffer)

#define EEPROM_ADDR_DIV  0x00  // EEPROM address for `div` (2 bytes)
#define EEPROM_ADDR_PID  0x02  // EEPROM address for `pid` (2 bytes)

void AT24C02_init(void);
uint8_t AT24C02_read_one_byte(uint16_t read_address);
void AT24C02_write_one_byte(uint16_t write_address, uint8_t data_to_write);
void AT24C02_read(uint16_t read_address, uint8_t *buffer, uint16_t num_to_read);
void AT24C02_write(uint16_t write_address, uint8_t *buffer, uint16_t num_to_write);
void AT24C02_read_usb_info();
uint8_t AT24C02_test(void);
void tim2_init(uint16_t prescaler);

extern uint16_t vid;
extern uint16_t pid;

#endif