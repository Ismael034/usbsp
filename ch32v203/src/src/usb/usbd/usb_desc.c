#include "usb_desc.h"
#include <string.h>
#include <stdlib.h>

/* USB Descriptor Arrays and Sizes */
uint8_t USBD_DeviceDescriptor[USBD_SIZE_DEVICE_DESC];
uint8_t *USBD_ConfigDescriptor = NULL;
uint8_t *USBD_StringDescriptor[4];
uint8_t *USBD_HIDDescriptor = NULL;
uint8_t *USBD_HIDReportDescriptor[MAX_USB_INTERFACES];

uint8_t USBD_StringLangID[USBD_SIZE_STRING_LANGID];
uint8_t *USBD_StringVendor = NULL;
uint8_t *USBD_StringProduct = NULL;
uint8_t *USBD_StringSerial = NULL;


uint16_t USBD_ConfigDescSize = 0;
uint8_t USBD_StringVendorSize = 0;
uint8_t USBD_StringProductSize = 0;
uint8_t USBD_StringSerialSize = 0;
uint16_t USBD_HIDDescSize = 0;
uint16_t USBD_HIDReportDescSize = 0;

/* Helper Function to Convert ASCII to UTF-16LE and Compute Size */
static uint8_t ascii_to_utf16le(const char *ascii, uint16_t *utf16le, uint8_t *out_size)
{
    if (!ascii || !utf16le || !out_size) return 1; /* Invalid parameters */
    
    uint8_t len = strlen(ascii);
    if (len > (USBD_MAX_STRING_LEN - 2) / 2) len = (USBD_MAX_STRING_LEN - 2) / 2; /* Cap at max */
    *out_size = 2 + 2 * len; /* bLength + bDescriptorType + string */
    
    for (uint8_t i = 0; i < len; i++) {
        utf16le[i] = (uint16_t)ascii[i]; /* ASCII to UTF-16LE */
    }
    return 0;
}

/* Initialize Device Descriptor */
uint8_t init_device_descriptor(const DeviceDescParams *params)
{
    if (!params || USBD_SIZE_DEVICE_DESC != sizeof(USB_DeviceDescriptor)) {
        return 1; /* Invalid parameters or size mismatch */
    }

    USB_DeviceDescriptor *desc = (USB_DeviceDescriptor *)USBD_DeviceDescriptor;
    desc->bLength = USBD_SIZE_DEVICE_DESC;
    desc->bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE;
    desc->bcdUSB = 0x0200; /* USB 2.0 */
    desc->bDeviceClass = 0x00; /* Class defined at interface level */
    desc->bDeviceSubClass = 0x00;
    desc->bDeviceProtocol = 0x00;
    desc->bMaxPacketSize0 = params->max_packet_size;
    desc->idVendor = params->vendor_id;
    desc->idProduct = params->product_id;
    desc->bcdDevice = params->device_version;
    desc->iManufacturer = 0x01;
    desc->iProduct = 0x02;
    desc->iSerialNumber = 0x03;
    desc->bNumConfigurations = 0x01;

    return 0;
}

/* Initialize Interface Descriptor */
uint8_t init_interface_descriptor(uint8_t *buffer, uint16_t *offset, const InterfaceDescParams *params)
{
    if (!buffer || !params || !offset) return 1; /* Invalid parameters */

    USB_InterfaceDescriptor *intf = (USB_InterfaceDescriptor *)(buffer + *offset);
    intf->bLength = 0x09;
    intf->bDescriptorType = USB_INTERFACE_DESCRIPTOR_TYPE;
    intf->bInterfaceNumber = params->interface_number;
    intf->bAlternateSetting = 0x00;
    intf->bNumEndpoints = params->num_endpoints;
    intf->bInterfaceClass = params->class;
    intf->bInterfaceSubClass = params->subclass;
    intf->bInterfaceProtocol = params->protocol;
    intf->iInterface = 0x00;
    *offset += 0x09;
    return 0;
}

/* Initialize Endpoint Descriptor */
uint8_t init_endpoint_descriptor(uint8_t *buffer, uint16_t *offset, const EndpointDescParams *params)
{
    if (!buffer || !params || !offset) return 1; /* Invalid parameters */

    USB_EndpointDescriptor *ep = (USB_EndpointDescriptor *)(buffer + *offset);
    ep->bLength = 0x07;
    ep->bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE;
    ep->bEndpointAddress = params->endpoint_address;
    ep->bmAttributes = params->attributes;
    ep->wMaxPacketSize = params->max_packet_size;
    ep->bInterval = params->interval;
    *offset += 0x07;
    return 0;
}

/* Initialize HID Descriptor */
uint8_t init_hid_descriptor(uint8_t *buffer, uint16_t *offset, const ClassSpecificParams *class_params)
{
    if (!buffer || !offset || !class_params || class_params->class != 0x03) return 1;

    USB_HIDDescriptor *hid = (USB_HIDDescriptor *)(buffer + *offset);
    hid->bLength = 0x09;
    hid->bDescriptorType = USB_HID_DESCRIPTOR_TYPE; /* 0x21 */
    hid->bcdHID = 0x0111; /* HID 1.11 */
    hid->bCountryCode = 0x00;
    hid->bNumDescriptors = 0x01; /* One report descriptor */
    hid->bReportDescriptorType = USB_HID_REPORT_DESCRIPTOR_TYPE; /* 0x22 */
    hid->wReportDescriptorLength = class_params->data.hid.report_desc_size;
    *offset += 0x09;

    /* Store report descriptor for GET_DESCRIPTOR requests */
    if (USBD_HIDReportDescriptor[0]) free(USBD_HIDReportDescriptor[0]);
    USBD_HIDReportDescriptor[0] = (uint8_t *)malloc(class_params->data.hid.report_desc_size);
    if (!USBD_HIDReportDescriptor[0]) return 1;
    memcpy(USBD_HIDReportDescriptor, class_params->data.hid.report_descriptor, class_params->data.hid.report_desc_size);
    USBD_HIDReportDescSize = class_params->data.hid.report_desc_size;

    return 0;
}

/* Initialize CDC Functional Descriptors */
static uint8_t init_cdc_functional_descriptors(uint8_t *buffer, uint16_t *offset, const ClassSpecificParams *class_params)
{
    if (!buffer || !offset || !class_params || class_params->class != 0x02) return 1;

    /* Header Functional Descriptor */
    USB_CDCHeaderFunctionalDescriptor *header = (USB_CDCHeaderFunctionalDescriptor *)(buffer + *offset);
    header->bFunctionLength = 0x05;
    header->bDescriptorType = 0x24;
    header->bDescriptorSubtype = 0x00;
    header->bcdCDC = 0x0110;
    *offset += 0x05;

    /* Call Management Functional Descriptor */
    USB_CDCCallManagementDescriptor *call = (USB_CDCCallManagementDescriptor *)(buffer + *offset);
    call->bFunctionLength = 0x05;
    call->bDescriptorType = 0x24;
    call->bDescriptorSubtype = 0x01;
    call->bmCapabilities = class_params->data.cdc.capabilities;
    call->bDataInterface = class_params->data.cdc.data_interface;
    *offset += 0x05;

    /* Abstract Control Management Functional Descriptor */
    uint8_t *acm = buffer + *offset;
    acm[0] = 0x04; /* bFunctionLength */
    acm[1] = 0x24; /* bDescriptorType */
    acm[2] = 0x02; /* bDescriptorSubtype */
    acm[3] = 0x02; /* bmCapabilities */
    *offset += 0x04;

    /* Union Functional Descriptor */
    USB_CDCUnionDescriptor *union_desc = (USB_CDCUnionDescriptor *)(buffer + *offset);
    union_desc->bFunctionLength = 0x05;
    union_desc->bDescriptorType = 0x24;
    union_desc->bDescriptorSubtype = 0x06;
    union_desc->bMasterInterface = 0x00;
    union_desc->bSlaveInterface0 = class_params->data.cdc.data_interface;
    *offset += 0x05;

    return 0;
}

/* Initialize MSC Descriptors (Placeholder) */
static uint8_t init_msc_descriptors(uint8_t *buffer, uint16_t *offset, const ClassSpecificParams *class_params)
{
    if (!buffer || !offset || !class_params || class_params->class != 0x08) return 1;
    return 0;
}

/* Initialize Class-Specific Descriptors */
static uint8_t init_class_specific_descriptors(uint8_t *buffer, uint16_t *offset, const InterfaceDescParams *intf_params, const ClassSpecificParams *class_params)
{
    if (!buffer || !offset || !intf_params || !class_params) return 1;

    switch (intf_params->class) {
        case 0x03: /* HID */
            return init_hid_descriptor(buffer, offset, class_params);
        case 0x02: /* CDC */
            return init_cdc_functional_descriptors(buffer, offset, class_params);
        case 0x08: /* MSC */
            return init_msc_descriptors(buffer, offset, class_params);
        default:
            return 0; /* No class-specific descriptors */
    }
}

/* Compute Configuration Descriptor Size */
uint16_t compute_config_desc_size(const ConfigDescParams *config_params,
                                        const InterfaceDescParams *interfaces, uint8_t num_interfaces,
                                        const EndpointDescParams *endpoints, uint8_t *num_endpoints_per_interface,
                                        const ClassSpecificParams *class_params)
{
    if (!config_params || !interfaces || !num_endpoints_per_interface || !class_params) return 0;

    uint16_t size = 9; /* Configuration descriptor */
    uint8_t total_endpoints = 0;

    for (uint8_t i = 0; i < num_interfaces; i++) {
        size += 9; /* Interface descriptor */
        switch (interfaces[i].class) {
            case 0x03: /* HID */
                size += 9; /* HID descriptor */
                break;
            case 0x02: /* CDC */
                if (interfaces[i].is_cdc_control) {
                    size += 19; /* CDC functional descriptors (5+5+4+5) */
                }
                break;
            case 0x08: /* MSC */
                /* No additional descriptors typically needed */
                break;
        }
        total_endpoints += num_endpoints_per_interface[i];
        size += num_endpoints_per_interface[i] * 7; /* Endpoint descriptors */
    }

    return size;
}

/* Initialize Configuration Descriptor */
uint8_t init_config_descriptor(const ConfigDescParams *config_params,
                                     const InterfaceDescParams *interfaces, uint8_t num_interfaces,
                                     const EndpointDescParams *endpoints, uint8_t *num_endpoints_per_interface,
                                     const ClassSpecificParams *class_params)
{
    if (!config_params || !interfaces || !num_endpoints_per_interface || !class_params) return 1;

    /* Compute required size */
    USBD_ConfigDescSize = compute_config_desc_size(config_params, interfaces, num_interfaces, endpoints, num_endpoints_per_interface, class_params);
    if (USBD_ConfigDescSize == 0) return 1;

    /* Allocate memory */
    if (USBD_ConfigDescriptor) free(USBD_ConfigDescriptor);
    USBD_ConfigDescriptor = (uint8_t *)malloc(USBD_ConfigDescSize);
    if (!USBD_ConfigDescriptor) return 1;

    uint16_t offset = 0;

    /* Configuration Descriptor */
    USB_ConfigDescriptor *cfg = (USB_ConfigDescriptor *)(USBD_ConfigDescriptor + offset);
    cfg->bLength = 0x09;
    cfg->bDescriptorType = USB_CONFIGURATION_DESCRIPTOR_TYPE;
    cfg->wTotalLength = USBD_ConfigDescSize;
    cfg->bNumInterfaces = config_params->num_interfaces;
    cfg->bConfigurationValue = config_params->config_value;
    cfg->iConfiguration = 0x00;
    cfg->bmAttributes = config_params->attributes;
    cfg->bMaxPower = config_params->max_power;
    offset += 0x09;

    uint8_t ep_index = 0;
    for (uint8_t i = 0; i < num_interfaces; i++) {
        if (init_interface_descriptor(USBD_ConfigDescriptor, &offset, &interfaces[i])) {
            free(USBD_ConfigDescriptor);
            USBD_ConfigDescriptor = NULL;
            return 1;
        }

        if (init_class_specific_descriptors(USBD_ConfigDescriptor, &offset, &interfaces[i], &class_params[i])) {
            free(USBD_ConfigDescriptor);
            USBD_ConfigDescriptor = NULL;
            return 1;
        }

        for (uint8_t j = 0; j < num_endpoints_per_interface[i]; j++) {
            if (init_endpoint_descriptor(USBD_ConfigDescriptor, &offset, &endpoints[ep_index])) {
                free(USBD_ConfigDescriptor);
                USBD_ConfigDescriptor = NULL;
                return 1;
            }
            ep_index++;
        }
    }

    return 0;
}

/* Initialize String Descriptors */
uint8_t init_string_descriptors(const StringDescParams *params)
{
    if (!params) return 1;

    /* Language ID */
    USB_StringDescriptor *lang = (USB_StringDescriptor *)USBD_StringLangID;
    lang->bLength = USBD_SIZE_STRING_LANGID;
    lang->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
    lang->wString[0] = params->lang_id;

    /* Vendor String */
    if (USBD_StringVendor) free(USBD_StringVendor);
    USBD_StringVendorSize = 0;
    USBD_StringVendor = (uint8_t *)malloc(USBD_MAX_STRING_LEN);
    if (!USBD_StringVendor) return 1;
    USB_StringDescriptor *vendor = (USB_StringDescriptor *)USBD_StringVendor;
    vendor->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
    if (ascii_to_utf16le(params->vendor_str, vendor->wString, &USBD_StringVendorSize)) {
        free(USBD_StringVendor);
        USBD_StringVendor = NULL;
        return 1;
    }
    vendor->bLength = USBD_StringVendorSize;

    /* Product String */
    if (USBD_StringProduct) free(USBD_StringProduct);
    USBD_StringProductSize = 0;
    USBD_StringProduct = (uint8_t *)malloc(USBD_MAX_STRING_LEN);
    if (!USBD_StringProduct) {
        free(USBD_StringVendor);
        USBD_StringVendor = NULL;
        return 1;
    }
    USB_StringDescriptor *product = (USB_StringDescriptor *)USBD_StringProduct;
    product->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
    if (ascii_to_utf16le(params->product_str, product->wString, &USBD_StringProductSize)) {
        free(USBD_StringVendor);
        free(USBD_StringProduct);
        USBD_StringVendor = NULL;
        USBD_StringProduct = NULL;
        return 1;
    }
    product->bLength = USBD_StringProductSize;

    /* Serial Number String */
    if (USBD_StringSerial) free(USBD_StringSerial);
    USBD_StringSerialSize = 0;
    USBD_StringSerial = (uint8_t *)malloc(USBD_MAX_STRING_LEN);
    if (!USBD_StringSerial) {
        free(USBD_StringVendor);
        free(USBD_StringProduct);
        USBD_StringVendor = NULL;
        USBD_StringProduct = NULL;
        return 1;
    }
    USB_StringDescriptor *serial = (USB_StringDescriptor *)USBD_StringSerial;
    serial->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
    if (ascii_to_utf16le(params->serial_str, serial->wString, &USBD_StringSerialSize)) {
        free(USBD_StringVendor);
        free(USBD_StringProduct);
        free(USBD_StringSerial);
        USBD_StringVendor = NULL;
        USBD_StringProduct = NULL;
        USBD_StringSerial = NULL;
        return 1;
    }
    serial->bLength = USBD_StringSerialSize;

    return 0;
}

/* Initialize USB Descriptors */
uint8_t usbd_descriptors_init(const DeviceDescParams *dev_params,
                             const ConfigDescParams *config_params,
                             const InterfaceDescParams *interfaces, uint8_t num_interfaces,
                             const EndpointDescParams *endpoints, uint8_t *num_endpoints_per_interface,
                             const StringDescParams *str_params,
                             const ClassSpecificParams *class_params)
{
    if (init_device_descriptor(dev_params) ||
        init_config_descriptor(config_params, interfaces, num_interfaces, endpoints, num_endpoints_per_interface, class_params) ||
        init_string_descriptors(str_params)) {
        return 1;
    }
    return 0;
}

uint8_t usbd_init_test(void)
{
    /* Device parameters */
    DeviceDescParams dev_params;
    memset(&dev_params, 0, sizeof(dev_params));
    dev_params.device_version = 1001;     /* bcdDevice (example) */
    dev_params.max_packet_size = 64;      /* EP0 max packet size */
    dev_params.product_id = 0x1234;
    dev_params.vendor_id  = 0x5678;

    /* Configuration parameters */
    ConfigDescParams config_params;
    memset(&config_params, 0, sizeof(config_params));
    config_params.config_value = 0x01;
    config_params.attributes   = 0x80;    /* Bus powered (0x80) - change to 0xC0 if self-powered */
    config_params.max_power    = 0x32;    /* In 2mA units -> 0x32 = 100mA */
    config_params.num_interfaces = 1;

    /* Interface: single HID interface */
    InterfaceDescParams interfaces[1];
    memset(interfaces, 0, sizeof(interfaces));
    interfaces[0].interface_number = 0x00;
    interfaces[0].num_endpoints = 1;
    interfaces[0].class = 0x03;       /* HID */
    interfaces[0].subclass = 0x01;    /* Boot Interface Subclass (typical) */
    interfaces[0].protocol = 0x01;    /* Keyboard (1) */

    /* Endpoint: Interrupt IN (0x81), 64 bytes, interval 10ms */
    EndpointDescParams endpoints[1];
    memset(endpoints, 0, sizeof(endpoints));
    endpoints[0].endpoint_address = 0x81; /* IN endpoint 1 */
    endpoints[0].attributes = 0x03;       /* Interrupt */
    endpoints[0].max_packet_size = 64;
    endpoints[0].interval = 10;

    /* Number of endpoints per interface */
    uint8_t num_endpoints_per_interface[1];
    num_endpoints_per_interface[0] = 1;

    /* Class-specific parameters for HID (report descriptor) */
    ClassSpecificParams class_params[1];
    memset(class_params, 0, sizeof(class_params));

    /* Small keyboard report descriptor (common example) */
    static const uint8_t hid_report_desc[] = {
        0x05, 0x01, /* Usage Page (Generic Desktop) */
        0x09, 0x06, /* Usage (Keyboard) */
        0xA1, 0x01, /* Collection (Application) */
        0x05, 0x07, /*   Usage Page (Key Codes) */
        0x19, 0xE0, /*   Usage Minimum (224) */
        0x29, 0xE7, /*   Usage Maximum (231) */
        0x15, 0x00, /*   Logical Minimum (0) */
        0x25, 0x01, /*   Logical Maximum (1) */
        0x75, 0x01, /*   Report Size (1) */
        0x95, 0x08, /*   Report Count (8) */
        0x81, 0x02, /*   Input (Data,Var,Abs) */
        0x95, 0x01, /*   Report Count (1) */
        0x75, 0x08, /*   Report Size (8) */
        0x81, 0x01, /*   Input (Const) */
        0x95, 0x05, /*   Report Count (5) */
        0x75, 0x01, /*   Report Size (1) */
        0x05, 0x08, /*   Usage Page (LEDs) */
        0x19, 0x01, /*   Usage Minimum (1) */
        0x29, 0x05, /*   Usage Maximum (5) */
        0x91, 0x02, /*   Output (Data,Var,Abs) */
        0x95, 0x01, /*   Report Count (1) */
        0x75, 0x03, /*   Report Size (3) */
        0x91, 0x01, /*   Output (Const) */
        0xC0        /* End Collection */
    };

    /* Fill HID class params (structure fields used in your code) */
    class_params[0].class = 0x03; /* HID */
    class_params[0].data.hid.report_descriptor = (uint8_t *)hid_report_desc;
    class_params[0].data.hid.report_desc_size = (uint16_t)sizeof(hid_report_desc);

    /* Strings */
    StringDescParams str_params;
    memset(&str_params, 0, sizeof(str_params));
    str_params.lang_id = 0x0409; /* English (United States) */
    str_params.vendor_str = "TestVendor";
    str_params.product_str = "TestHIDDevice";
    str_params.serial_str = "TESTSN0001";

    /* Call the main initialization that builds and stores descriptors */
    uint8_t rc = usbd_descriptors_init(&dev_params,
                                      &config_params,
                                      interfaces, (uint8_t)config_params.num_interfaces,
                                      endpoints,
                                      num_endpoints_per_interface,
                                      &str_params,
                                      class_params);

    return rc;
}