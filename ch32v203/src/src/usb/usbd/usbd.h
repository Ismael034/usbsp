#ifndef __USBD_H
#define __USBD_H

#include "usb_lib.h"
#include "usb_pwr.h"
#include "usb_hw.h"
#include "usb_desc.h"
#include "usb_prop.h"
#include "usb_istr.h"

/* ISTR events */
/* IMR_MSK */
/* mask defining which events has to be handled */
/* by the device application software */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
                 | CNTR_ESOFM | CNTR_RESETM | ISTR_DOVR)

#define EP_NUM                          (15)

/* Buffer Description Table */
/* buffer table base address */
#define BTABLE_ADDRESS      (0x00)

/* PMA layout (USB FS PMA is 0x000..0x1FF, BTABLE uses 0x000..0x03F). */
/* Reserve 64 bytes for both EP0 RX and EP0 TX so larger mirrored bMaxPacketSize0 */
/* values do not overlap the control buffers and corrupt long descriptors. */
#define ENDP0_RXADDR        (0x40)
#define ENDP0_TXADDR        (0x80)

#define ENDP1_TXADDR        (0x60)
#define ENDP1_RXADDR        (0xA0)
#define ENDP2_TXADDR        (0xE0)
#define ENDP2_RXADDR        (0x120)
#define ENDP3_TXADDR        (0x160)
#define ENDP3_RXADDR        (0x1A0)

/* Optional EP4 mapping left disabled; no PMA headroom guaranteed for 64-byte RX/TX. */
#define ENDP4_TXADDR        (0x1E0)
#define ENDP4_RXADDR        (0x1F0)


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
// #define  EP5_IN_Callback   NOP_Process
// #define  EP6_IN_Callback   NOP_Process
// #define  EP7_IN_Callback   NOP_Process

// #define  EP1_OUT_Callback   NOP_Process
// #define  EP2_OUT_Callback   NOP_Process
// #define  EP3_OUT_Callback   NOP_Process
// #define  EP4_OUT_Callback   NOP_Process
// #define  EP5_OUT_Callback   NOP_Process
// #define  EP6_OUT_Callback   NOP_Process
// #define  EP7_OUT_Callback   NOP_Process

void usbd_driver_init(void);
uint8_t usbd_test(void);

#endif
