#include "usbd.h"
#include "usbh.h"
#include "usb_endp.h"
#include <string.h>
#include "queue.h"
#include "debug_log.h"

/* USB Endpoint Variables */
uint8_t volatile USBD_Endp_Busy[MAX_USB_IN_ENDPOINTS] = {0};
uint16_t USB_Rx_Cnt = 0;
uint8_t Buffer[DEF_USBD_MAX_PACK_SIZE];

static void update_out_ep_flow(uint8_t ep_num)
{
    if (ep_num == 0)
    {
        return;
    }

    if (ep_num >= MAX_EP_NUM)
    {
        SetEPRxStatus(ep_num, EP_RX_NAK);
        return;
    }

    if (isr_queue_has_space(ep_num))
    {
        SetEPRxStatus(ep_num, EP_RX_VALID);
    }
    else
    {
        SetEPRxStatus(ep_num, EP_RX_NAK);
    }
}

/* Endpoint 1 IN Callback */
void EP1_IN_Callback(void) /* Clear busy flag after transfer */
{
	USBD_Endp_Busy[1] = 0;
}

/* Endpoint 2 IN Callback */
void EP2_IN_Callback(void) /* Clear busy flag after transfer */
{
	USBD_Endp_Busy[2] = 0; /* Clear busy flag after transfer */
}

void EP3_IN_Callback(void)
{
    USBD_Endp_Busy[3] = 0; /* Clear busy flag after transfer */
}

void EP4_IN_Callback(void)
{
    USBD_Endp_Busy[4] = 0; /* Clear busy flag after transfer */
}

void EP5_IN_Callback(void)
{
    USBD_Endp_Busy[5] = 0; /* Clear busy flag after transfer */
}

void EP6_IN_Callback(void)
{
    USBD_Endp_Busy[6] = 0; /* Clear busy flag after transfer */
}

void EP7_IN_Callback(void)
{
    USBD_Endp_Busy[7] = 0; /* Clear busy flag after transfer */
}

/* Endpoint 1 OUT Callback  */
void EP1_OUT_Callback(void)
{
    uint16_t count = USB_SIL_Read(EP1_OUT, Buffer);
    //LOG_DEBUG("USB_SIL_Read: ");
    //for(uint32_t i = 0; i < count; i++)
    //{
    //    printf("%02x", Buffer[i]);
    //}
    //printf("\n\r");
    //fflush(stdout);
    (void)isr_enqueue_packet(EP1_OUT, Buffer, count);
    update_out_ep_flow(ENDP1);
}

/* Endpoint 2 OUT Callback */
void EP2_OUT_Callback(void)
{
    uint16_t count = USB_SIL_Read(EP2_OUT, Buffer);
    (void)isr_enqueue_packet(EP2_OUT, Buffer, count);
    update_out_ep_flow(ENDP2);
}

/* Endpoint 3 OUT Callback */
void EP3_OUT_Callback(void)
{
    uint16_t count = USB_SIL_Read(EP3_OUT, Buffer);
    (void)isr_enqueue_packet(EP3_OUT, Buffer, count);
    update_out_ep_flow(ENDP3);
}

/* Endpoint 4 OUT Callback */
void EP4_OUT_Callback(void)
{
    uint16_t count = USB_SIL_Read(EP4_OUT, Buffer);
    (void)isr_enqueue_packet(EP4_OUT, Buffer, count);
    update_out_ep_flow(ENDP4);
}

/* Endpoint 5 OUT Callback */
void EP5_OUT_Callback(void)
{
    uint16_t count = USB_SIL_Read(EP5_OUT, Buffer);
    (void)isr_enqueue_packet(EP5_OUT, Buffer, count);
    update_out_ep_flow(ENDP5);
}

/* Endpoint 6 OUT Callback */
void EP6_OUT_Callback(void)
{
    uint16_t count = USB_SIL_Read(EP6_OUT, Buffer);
    (void)isr_enqueue_packet(EP6_OUT, Buffer, count);
    update_out_ep_flow(ENDP6);
}

/* Endpoint 7 OUT Callback */
void EP7_OUT_Callback(void)
{
    uint16_t count = USB_SIL_Read(EP7_OUT, Buffer);
    (void)isr_enqueue_packet(EP7_OUT, Buffer, count);
    update_out_ep_flow(ENDP7);
}
uint8_t UDBD_ENDP_Busy(uint16_t endpoint)
{
    return USBD_Endp_Busy[endpoint];
}
/* Send Data Over Endpoint */
uint8_t USBD_ENDP_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len)
{
    //printf("USBD_ENDPx_DataUp: endp=%u, len=%u, busy=%u\n\r",
    //       endp, len, USBD_Endp_Busy[endp]);
    if (endp >= 8 || endp == 0)
    {
        return USB_ERROR; // Invalid or control endpoint
    }

    if (USBD_Endp_Busy[endp] == 1)
    {
        return USB_ERROR;
    }

    switch (endp)
    {
        case 1:
            USB_SIL_Write(EP1_IN, pbuf, len);
            SetEPTxStatus(ENDP1, EP_TX_VALID);
            break;
        case 2:
            USB_SIL_Write(EP2_IN, pbuf, len);
            SetEPTxStatus(ENDP2, EP_TX_VALID);
            break;
        case 3:
            USB_SIL_Write(EP3_IN, pbuf, len);
            SetEPTxStatus(ENDP3, EP_TX_VALID);
            break;
        case 4:
            USB_SIL_Write(EP4_IN, pbuf, len);
            SetEPTxStatus(ENDP4, EP_TX_VALID);
            break;
        case 5:
            USB_SIL_Write(EP5_IN, pbuf, len);
            SetEPTxStatus(ENDP5, EP_TX_VALID);
            break;
        case 6:
            USB_SIL_Write(EP6_IN, pbuf, len);
            SetEPTxStatus(ENDP6, EP_TX_VALID);
            break;
        case 7:
            USB_SIL_Write(EP7_IN, pbuf, len);
            SetEPTxStatus(ENDP7, EP_TX_VALID);
            break;
        default:
            return USB_ERROR;
    }

    USBD_Endp_Busy[endp] = 1;

    return USB_SUCCESS;
}
