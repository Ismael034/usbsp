#ifndef USB_LOGS_H
#define USB_LOGS_H

#include <stdint.h>

void usb_logs_init(void);
void usb_logs_on_config(uint8_t configured);
void usb_logs_on_ep1_in_ready(uint8_t ready);
void usb_logs_task(uint8_t ep1_ready);
void usb_logs_push(const char *msg);

#endif
