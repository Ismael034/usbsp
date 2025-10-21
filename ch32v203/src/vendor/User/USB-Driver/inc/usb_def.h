/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_def.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2021/08/08
 * Description        : This file contains all the functions prototypes for the  
 *                      USB definition firmware library.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/ 
#ifndef __USB_DEF_H
#define __USB_DEF_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v20x.h"

typedef enum _RECIPIENT_TYPE
{
  DEVICE_RECIPIENT,     
  INTERFACE_RECIPIENT,  
  ENDPOINT_RECIPIENT,   
  OTHER_RECIPIENT
} RECIPIENT_TYPE;

typedef enum _STANDARD_REQUESTS
{
  GET_STATUS = 0,
  CLEAR_FEATURE,
  RESERVED1,
  SET_FEATURE,
  RESERVED2,
  SET_ADDRESS,
  GET_DESCRIPTOR,
  SET_DESCRIPTOR,
  GET_CONFIGURATION,
  SET_CONFIGURATION,
  GET_INTERFACE,
  SET_INTERFACE,
  TOTAL_sREQUEST,  
  SYNCH_FRAME = 12
} STANDARD_REQUESTS;


typedef enum _HID_CLASS_REQUEST
{
	HID_REQ_GET_REPORT		= 0x01,
	HID_REQ_GET_IDLE		= 0x02,
	HID_REQ_GET_PROTOCOL		= 0x03,
	HID_REQ_SET_REPORT		= 0x09,
	HID_REQ_SET_IDLE		= 0x0A,
	HID_REQ_SET_PROTOCOL		= 0x0B
} HID_CLASS_REQUEST;

/* Definition of "USBwValue" */
typedef enum _DESCRIPTOR_TYPE
{
  USB_DEVICE_DESCRIPTOR = 0x1,
  USB_CONFIG_DESCRIPTOR = 0x2,
  USB_STRING_DESCRIPTOR = 0x3,
  USB_INTERFACE_DESCRIPTOR = 0x4,
  USB_ENDPOINT_DESCRIPTOR = 0x5,
  USB_DEVICE_QUALIFIER_DESCRIPTOR = 0x6,
  USB_OTHER_SPEED_CONFIGURATION_DESCRIPTOR = 0x7,
  USB_INTERFACE_POWER_DESCRIPTOR = 0x8,
  USB_OTG_DESCRIPTOR_DESCRIPTOR = 0x9,
  USB_DEBUG_DESCRIPTOR = 0xa,
  USB_INTERFACE_ASSOCIATION_DESCRIPTOR = 0xb,
  USB_BOS_DESCRIPTOR = 0xf,
  USB_DEVICE_CAPABILITY_DESCRIPTOR = 0x10,
  USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR = 0x30,
  USB_20_HUB_DESCRIPTOR_DESCRIPTOR = 0x29,
  USB_30_HUB_DESCRIPTOR_DESCRIPTOR = 0x2a,
  USB_SUPERSPEEDPLUS_ISOCH_ENDPOINT_COMPANION_DESCRIPTOR = 0x31,

  USB_HID_DESCRIPTOR = 0x21,
  USB_HID_REPORT_DESCRIPTOR = 0x22

} DESCRIPTOR_TYPE;

/* Feature selector of a SET_FEATURE or CLEAR_FEATURE */
typedef enum _FEATURE_SELECTOR
{
  ENDPOINT_STALL,
  DEVICE_REMOTE_WAKEUP
} FEATURE_SELECTOR;

/* Definition of "USBbmRequestType" */
#define REQUEST_TYPE      0x60  
#define STANDARD_REQUEST  0x00  
#define CLASS_REQUEST     0x20 
#define VENDOR_REQUEST    0x40  
#define RECIPIENT         0x1F  

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEF_H */






