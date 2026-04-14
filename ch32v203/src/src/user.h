#ifndef __USER_H
#define __USER_H

#include "ch32v20x.h"
#include "debug_log.h"
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
uint8_t user_build_current_tlv_image(uint8_t *image, uint16_t image_size, uint16_t *used_len);
void user_clear_connected_usb_snapshot(void);
void user_set_connected_usb_device_descriptor(const uint8_t *desc, uint16_t len);
void user_set_connected_usb_config_descriptor(const uint8_t *cfg, uint16_t len);
void user_set_connected_usb_string_descriptor(uint8_t index, const uint8_t *src, uint16_t len);
uint8_t user_reset_requested(void);
void user_clear_reset_request(void);

void user_led_init(void);
void user_led_toggle(void);


#endif
