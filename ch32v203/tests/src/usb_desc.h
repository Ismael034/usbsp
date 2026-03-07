#ifndef __TEST_USB_DESC_H
#define __TEST_USB_DESC_H

#include "../../src/src/usb/usbd/usb_desc.h"

typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version;
    uint8_t max_packet_size;
} DeviceDescParams;

typedef struct {
    uint8_t num_interfaces;
    uint8_t config_value;
    uint8_t max_power;
    uint8_t attributes;
} ConfigDescParams;

typedef struct {
    uint8_t interface_number;
    uint8_t class;
    uint8_t subclass;
    uint8_t protocol;
    uint8_t num_endpoints;
    uint8_t is_cdc_control;
} InterfaceDescParams;

typedef struct {
    uint8_t endpoint_address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} EndpointDescParams;

typedef struct {
    const char *vendor_str;
    const char *product_str;
    const char *serial_str;
    uint16_t lang_id;
} StringDescParams;

typedef struct {
    uint8_t class;
    union {
        struct {
            const uint8_t *report_descriptor;
            uint16_t report_desc_size;
        } hid;
    } data;
} ClassSpecificParams;

uint8_t usbd_test_descriptors_init(const DeviceDescParams *dev_params,
                                   const ConfigDescParams *config_params,
                                   const InterfaceDescParams *interfaces,
                                   uint8_t num_interfaces,
                                   const EndpointDescParams *endpoints,
                                   uint8_t *num_endpoints_per_interface,
                                   const StringDescParams *str_params,
                                   const ClassSpecificParams *class_params);

#endif
