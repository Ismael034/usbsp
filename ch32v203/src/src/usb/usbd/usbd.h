#ifndef __USBD_H
#define __USBD_H

#include "usb_lib.h"
#include "usb_pwr.h"
#include "usb_hw.h"
#include "usb_desc.h"
#include "usb_prop.h"
#include "usb_istr.h"

#ifdef USB_DEBUG
    #define printf_usbd_debug(...) printf("usbd debug: " __VA_ARGS__)
#else
    #define printf_usbd_debug(...)
#endif

/* ISTR events */
/* IMR_MSK */
/* mask defining which events has to be handled */
/* by the device application software */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM )

#define EP_NUM                          (15)

/* Buffer Description Table */
/* buffer table base address */
/* buffer table base address */
#define BTABLE_ADDRESS      (0x00)

/* EP0  */
/* rx/tx buffer base address */
#define ENDP0_RXADDR        (0x40)
#define ENDP0_TXADDR        (0x80)

/* EP1  */
/* tx buffer base address */
#define ENDP1_TXADDR     (0xC0)
#define ENDP1_RXADDR     (ENDP1_TXADDR + 0x40)
#define ENDP2_TXADDR     (ENDP1_RXADDR + 0x40)
#define ENDP2_RXADDR     (ENDP2_TXADDR + 0x40)
#define ENDP3_TXADDR     (ENDP2_RXADDR + 0x40)
#define ENDP3_RXADDR     (ENDP3_TXADDR + 0x40)
#define ENDP4_TXADDR     (ENDP3_RXADDR + 0x40)
#define ENDP4_RXADDR     (ENDP4_TXADDR + 0x40)


/* #define CTR_CALLBACK */
/* #define DOVR_CALLBACK */
/* #define ERR_CALLBACK */
/* #define WKUP_CALLBACK */
/* #define SUSP_CALLBACK */
/* #define RESET_CALLBAC K*/
/* #define SOF_CALLBACK */
/* #define ESOF_CALLBACK */


/* CTR service routines */
/* associated to defined endpoints */
// #define  EP1_IN_Callback   NOP_Process
// #define  EP2_IN_Callback   NOP_Process
// #define  EP3_IN_Callback   NOP_Process
// #define  EP4_IN_Callback   NOP_Process
#define  EP5_IN_Callback   NOP_Process
#define  EP6_IN_Callback   NOP_Process
#define  EP7_IN_Callback   NOP_Process

// #define  EP1_OUT_Callback   NOP_Process
// #define  EP2_OUT_Callback   NOP_Process
// #define  EP3_OUT_Callback   NOP_Process
// #define  EP4_OUT_Callback   NOP_Process
#define  EP5_OUT_Callback   NOP_Process
#define  EP6_OUT_Callback   NOP_Process
#define  EP7_OUT_Callback   NOP_Process

void usbd_driver_init(void);

#endif