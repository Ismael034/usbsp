#ifndef USB_WEBUSB_CMDS_H
#define USB_WEBUSB_CMDS_H

#include <stdint.h>

typedef uint8_t (*usb_webusb_send_fn)(const uint8_t *data, uint8_t len);

void usb_webusb_cmds_init(usb_webusb_send_fn send_fn);
void usb_webusb_cmds_handle(const uint8_t *buf, uint8_t len);

#endif
