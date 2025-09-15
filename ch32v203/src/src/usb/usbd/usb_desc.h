#ifndef __USB_DESC_H
#define __USB_DESC_H

#include <string.h>
#include <stdlib.h>
#include "ch32v20x.h"

/* USB Descriptor Types */
#define USB_DEVICE_DESCRIPTOR_TYPE              0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE       0x02
#define USB_STRING_DESCRIPTOR_TYPE              0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE           0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE            0x05
#define USB_HID_DESCRIPTOR_TYPE                 0x21
#define USB_HID_REPORT_DESCRIPTOR_TYPE          0x22

/* USB Descriptor Constants */
#define DEF_USBD_UEP0_SIZE          64           /* Endpoint 0 size */
#define DEF_USBD_MAX_PACK_SIZE      64           /* Default max packet size */
#define USBD_SIZE_DEVICE_DESC       18           /* Device descriptor size */
#define USBD_SIZE_STRING_LANGID     4            /* Language ID string size */
#define USBD_MAX_STRING_LEN         255          /* Max string descriptor length */

#define MAX_USB_INTERFACES 8
#define MAX_USB_IN_ENDPOINTS 8

/* USB Descriptor Structures */
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
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} __attribute__((packed)) USB_ConfigDescriptor;

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
} __attribute__((packed)) USB_InterfaceDescriptor, *PUSB_InterfaceDescriptor;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed)) USB_EndpointDescriptor;

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
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdCDC;
} __attribute__((packed)) USB_CDCHeaderFunctionalDescriptor;

typedef struct {
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bmCapabilities;
    uint8_t bDataInterface;
} __attribute__((packed)) USB_CDCCallManagementDescriptor;

typedef struct {
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bMasterInterface;
    uint8_t bSlaveInterface0;
} __attribute__((packed)) USB_CDCUnionDescriptor;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wString[]; /* Variable-length UTF-16LE string */
} __attribute__((packed)) USB_StringDescriptor;

/* Parameter Structures for Descriptor Initialization */
typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version; /* BCD format (e.g., 0x0100 for 1.00) */
    uint8_t max_packet_size; /* Endpoint 0 max packet size */
} DeviceDescParams;

typedef struct {
    uint8_t num_interfaces;
    uint8_t config_value;
    uint8_t max_power; /* In 2mA units (e.g., 50 = 100mA) */
    uint8_t attributes; /* e.g., 0xC0 for self-powered */
} ConfigDescParams;

typedef struct {
    uint8_t interface_number;
    uint8_t class;
    uint8_t subclass;
    uint8_t protocol;
    uint8_t num_endpoints;
    uint8_t is_cdc_control; /* 1 if CDC control interface, 0 otherwise */
} InterfaceDescParams;

typedef struct {
    uint8_t endpoint_address; /* e.g., 0x81 for IN EP1 */
    uint8_t attributes;      /* e.g., 0x02 for Bulk */
    uint16_t max_packet_size;
    uint8_t interval;        /* Polling interval (ms) */
} EndpointDescParams;

typedef struct {
    const char *vendor_str;
    const char *product_str;
    const char *serial_str;
    uint16_t lang_id; /* e.g., 0x0409 for US English */
} StringDescParams;

typedef struct {
    uint8_t class; /* USB class code (e.g., 0x03 for HID, 0x02 for CDC) */
    union {
        struct { /* HID-specific */
            const uint8_t *report_descriptor;
            uint16_t report_desc_size;
        } hid;
        struct { /* CDC-specific */
            uint8_t data_interface; /* Data interface number for CDC */
            uint8_t capabilities; /* CDC capabilities */
        } cdc;
        struct { /* MSC-specific */
            uint8_t protocol; /* e.g., 0x50 for Bulk-Only Transport */
        } msc;
        /* Add other class-specific structs here (e.g., Audio, Video) */
    } data;
} ClassSpecificParams;

/* External Descriptor Arrays and Sizes */
extern uint8_t USBD_DeviceDescriptor[USBD_SIZE_DEVICE_DESC];
extern uint8_t *USBD_ConfigDescriptor;
extern uint16_t USBD_ConfigDescSize;

extern uint8_t *USBD_StringDescriptor[4];
extern uint8_t USBD_StringLangID[USBD_SIZE_STRING_LANGID];
extern uint8_t *USBD_StringVendor;
extern uint8_t *USBD_StringProduct;
extern uint8_t *USBD_StringSerial;
extern uint8_t USBD_StringVendorSize;
extern uint8_t USBD_StringProductSize;
extern uint8_t USBD_StringSerialSize;

extern uint8_t *USBD_HIDReportDescriptor[MAX_USB_INTERFACES];
extern uint16_t USBD_HIDReportDescSize;

/* Function Prototypes */

uint8_t init_device_descriptor(const DeviceDescParams *params);

uint8_t usbd_descriptors_init(const DeviceDescParams *dev_params,
                             const ConfigDescParams *config_params,
                             const InterfaceDescParams *interfaces, uint8_t num_interfaces,
                             const EndpointDescParams *endpoints, uint8_t *num_endpoints_per_interface,
                             const StringDescParams *str_params,
                             const ClassSpecificParams *class_params);

#endif