#include "usbd.h"
#include "usb_endp.h"
#include <string.h>

/* USB Endpoint Variables */
uint8_t USBD_Endp_Busy[MAX_USB_IN_ENDPOINTS] = {1};
uint16_t USB_Rx_Cnt = 0;
uint8_t HID_Buffer[DEF_USBD_MAX_PACK_SIZE];

/* Endpoint 1 IN Callback */
void EP1_IN_Callback(void)
{
    printf("CALLBACK IN 1\n\r");
	USBD_Endp_Busy[1] = 0;
}

/* Endpoint 2 IN Callback */
void EP2_IN_Callback(void)
{
    printf("CALLBACK IN 2\n\r");
	USBD_Endp_Busy[2] = 0;
}

void EP3_IN_Callback(void)
{
    printf("CALLBACK IN 3\n\r");
    USBD_Endp_Busy[3] = 0; /* Clear busy flag after transfer */
}

void EP4_IN_Callback(void)
{
    printf("CALLBACK IN 4\n\r");
    USBD_Endp_Busy[4] = 0; /* Clear busy flag after transfer */
}

/* Endpoint 1 OUT Callback  */
void EP1_OUT_Callback(void)
{
    printf("CALLBACK OUT 1\n\r");
	USBD_Endp_Busy[1] = 0;
}

/* Endpoint 2 OUT Callback */
void EP2_OUT_Callback(void)
{
    printf("CALLBACK OUT 2\n\r");
	USBD_Endp_Busy[2] = 0;
}

/* Endpoint 3 OUT Callback */
void EP3_OUT_Callback(void)
{
    printf("CALLBACK OUT 3\n\r");
	USBD_Endp_Busy[3] = 0;
}

/* Endpoint 4 OUT Callback */
void EP4_OUT_Callback(void)
{
    printf("CALLBACK OUT 4\n\r");
	USBD_Endp_Busy[4] = 0;
}

/* Send Data Over Endpoint */
uint8_t USBD_ENDP_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len)
{
    //printf("[USB DEBUG] USBD_ENDPx_DataUp: endp=%u, len=%u, busy=%u\n\r",
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
        default:
            return USB_ERROR;
    }

    USBD_Endp_Busy[endp] = 1;

    return USB_SUCCESS;
}