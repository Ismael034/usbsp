#ifndef __USB_RELAY_H
#define __USB_RELAY_H

#include "relay.h"
#include "usb_lib.h"
#include "usbd.h"


void usb_relay_driver_init(void);
uint8_t usb_relay_usbd_endp_data_up(uint8_t endp, uint8_t *pbuf, uint16_t len);

#endif