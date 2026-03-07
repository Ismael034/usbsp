/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v20x_it.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/12/29
 * Description        : Main Interrupt Service Routines.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include "ch32v20x_it.h"
#include "debug_log.h"

volatile uint32_t g_hardfault_mcause = 0;
volatile uint32_t g_hardfault_mepc = 0;
volatile uint32_t g_hardfault_mtval = 0;

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 */
void NMI_Handler(void)
{
  while (1)
  {
  }
}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 *
 * @return  none
 */
void HardFault_Handler(void)
{
  g_hardfault_mcause = __get_MCAUSE();
  g_hardfault_mepc = __get_MEPC();
  g_hardfault_mtval = __get_MTVAL();

  LOG_ERROR("hardfault: mcause=0x%08lX mepc=0x%08lX mtval=0x%08lX",
            (unsigned long)g_hardfault_mcause,
            (unsigned long)g_hardfault_mepc,
            (unsigned long)g_hardfault_mtval);

  while (1)
  {
  }
}



