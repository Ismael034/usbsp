/********************************** (C) COPYRIGHT  *******************************
 * File Name          : app_km.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2020/04/30
 * Description        : This file contains all the function prototypes for the USB
 *                      firmware library.
*********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#ifndef __APP_H
#define __APP_H

/*******************************************************************************/
/* Header File */
#include "stdint.h"
#include "usb_relay.h"

/*******************************************************************************/
/* Macro Definition */
#define DEF_DEBUG_PRINTF            1

/* General */
#define DEF_COM_BUF_LEN             1024                                    

/* USB HID Device Interface Type */
#define DEC_KEY                     0x01
#define DEC_MOUSE                   0x02
#define DEC_UNKNOW                  0xFF

/* USB Keyboard Lighting Key */
#define DEF_KEY_NUM                 0x53
#define DEF_KEY_CAPS                0x39
#define DEF_KEY_SCROLL              0x47

/*******************************************************************************/
/* Variable Declaration */
extern uint8_t  DevDesc_Buf[];                                         
extern uint8_t  Com_Buf[];   

/*******************************************************************************/
/* Function Declaration */
extern void tim3_init(uint16_t arr, uint16_t psc);
extern void usbh_analyze_device_type(uint8_t *device_descriptor, uint8_t *config_descriptor, uint8_t *device_type);
extern uint8_t usbh_enumerate_root_device(void);
extern uint8_t km_analyze_config_descriptor(uint8_t host_index, uint8_t ep0_size);
extern void km_analyze_hid_report_descriptor(uint8_t host_index, uint8_t interface_index);
extern uint8_t km_deal_hid_report_descriptor(uint8_t host_index, uint8_t ep0_size);
extern uint8_t usbh_enumerate_hid_device(uint8_t host_index, uint8_t ep0_size);
extern uint8_t hub_analyze_config_descriptor(uint8_t host_index);
extern uint8_t hub_port_pre_enum1(uint8_t hub_port, uint8_t *buffer);
extern uint8_t hub_port_pre_enum2(uint8_t hub_port, uint8_t *buffer);
extern uint8_t hub_check_port_speed(uint8_t hub_port, uint8_t *buffer);
extern uint8_t usbh_enumerate_hub_port_device(uint8_t hub_port, uint8_t *device_address, uint8_t *device_type);
extern void kb_analyze_key_value(uint8_t host_index, uint8_t interface_index, uint8_t *data_buffer, uint16_t data_length);
extern uint8_t kb_set_report(uint8_t host_index, uint8_t ep0_size, uint8_t interface_index);
extern void usbh_main_deal(void);
extern uint8_t usbh_test(void);

#endif