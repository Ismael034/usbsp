#ifndef __USBH_HW_H
#define __USBH_HW_H

#include "usbh.h"

void usbh_hw_set_clk(void);
void tim3_init(uint16_t arr, uint16_t psc);

#endif

