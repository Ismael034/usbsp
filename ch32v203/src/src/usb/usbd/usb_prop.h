#ifndef __USB_PROP_H
#define __USB_PROP_H

#include "ch32v20x.h"
#include "usbd.h"

#define CDC_GET_LINE_CODING         0x21                                      /* This request allows the host to find out the currently configured line coding */
#define CDC_SET_LINE_CODING         0x20                                      /* Configures DTE rate, stop-bits, parity, and number-of-character */
#define CDC_SET_LINE_CTLSTE         0x22                                      /* This request generates RS-232/V.24 style control signals */
#define CDC_SEND_BREAK              0x23      

#define HID_SET_IDLE 0x0A
#define HID_SET_PROTOCOL 0x0B

#define usbd_get_configuration          NOP_Process
// #define usbd_set_cofiguration          NOP_Process
#define usbd_get_interface              NOP_Process
#define usbd_set_interface              NOP_Process
#define usbd_get_status                 NOP_Process
// #define usbd_clear_feature              NOP_Process
#define usbd_set_endpoint_feature        NOP_Process
// #define usbd_set_device_feature          NOP_Process
// #define usbd_set_device_address          NOP_Process

typedef struct _ep_config {
    uint8_t ep_num;
    uint16_t ep_type;

    uint16_t ep_tx_status;
    uint16_t ep_tx_addr;
    uint16_t ep_tx_count;

    uint16_t ep_rx_status;
    uint16_t ep_rx_addr;
    uint16_t ep_rx_count;
} ep_config;


void usbd_init(void);
void usbd_init_test(void);
void usbd_reset(void);
void usbd_status_in(void);
void usbd_status_out(void);
uint8_t *usbd_get_device_descriptor(uint16_t Length);
uint8_t *usbd_get_config_descriptor(uint16_t Length);
uint8_t *usbd_get_string_descriptor(uint16_t Length);
uint8_t *usbd_get_hid_descriptor(uint16_t length);
uint8_t *usbd_get_hid_report_descriptor(uint16_t length);

void usbd_set_endpoint_config(ep_config *ep_config, uint8_t endpoints);

void usbd_set_device_address(void);
void usbd_set_device_feature(void);
void usbd_set_configuration(void);
void usbd_clear_feature(void);

RESULT usbd_data_setup(uint8_t);
RESULT usbd_nodata_setup(uint8_t);
RESULT usbd_get_interface_setting(uint8_t Interface, uint8_t AlternateSetting);

uint8_t USBD_ENDP_DataUp( uint8_t endp, uint8_t *pbuf, uint16_t len );

#endif /* __usb_prop_H */