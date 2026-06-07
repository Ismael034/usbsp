#ifndef __EEPROM_H
#define __EEPROM_H

#define Address_8bit  1
#define Address_16bit  0

#define Address_Lenth   Address_8bit
#define SIZE sizeof(TEXT_Buffer)

#define EEPROM_ADDR_DIV  0x00  // EEPROM address for `div` (2 bytes)
#define EEPROM_ADDR_PID  0x02  // EEPROM address for `pid` (2 bytes)

#define EEPROM_USB_TEXT_MAX_LEN 60u
#define EEPROM_FLAG_SELF_POWERED    (1u << 0)
#define EEPROM_FLAG_REMOTE_WAKEUP   (1u << 1)
#define EEPROM_FLAG_BOOT_CONNECTED  (1u << 2)

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
    uint8_t has_manufacturer;
    uint8_t has_product;
    uint8_t has_serial;
    uint16_t vid;
    uint16_t pid;
    uint16_t bcd_device;
    uint16_t max_power_ma;
    uint8_t flags;
    uint16_t attach_delay_ms;
    char manufacturer[EEPROM_USB_TEXT_MAX_LEN + 1];
    char product[EEPROM_USB_TEXT_MAX_LEN + 1];
    char serial[EEPROM_USB_TEXT_MAX_LEN + 1];
} eeprom_usb_info_t;

void AT24C02_init(void);
uint8_t AT24C02_read_one_byte(uint16_t read_address);
void AT24C02_write_one_byte(uint16_t write_address, uint8_t data_to_write);
void AT24C02_read(uint16_t read_address, uint8_t *buffer, uint16_t num_to_read);
void AT24C02_write(uint16_t write_address, uint8_t *buffer, uint16_t num_to_write);
void AT24C02_read_usb_info();
void eeprom_apply_usb_overrides(void);
uint8_t AT24C02_test(void);
void tim2_init(uint16_t prescaler);

extern uint16_t vid;
extern uint16_t pid;
extern eeprom_usb_info_t eeprom_usb_info;

#endif
