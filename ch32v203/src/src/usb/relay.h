#ifndef __RELAY_H__
#define __RELAY_H__

#include "usb_core.h"

#define USB_DEVICE_DESCRIPTOR_MAX_SIZE 64
#define USB_CONFIG_DESCRIPTOR_MAX_SIZE 255
#define USB_STRING_DESCRIPTOR_MAX_SIZE 256
#define USB_HID_REPORT_DESCRIPTOR_MAX_SIZE 1024
#define MAX_RETRY_ATTEMPTS 5

void usb_relay_init(void);
void usb_relay_reset(void);
void usb_relay_status_in(void);
void usb_relay_status_out(void);
RESULT usb_relay_data_setup(uint8_t request_no);
RESULT usb_relay_nodata_setup(uint8_t request_no);
RESULT usb_relay_get_interface_setting(uint8_t Interface, uint8_t AlternateSetting);
uint8_t *usb_relay_get_device_descriptor(uint16_t length);
uint8_t *usb_relay_get_config_descriptor(uint16_t length);
uint8_t *usb_relay_get_string_descriptor(uint16_t length);
uint8_t *usb_relay_get_hid_report_descriptor(uint16_t length);
uint8_t *usb_relay_set_report(uint16_t length);
uint8_t *usb_relay_get_report(uint16_t length);

void usb_relay_set_configuration(void);
void usb_relay_set_device_address(void);
void usb_relay_set_device_feature(void);
void usb_relay_clear_feature(void);

extern uint8_t usbd_configured;

#endif