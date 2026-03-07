/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_mem.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2021/08/08
 * Description        : Utility functions for memory transfers to/from PMA
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/ 
#include "usb_lib.h"

#define USB_PMA_ADDR_LIMIT   (0x0200u)

static uint8_t usb_pma_addr_is_valid(uint16_t pma_addr, uint16_t nbytes)
{
  uint32_t words;
  uint32_t end_addr;

  if ((pma_addr == 0xFFFFu) || ((pma_addr & 0x0001u) != 0u) || (pma_addr >= USB_PMA_ADDR_LIMIT))
  {
    return 0u;
  }

  words = ((uint32_t)nbytes + 1u) >> 1;
  end_addr = (uint32_t)pma_addr + (words << 1);
  if (end_addr > USB_PMA_ADDR_LIMIT)
  {
    return 0u;
  }

  return 1u;
}


/*******************************************************************************
 * @fn           UserToPMABufferCopy
 *
 * @brief        Copy a buffer from user memory area to packet memory area (PMA)
 *
 * @param        pbUsrBuf: pointer to user memory area.
 *                  wPMABufAddr: address into PMA.
 *                  wNBytes: no. of bytes to be copied.
 *
 * @param        None	.
 */
void UserToPMABufferCopy(uint8_t *pbUsrBuf, uint16_t wPMABufAddr, uint16_t wNBytes)
{
  volatile uint16_t *pma_word;

  if ((pbUsrBuf == NULL) || (wNBytes == 0u) || (usb_pma_addr_is_valid(wPMABufAddr, wNBytes) == 0u))
  {
    return;
  }

  pma_word = (volatile uint16_t *)(PMAAddr + ((uint32_t)wPMABufAddr * 2u));

  while (wNBytes > 1u)
  {
    uint16_t value = (uint16_t)pbUsrBuf[0] | ((uint16_t)pbUsrBuf[1] << 8);
    *pma_word = value;
    pma_word += 2;
    pbUsrBuf += 2;
    wNBytes -= 2;
  }

  if (wNBytes != 0u)
  {
    *pma_word = pbUsrBuf[0];
  }
}

/*******************************************************************************
 * @fn          PMAToUserBufferCopy
 *
 * @brief       Copy a buffer from user memory area to packet memory area (PMA)
 *
 * @param       pbUsrBuf: pointer to user memory area.
 *                  wPMABufAddr: address into PMA.
 *                  wNBytes:  no. of bytes to be copied.
 *
 * @param       None. 
 */
void PMAToUserBufferCopy(uint8_t *pbUsrBuf, uint16_t wPMABufAddr, uint16_t wNBytes)
{
  volatile uint16_t *pma_word;

  if ((pbUsrBuf == NULL) || (wNBytes == 0u) || (usb_pma_addr_is_valid(wPMABufAddr, wNBytes) == 0u))
  {
    return;
  }

  pma_word = (volatile uint16_t *)(PMAAddr + ((uint32_t)wPMABufAddr * 2u));

  while (wNBytes > 1u)
  {
    uint16_t value = *pma_word;
    pbUsrBuf[0] = (uint8_t)(value & 0x00FFu);
    pbUsrBuf[1] = (uint8_t)(value >> 8);
    pma_word += 2;
    pbUsrBuf += 2;
    wNBytes -= 2;
  }

  if (wNBytes != 0u)
  {
    uint16_t value = *pma_word;
    pbUsrBuf[0] = (uint8_t)(value & 0x00FFu);
  }
}





