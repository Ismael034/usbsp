#ifndef __USB_HW_H
#define __USB_HW_H

#include "usb_type.h"
#include "usbd.h"

void usbd_hw_set_clk(void);
void usb_hw_set_config(void);
void usb_hw_set_lpm(void);
void usb_hw_leave_lpm(void);
void usb_hw_set_isr_config(void);
void usb_hw_set_port(FunctionalState NewState, FunctionalState Pin_In_IPU);

#endif