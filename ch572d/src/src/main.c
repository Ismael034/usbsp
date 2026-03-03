#include "CH57x_common.h"
#include "log.h"
#include "usb/usb_cdc.h"
#include "eeprom/eeprom.h"
#include "usb/usb_logs.h"
#include "spi_link.h"

__HIGH_CODE
int main(void)
{
    R16_PIN_ALTERNATE &= ~RB_PIN_DEBUG_EN;
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(CLK_SOURCE_HSE_PLL_100MHz);

    usb_cdc_init();
    log_printf("USB CDC ready\r\n");

    AT24C02_init();
    spi_link_init();
    DelayMs(50);
    spi_link_send_test();

    while (1) {
        usb_cdc_task();
    }
}
