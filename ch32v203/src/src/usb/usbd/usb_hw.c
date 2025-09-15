#include "usb_hw.h"

void USBWakeUp_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USB_LP_CAN1_RX0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void USBWakeUp_IRQHandler(void)
{
    EXTI_ClearITPendingBit(EXTI_Line18);
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    usbd_istr();
}

void usbd_hw_set_clk(void)
{
    RCC_ClocksTypeDef RCC_ClocksStatus= {0};
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

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
}

void usb_hw_set_lpm(void)
{
 	printf_usbd_debug("enter low power mode\r\n");
	bDeviceState = SUSPENDED;
}

void usb_hw_leave_lpm(void)
{
	DEVICE_INFO *pInfo = &Device_Info;
	printf_usbd_debug("leave low power mode\r\n"); 
	if (pInfo->Current_Configuration!=0){
		bDeviceState = CONFIGURED;
	}
	else bDeviceState = ATTACHED; 
}

void usb_hw_set_isr_config(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;

	EXTI_ClearITPendingBit(EXTI_Line18);
	EXTI_InitStructure.EXTI_Line = EXTI_Line18; 
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;	
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure); 	 

	NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;	
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = USBWakeUp_IRQn;  
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_Init(&NVIC_InitStructure);   
}

void usb_hw_set_port(FunctionalState NewState, FunctionalState Pin_In_IPU)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	if(NewState) {
		_SetCNTR(_GetCNTR()&(~(1<<1)));
		GPIOA->CFGHR&=0XFFF00FFF;
		GPIOA->OUTDR&=~(3<<11);	//PA11/12=0
		GPIOA->CFGHR|=0X00044000; //float
	}
	else
	{	  
		_SetCNTR(_GetCNTR()|(1<<1));  
		GPIOA->CFGHR&=0XFFF00FFF;
		GPIOA->OUTDR&=~(3<<11);	//PA11/12=0
		GPIOA->CFGHR|=0X00033000;	// LOW
	}
	
	if(Pin_In_IPU) (EXTEN->EXTEN_CTR) |= EXTEN_USBD_PU_EN; 
	else (EXTEN->EXTEN_CTR) &= ~EXTEN_USBD_PU_EN;
}