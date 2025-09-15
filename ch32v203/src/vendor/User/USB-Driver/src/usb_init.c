/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_init.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2021/08/08
 * Description        : Initialization routines & global variables
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/ 
#include "usb_lib.h"

uint8_t	EPindex;
DEVICE_INFO *pInformation;
DEVICE *dTable;
DEVICE_PROP *pProperty;
uint16_t	SaveState ;
uint16_t  wInterrupt_Mask;
DEVICE_INFO	Device_Info;
USER_STANDARD_REQUESTS  *pUser_Standard_Requests;

/*******************************************************************************
 * @fn        USB_Init
 *
 * @brief     USB system initialization
 *
 * @return    None.
 *
 */
void USB_Init(DEVICE_INFO *Device_Info, DEVICE *device_table, DEVICE_PROP *Device_Property, USER_STANDARD_REQUESTS *User_Standard_Requests)
{
  pInformation = Device_Info;
  pInformation->ControlState = 2;
  dTable = device_table;
  pProperty = Device_Property;
  pUser_Standard_Requests = User_Standard_Requests;
  pProperty->Init();
}





