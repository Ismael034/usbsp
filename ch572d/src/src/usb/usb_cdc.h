#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdint.h>

void usb_cdc_init(void);
void usb_cdc_task(void);
uint8_t usb_cdc_send_ep1(const uint8_t *data, uint16_t len);

#endif
