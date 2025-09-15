
#include "usb_hw.h"

void usbh_hw_set_clk(void)
{
    RCC_ClocksTypeDef RCC_ClocksStatus = {0};
    RCC_GetClocksFreq(&RCC_ClocksStatus);
    if(RCC_ClocksStatus.SYSCLK_Frequency == 144000000)
    {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div3);
    }
    else if(RCC_ClocksStatus.SYSCLK_Frequency == 96000000) 
    {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div2);
    }
    else if(RCC_ClocksStatus.SYSCLK_Frequency == 48000000) 
    {
        RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div1);
    }
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, ENABLE);
}

void tim3_init(uint16_t arr, uint16_t psc)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = { 0 };
    NVIC_InitTypeDef NVIC_InitStructure = { 0 };

    /* Enable timer3 clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* Initialize timer3 */
    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    /* Enable updating timer3 interrupt */
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    /* Configure timer3 interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* Enable timer3 */
    TIM_Cmd(TIM3, ENABLE);

    /* Enable timer3 interrupt */
    NVIC_EnableIRQ(TIM3_IRQn);
}