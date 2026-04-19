#include "usb_webusb.h"

#define USB_DESCR_TYP_BOS       0x0F
#define WEBUSB_URL_DESCRIPTOR_TYPE 0x03
#define WEBUSB_URL_SCHEME_HTTPS 0x01
#define USBSP_WEBUSB_INTERFACE   0x02

static uint16_t clamp_setup_len(uint16_t requested, uint16_t available)
{
    return (requested != 0u && requested < available) ? requested : available;
}

static const uint8_t webusb_url_descriptor[] = {
    0x15,
    WEBUSB_URL_DESCRIPTOR_TYPE,
    WEBUSB_URL_SCHEME_HTTPS,
    'u','s','b','s','p','.','l','o','c','a','l','/','c','o','n','f','i','g'
};

static const uint8_t ms_os_10_string_descriptor[] = {
    0x12, 0x03,
    'M', 0x00, 'S', 0x00, 'F', 0x00, 'T', 0x00, '1', 0x00, '0', 0x00, '0', 0x00,
    WEBUSB_VENDOR_CODE, 0x00
};

static const uint8_t ms_os_10_compat_id_descriptor[] = {
    0x28, 0x00, 0x00, 0x00,
    0x00, 0x01,
    0x04, 0x00,
    0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    USBSP_WEBUSB_INTERFACE,
    0x01,
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t ms_os_10_ext_properties_descriptor[] = {
    0x8E, 0x00, 0x00, 0x00,
    0x00, 0x01,
    0x05, 0x00,
    0x01, 0x00,

    0x84, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00,
    0x28, 0x00,
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00,
    'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00,
    'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00,
    'D', 0x00, 0x00, 0x00,
    0x4E, 0x00, 0x00, 0x00,
    '{', 0x00, '1', 0x00, 'D', 0x00, '4', 0x00, 'B', 0x00, '2', 0x00,
    '3', 0x00, '6', 0x00, '5', 0x00, '-', 0x00, '4', 0x00, '7', 0x00,
    '4', 0x00, '9', 0x00, '-', 0x00, '4', 0x00, '8', 0x00, 'E', 0x00,
    'A', 0x00, '-', 0x00, 'B', 0x00, '3', 0x00, '8', 0x00, 'A', 0x00,
    '-', 0x00, '7', 0x00, 'C', 0x00, '6', 0x00, 'F', 0x00, 'D', 0x00,
    'D', 0x00, 'D', 0x00, 'D', 0x00, '7', 0x00, 'E', 0x00, '2', 0x00,
    '6', 0x00, '}', 0x00, 0x00, 0x00
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

uint8_t usb_webusb_get_string_descriptor(uint8_t index, const uint8_t **p_descr, uint16_t *len)
{
    if (!p_descr || !len) {
        return 0;
    }

    if (index == MS_OS_10_STRING_INDEX) {
        *p_descr = ms_os_10_string_descriptor;
        *len = sizeof(ms_os_10_string_descriptor);
        return 1;
    }

    return 0;
}

uint8_t usb_webusb_handle_vendor_request(const PUSB_SETUP_REQ setup_req,
                                         const uint8_t **p_descr,
                                         uint16_t *setup_len)
{
    if (!setup_req || !p_descr || !setup_len) {
        return 0;
    }

    if (setup_req->bRequest == WEBUSB_VENDOR_CODE && setup_req->wIndex == WEBUSB_REQ_GET_URL) {
        if ((setup_req->wValue & 0xff) == WEBUSB_URL_INDEX) {
            *p_descr = webusb_url_descriptor;
            *setup_len = clamp_setup_len(setup_req->wLength, webusb_url_descriptor[0]);
            return 1;
        }
        *setup_len = 0;
        return 1;
    }

    if (setup_req->bRequest == WEBUSB_VENDOR_CODE && setup_req->wIndex == MS_OS_10_REQ_COMPAT_ID) {
        *p_descr = ms_os_10_compat_id_descriptor;
        *setup_len = clamp_setup_len(setup_req->wLength, (uint16_t)sizeof(ms_os_10_compat_id_descriptor));
        return 1;
    }

    if (setup_req->bRequest == WEBUSB_VENDOR_CODE && setup_req->wIndex == MS_OS_10_REQ_PROPERTIES) {
        *p_descr = ms_os_10_ext_properties_descriptor;
        *setup_len = clamp_setup_len(setup_req->wLength, (uint16_t)sizeof(ms_os_10_ext_properties_descriptor));
        return 1;
    }

    return 0;
}
