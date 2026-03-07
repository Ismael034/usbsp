/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_sil.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2021/08/08
 * Description        : Simplified Interface Layer for Global Initialization and 
 *			           Endpoint  Rea/Write operations.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/ 
#include "usb_lib.h"


/*******************************************************************************
 * @fn         USB_SIL_Init
 *
 * @brief      Initialize the USB Device IP and the Endpoint 0.
 *
 * @return   Status.
 */
uint32_t USB_SIL_Init(uint16_t imrMask)
{
  _SetISTR(0);
  wInterrupt_Mask = imrMask;
  _SetCNTR(wInterrupt_Mask);
	
  return 0;
}

/*******************************************************************************
 * @fn                USB_SIL_Write
 *
 * @brief           Write a buffer of data to a selected endpoint.
 *
 * @param          bEpAddr: The address of the non control endpoint.
 *                  pBufferPointer: The pointer to the buffer of data to be written
 *                     to the endpoint.
 *                  wBufferSize: Number of data to be written (in bytes).
 *
 * @return         Status.
 */
uint32_t USB_SIL_Write(uint8_t bEpAddr, uint8_t* pBufferPointer, uint32_t wBufferSize)
{
  UserToPMABufferCopy(pBufferPointer, GetEPTxAddr(bEpAddr & 0x7F), wBufferSize);
  SetEPTxCount((bEpAddr & 0x7F), wBufferSize);
  
  return 0;
}

/*******************************************************************************
 * @fn       USB_SIL_Read
 *
 * @brief     Write a buffer of data to a selected endpoint.
 *
 * @param    bEpAddr: The address of the non control endpoint.
 *                  pBufferPointer: The pointer to which will be saved the 
 *             received data buffer.
 *
 * @return     Number of received data (in Bytes).
 */
uint32_t USB_SIL_Read(uint8_t bEpAddr, uint8_t* pBufferPointer)
{
  uint8_t ep_num;
  uint16_t rx_addr;
  uint32_t DataLength = 0;

  if (pBufferPointer == NULL)
  {
    return 0;
  }

  ep_num = bEpAddr & 0x7F;
  if (ep_num > ENDP7)
  {
    return 0;
  }

  DataLength = GetEPRxCount(ep_num);
  if (DataLength == 0)
  {
    return 0;
  }

  rx_addr = GetEPRxAddr(ep_num);
  if ((rx_addr == 0xFFFFu) || ((rx_addr & 0x0001u) != 0u) || (rx_addr >= 0x0200u))
  {
    return 0;
  }

  PMAToUserBufferCopy(pBufferPointer, rx_addr, (uint16_t)DataLength);

  return DataLength;
}





