#ifndef USB_WEBUSB_H
#define USB_WEBUSB_H

#include <stdint.h>
#include "CH57x_common.h"

#define WEBUSB_VENDOR_CODE      0x22
#define WEBUSB_REQ_GET_URL      0x0002
#define WEBUSB_URL_INDEX        0x01
#define MS_OS_10_STRING_INDEX   0xEE
#define MS_OS_10_REQ_COMPAT_ID  0x0004
#define MS_OS_10_REQ_PROPERTIES 0x0005

const uint8_t *usb_webusb_get_bos(uint16_t *len);
uint8_t usb_webusb_get_string_descriptor(uint8_t index, const uint8_t **p_descr, uint16_t *len);
uint8_t usb_webusb_handle_vendor_request(const PUSB_SETUP_REQ setup_req,
                                         const uint8_t **p_descr,
                                         uint16_t *setup_len);

#endif
