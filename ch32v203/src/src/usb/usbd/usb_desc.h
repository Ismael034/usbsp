#ifndef __USB_DESC_H
#define __USB_DESC_H

#include "ch32v20x.h"

#define USB_DEVICE_DESCRIPTOR_TYPE              0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE       0x02
#define USB_STRING_DESCRIPTOR_TYPE              0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE           0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE            0x05
#define USB_HID_DESCRIPTOR_TYPE                 0x21
#define USB_HID_REPORT_DESCRIPTOR_TYPE          0x22

#define DEF_USBD_UEP0_SIZE          64
#define DEF_USBD_MAX_PACK_SIZE      64
#define USBD_SIZE_DEVICE_DESC       18
#define USBD_SIZE_STRING_LANGID     4
#define USBD_MAX_STRING_LEN         255
#define USBD_MAX_BUFF_LEN           1024

#define MAX_USB_INTERFACES 8
#define MAX_USB_IN_ENDPOINTS 8

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
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed)) USB_EndpointDescriptor;

typedef struct {
    uint8_t USBD_StringDescriptor[USBD_MAX_BUFF_LEN];
    uint16_t USBD_StringDescriptorSize;
} USBD_StringDescriptor_s;

extern uint8_t USBD_DeviceDescriptor[USBD_SIZE_DEVICE_DESC];
extern uint8_t *USBD_BOSDescriptor;
extern uint16_t USBD_BOSDescriptorSize;
extern uint8_t *USBD_ConfigDescriptor;
extern uint16_t USBD_ConfigDescSize;

extern USBD_StringDescriptor_s USBD_StringDescriptor[4];
extern uint8_t USBD_StringLangID[USBD_SIZE_STRING_LANGID];
extern uint8_t *USBD_StringVendor;
extern uint8_t *USBD_StringProduct;
extern uint8_t *USBD_StringSerial;
extern uint8_t USBD_StringVendorSize;
extern uint8_t USBD_StringProductSize;
extern uint8_t USBD_StringSerialSize;

extern uint8_t *USBD_HIDReportDescriptor[MAX_USB_INTERFACES];
extern uint16_t USBD_HIDReportDescSize;

#endif
