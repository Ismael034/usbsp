#include "usb_webusb.h"

#define USB_DESCR_TYP_BOS       0x0F
#define WEBUSB_URL_DESCRIPTOR_TYPE 0x03
#define WEBUSB_URL_SCHEME_HTTPS 0x01

static const uint8_t webusb_url_descriptor[] = {
    0x15,
    WEBUSB_URL_DESCRIPTOR_TYPE,
    WEBUSB_URL_SCHEME_HTTPS,
    'u','s','b','s','p','.','l','o','c','a','l','/','c','o','n','f','i','g'
};

static const uint8_t bos_descriptor[] = {
    0x05, USB_DESCR_TYP_BOS, 0x1D, 0x00, 0x01,

    0x18, 0x10, 0x05, 0x00,
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
    0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65,
    0x00, 0x01,
    WEBUSB_VENDOR_CODE,
    WEBUSB_URL_INDEX
};

const uint8_t *usb_webusb_get_bos(uint16_t *len)
{
    if (len) {
        *len = sizeof(bos_descriptor);
    }
    return bos_descriptor;
}

uint8_t usb_webusb_handle_vendor_request(const PUSB_SETUP_REQ setup_req,
                                         const uint8_t **p_descr,
                                         uint16_t *setup_len)
{
    if (!setup_req || !p_descr || !setup_len) {
        return 0;
    }

    if (setup_req->bRequest == WEBUSB_VENDOR_CODE &&
        setup_req->wIndex == WEBUSB_REQ_GET_URL) {
        if ((setup_req->wValue & 0xff) == WEBUSB_URL_INDEX) {
            *p_descr = webusb_url_descriptor;
            *setup_len = webusb_url_descriptor[0];
            return 1;
        }
        *setup_len = 0;
        return 1;
    }

    return 0;
}
