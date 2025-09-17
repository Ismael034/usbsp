#ifndef __USER_H
#define __USER_H

#include "ch32v20x.h"
#include "usbh.h"
#include "eeprom.h"

// Pin definitions
#define BUTTON_PORT      GPIOB
#define BUTTON_PIN       GPIO_Pin_8
#define BUTTON_EXTI_LINE EXTI_Line8
#define BUTTON_PORT_SRC  GPIO_PortSourceGPIOB
#define BUTTON_PIN_SRC   GPIO_PinSource8
#define BUTTON_IRQ       EXTI9_5_IRQn

#define LED_PORT         GPIOB
#define LED_PIN          GPIO_Pin_9

void EXTI9_5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));


void user_btn_init(void);
uint8_t user_btn_test(void);
void user_btn_handler(void);

void user_led_init(void);
void user_led_toggle(void);


#endif