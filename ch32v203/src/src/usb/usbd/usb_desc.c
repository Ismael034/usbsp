#include "usb_desc.h"

uint8_t USBD_DeviceDescriptor[USBD_SIZE_DEVICE_DESC];
uint8_t *USBD_BOSDescriptor = NULL;
uint16_t USBD_BOSDescriptorSize = 0;
uint8_t *USBD_ConfigDescriptor = NULL;
uint16_t USBD_ConfigDescSize = 0;

USBD_StringDescriptor_s USBD_StringDescriptor[4];
uint8_t USBD_StringLangID[USBD_SIZE_STRING_LANGID];
uint8_t *USBD_StringVendor = NULL;
uint8_t *USBD_StringProduct = NULL;
uint8_t *USBD_StringSerial = NULL;
uint8_t USBD_StringVendorSize = 0;
uint8_t USBD_StringProductSize = 0;
uint8_t USBD_StringSerialSize = 0;

uint8_t *USBD_HIDReportDescriptor[MAX_USB_INTERFACES];
uint16_t USBD_HIDReportDescSize = 0;
