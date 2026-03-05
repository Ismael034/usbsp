#ifndef __RELAY_H__
#define __RELAY_H__

#include "usb_core.h"

void usb_relay_init(void);
void usb_relay_reset(void);
void usb_relay_status_in(void);
void usb_relay_status_out(void);
RESULT usb_relay_data_setup(uint8_t request_no);
RESULT usb_relay_nodata_setup(uint8_t request_no);
RESULT usb_relay_get_interface_setting(uint8_t Interface, uint8_t AlternateSetting);
uint8_t *usb_relay_data_generic(uint16_t length);
uint8_t usb_relay_poll(void);

void usb_relay_set_configuration(void);
void usb_relay_set_device_feature(void);
void usb_relay_clear_feature(void);

#endif
