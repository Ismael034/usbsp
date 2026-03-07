#include "usb_desc.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed)) USB_DeviceDescriptor;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed)) USB_InterfaceDescriptor;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    uint8_t bReportDescriptorType;
    uint16_t wReportDescriptorLength;
} __attribute__((packed)) USB_HIDDescriptor;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[];
} __attribute__((packed)) USB_StringDescriptor;

static uint8_t test_ascii_to_utf16le(const char *ascii, uint16_t *utf16le, uint8_t *out_size)
{
    uint8_t len;
    uint8_t i;

    if (ascii == NULL || utf16le == NULL || out_size == NULL)
    {
        return 1;
    }

    len = (uint8_t)strlen(ascii);
    if (len > (USBD_MAX_STRING_LEN - 2u) / 2u)
    {
        len = (USBD_MAX_STRING_LEN - 2u) / 2u;
    }

    *out_size = (uint8_t)(2u + 2u * len);
    for (i = 0; i < len; i++)
    {
        utf16le[i] = (uint16_t)ascii[i];
    }

    return 0;
}

uint8_t usbd_test_descriptors_init(const DeviceDescParams *dev_params,
                                   const ConfigDescParams *config_params,
                                   const InterfaceDescParams *interfaces,
                                   uint8_t num_interfaces,
                                   const EndpointDescParams *endpoints,
                                   uint8_t *num_endpoints_per_interface,
                                   const StringDescParams *str_params,
                                   const ClassSpecificParams *class_params)
{
    USB_DeviceDescriptor *dev;
    USB_ConfigDescriptor *cfg;
    USB_InterfaceDescriptor *intf;
    USB_HIDDescriptor *hid;
    USB_EndpointDescriptor *ep;
    USB_StringDescriptor *lang;
    USB_StringDescriptor *vendor;
    USB_StringDescriptor *product;
    USB_StringDescriptor *serial;
    uint16_t cfg_size;
    uint16_t offset;

    if (dev_params == NULL ||
        config_params == NULL ||
        interfaces == NULL ||
        endpoints == NULL ||
        num_endpoints_per_interface == NULL ||
        str_params == NULL ||
        class_params == NULL)
    {
        return 1;
    }

    if (num_interfaces == 0u)
    {
        return 1;
    }

    if (class_params[0].class != 0x03u)
    {
        return 1;
    }

    dev = (USB_DeviceDescriptor *)USBD_DeviceDescriptor;
    dev->bLength = USBD_SIZE_DEVICE_DESC;
    dev->bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE;
    dev->bcdUSB = 0x0200;
    dev->bDeviceClass = 0x00;
    dev->bDeviceSubClass = 0x00;
    dev->bDeviceProtocol = 0x00;
    dev->bMaxPacketSize0 = dev_params->max_packet_size;
    dev->idVendor = dev_params->vendor_id;
    dev->idProduct = dev_params->product_id;
    dev->bcdDevice = dev_params->device_version;
    dev->iManufacturer = 0x01;
    dev->iProduct = 0x02;
    dev->iSerialNumber = 0x03;
    dev->bNumConfigurations = 0x01;

    cfg_size = (uint16_t)(9u + 9u + 9u + 7u);
    if (USBD_ConfigDescriptor != NULL)
    {
        free(USBD_ConfigDescriptor);
    }
    USBD_ConfigDescriptor = (uint8_t *)malloc(cfg_size);
    if (USBD_ConfigDescriptor == NULL)
    {
        return 1;
    }
    memset(USBD_ConfigDescriptor, 0, cfg_size);
    USBD_ConfigDescSize = cfg_size;

    offset = 0;
    cfg = (USB_ConfigDescriptor *)(USBD_ConfigDescriptor + offset);
    cfg->bLength = 0x09;
    cfg->bDescriptorType = USB_CONFIGURATION_DESCRIPTOR_TYPE;
    cfg->wTotalLength = cfg_size;
    cfg->bNumInterfaces = config_params->num_interfaces;
    cfg->bConfigurationValue = config_params->config_value;
    cfg->iConfiguration = 0x00;
    cfg->bmAttributes = config_params->attributes;
    cfg->bMaxPower = config_params->max_power;
    offset += 9u;

    intf = (USB_InterfaceDescriptor *)(USBD_ConfigDescriptor + offset);
    intf->bLength = 0x09;
    intf->bDescriptorType = USB_INTERFACE_DESCRIPTOR_TYPE;
    intf->bInterfaceNumber = interfaces[0].interface_number;
    intf->bAlternateSetting = 0x00;
    intf->bNumEndpoints = num_endpoints_per_interface[0];
    intf->bInterfaceClass = interfaces[0].class;
    intf->bInterfaceSubClass = interfaces[0].subclass;
    intf->bInterfaceProtocol = interfaces[0].protocol;
    intf->iInterface = 0x00;
    offset += 9u;

    hid = (USB_HIDDescriptor *)(USBD_ConfigDescriptor + offset);
    hid->bLength = 0x09;
    hid->bDescriptorType = USB_HID_DESCRIPTOR_TYPE;
    hid->bcdHID = 0x0111;
    hid->bCountryCode = 0x00;
    hid->bNumDescriptors = 0x01;
    hid->bReportDescriptorType = USB_HID_REPORT_DESCRIPTOR_TYPE;
    hid->wReportDescriptorLength = class_params[0].data.hid.report_desc_size;
    offset += 9u;

    ep = (USB_EndpointDescriptor *)(USBD_ConfigDescriptor + offset);
    ep->bLength = 0x07;
    ep->bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE;
    ep->bEndpointAddress = endpoints[0].endpoint_address;
    ep->bmAttributes = endpoints[0].attributes;
    ep->wMaxPacketSize = endpoints[0].max_packet_size;
    ep->bInterval = endpoints[0].interval;

    if (USBD_HIDReportDescriptor[0] != NULL)
    {
        free(USBD_HIDReportDescriptor[0]);
    }
    USBD_HIDReportDescriptor[0] = (uint8_t *)malloc(class_params[0].data.hid.report_desc_size);
    if (USBD_HIDReportDescriptor[0] == NULL)
    {
        return 1;
    }
    memcpy(USBD_HIDReportDescriptor[0],
           class_params[0].data.hid.report_descriptor,
           class_params[0].data.hid.report_desc_size);
    USBD_HIDReportDescSize = class_params[0].data.hid.report_desc_size;

    lang = (USB_StringDescriptor *)USBD_StringLangID;
    lang->bLength = USBD_SIZE_STRING_LANGID;
    lang->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
    lang->wString[0] = str_params->lang_id;

    if (USBD_StringVendor != NULL)
    {
        free(USBD_StringVendor);
    }
    USBD_StringVendor = (uint8_t *)malloc(USBD_MAX_STRING_LEN);
    if (USBD_StringVendor == NULL)
    {
        return 1;
    }
    vendor = (USB_StringDescriptor *)USBD_StringVendor;
    vendor->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
    if (test_ascii_to_utf16le(str_params->vendor_str, vendor->wString, &USBD_StringVendorSize) != 0)
    {
        return 1;
    }
    vendor->bLength = USBD_StringVendorSize;

    if (USBD_StringProduct != NULL)
    {
        free(USBD_StringProduct);
    }
    USBD_StringProduct = (uint8_t *)malloc(USBD_MAX_STRING_LEN);
    if (USBD_StringProduct == NULL)
    {
        return 1;
    }
    product = (USB_StringDescriptor *)USBD_StringProduct;
    product->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
    if (test_ascii_to_utf16le(str_params->product_str, product->wString, &USBD_StringProductSize) != 0)
    {
        return 1;
    }
    product->bLength = USBD_StringProductSize;

    if (USBD_StringSerial != NULL)
    {
        free(USBD_StringSerial);
    }
    USBD_StringSerial = (uint8_t *)malloc(USBD_MAX_STRING_LEN);
    if (USBD_StringSerial == NULL)
    {
        return 1;
    }
    serial = (USB_StringDescriptor *)USBD_StringSerial;
    serial->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
    if (test_ascii_to_utf16le(str_params->serial_str, serial->wString, &USBD_StringSerialSize) != 0)
    {
        return 1;
    }
    serial->bLength = USBD_StringSerialSize;

    return 0;
}
