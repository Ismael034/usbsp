#include "user.h"

#define DEBOUNCE_TICKS (50 * (SystemCoreClock / 1000))
static volatile uint32_t last_interrupt_time = 0;
volatile uint8_t button_pressed = 0;

void user_btn_init()
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    EXTI_InitTypeDef EXTI_InitStruct = {0};
    NVIC_InitTypeDef NVIC_InitStruct = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStruct.GPIO_Pin = BUTTON_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);

    GPIO_EXTILineConfig(BUTTON_PORT_SRC, BUTTON_PIN_SRC);

    EXTI_InitStruct.EXTI_Line = BUTTON_EXTI_LINE;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    NVIC_InitStruct.NVIC_IRQChannel = BUTTON_IRQ;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 10;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}

void user_led_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Pin = LED_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &GPIO_InitStruct);

    GPIO_ResetBits(LED_PORT, LED_PIN);
}

void user_led_toggle(void)
{
    GPIO_WriteBit(LED_PORT, LED_PIN, 
                    (BitAction)(1 - GPIO_ReadOutputDataBit(LED_PORT, LED_PIN)));
}

uint8_t user_btn_test()
{
	uint16_t counter = TIM2->CNT;

	while(counter + 20000 >= TIM2->CNT)
	{
    	if (button_pressed)
		{
			return 1;
		}
	}
	return 0;
}

void user_btn_handler(void)
{
    user_led_toggle();
    printf("Button pressed\n");
    uint8_t s = USBFSH_CheckRootHubPortStatus(RootHubDev.bStatus); // Check USB device connection or disconnection
    printf("%d\n", s);

    if(s == ROOT_DEV_CONNECTED || s == ROOT_DEV_FAILED)
    {
        printf("USB Port Dev In.\r\n");

        usbd_configured = FALSE;
        RootHubDev.bStatus = ROOT_DEV_CONNECTED;
        RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;
        s = usbh_enumerate_root_device(); // Simply enumerate root device
        
        if(s == ERR_SUCCESS)
        {
            uint8_t buffer[4];
            buffer[0] = USBD_DeviceDescriptor[9];
            buffer[1] = USBD_DeviceDescriptor[8];
            buffer[2] = USBD_DeviceDescriptor[11];
            buffer[3] = USBD_DeviceDescriptor[10];

            AT24C02_write(EEPROM_ADDR_DIV, buffer, 4);  // Write both div and pid

            printf("Write OK!\n\r");
        }
    }
    user_led_toggle();
}

void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(BUTTON_EXTI_LINE) != RESET)
    {
        uint32_t current_time = SysTick->CNT;
        uint32_t delta = (current_time - last_interrupt_time); 
        if (delta >= DEBOUNCE_TICKS)
        {  // Valid press
            last_interrupt_time = current_time;
            button_pressed = 1;
            user_btn_handler();
        } else
        {
            button_pressed = 0;
        }
        EXTI_ClearITPendingBit(BUTTON_EXTI_LINE);
    }
}