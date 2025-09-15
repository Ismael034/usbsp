#include "debug.h"
#include "usb_lib.h"
#include "usbd.h"
#include "usbh.h"
#include "user.h"
#include "eeprom.h"
#include <stdbool.h>
#include <string.h>

void print_banner(void) {
    printf("CH32V203 Hardware test\n\r");
}

int main(void) {

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    Delay_Init();
    USART_Debug_Init(115200);

    printf("\033[H");
    printf("\033[2J");
    print_banner();
    printf("SystemClk:%d\r\n", SystemCoreClock);
    printf("\r\n");

    tim2_init((SystemCoreClock / 100) - 1); // General purpouse timout timer
    tim3_init(9, SystemCoreClock / 10000 - 1); // USB timer

    usbd_hw_set_clk();
    usbh_hw_set_clk( );
    usb_hw_set_isr_config();

    /* Init testing */

    printf("Press any enter to start the test\r\n");
    scanf("%*c");

    printf("\r\nTesting user LED...");
    fflush(stdout);
    user_led_init();
    user_led_toggle();
    Delay_Ms(3000);
    user_led_toggle();
    printf("\r\n");

    printf("Testing button...");
    fflush(stdout);
    user_btn_init();
    user_btn_init();
    user_btn_test() ? printf("OK") : printf("FAIL");
    printf("\r\n");

    printf("Testing 24C02 EEPROM...");
    fflush(stdout);
    AT24C02_init();
    AT24C02_test() ? printf("OK") : printf("FAIL");
    printf("\r\n");

    printf("Testing USBD...");
    fflush(stdout);
    usbd_init_test();
    usbd_driver_init();
    usbd_init();
    usbd_test() ? printf("OK") : printf("FAIL");

    printf("\r\n");

    printf("Testing USBH...");
    fflush(stdout);
    usbh_init(ENABLE);
    usbh_test() ? printf("OK") : printf("FAIL");
    printf("\r\n");

    printf("\r\n");
    printf("Test finished. Press RESET to restart\r\n");

    while (1);
   
}