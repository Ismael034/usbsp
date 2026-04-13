#ifndef __EEPROM_H
#define __EEPROM_H

#include <stdint.h>

#define Address_8bit   1
#define Address_16bit  0

#define Address_Lenth  Address_8bit

#define EEPROM_ADDR_DIV  0x00
#define EEPROM_ADDR_PID  0x02

void i2c_init(uint32_t clock_speed, uint16_t own_address);
void AT24C02_init(void);
void AT24C02_bus_release(void);
void AT24C02_bus_claim(void);
uint8_t AT24C02_read_one_byte(uint16_t read_address);
void AT24C02_write_one_byte(uint16_t write_address, uint8_t data_to_write);
void AT24C02_read(uint16_t read_address, uint8_t *buffer, uint16_t num_to_read);
// Returns 0 on success, 1 on error.
uint8_t AT24C02_write(uint16_t write_address, uint8_t *buffer, uint16_t num_to_write);
void AT24C02_read_usb_info(void);
uint8_t AT24C02_test(void);

extern uint16_t vid;
extern uint16_t pid;

#endif
