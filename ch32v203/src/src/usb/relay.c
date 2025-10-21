#include "relay.h"
#include "usbd.h"
#include "usbh.h"
#include "eeprom.h"
#include "debug_log.h"

uint16_t ep0_size;
uint8_t *paddr;
static uint16_t current_interface_num = 0;
extern uint8_t USBD_Endp3_Busy;
extern usb_state_t usb_state;

/**
 * Initialize the USB relay.
 */
void usb_relay_init(void)
{
    usbd_init();
}

/**
 * Reset the USB relay.
 */
void usb_relay_reset(void)
{
    usbd_reset();
}

/**
 * Handle USB status IN stage.
 */
void usb_relay_status_in(void)
{
    USBFSH_CtrlTransfer(ep0_size, Setup_Buf, &Setup_Buf_Len);
}

/**
 * Handle USB status OUT stage.
 */
void usb_relay_status_out(void)
{
    LOG_DEBUG("USB: status OUT stage complete");
    if (pInformation->Ctrl_Info.CopyData != NULL)
    {
        pInformation->Ctrl_Info.Usb_wOffset = 0;
    }
    SetEPRxCount(ENDP0, pProperty->MaxPacketSize);
    SetEPRxStatus(ENDP0, EP_RX_VALID);
}

/**
 * Handle USB data setup requests.
 * @param request_no The USB request number.
 * @return USB_SUCCESS on success, USB_UNSUPPORT otherwise.
 */
RESULT usb_relay_data_setup(uint8_t request_no)
{
    current_interface_num = pInformation->USBwIndex0;

    LOG_DEBUG("USB: usbr data request %x, recipient %02X", 
                      pInformation->USBbRequest, 
                      pInformation->USBbmRequestType);

    pInformation->Ctrl_Info.CopyData = usb_relay_data_generic;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    pInformation->Ctrl_Info.Usb_wLength = pInformation->USBwLengths.w;
    (*pInformation->Ctrl_Info.CopyData)(0);
    return USB_SUCCESS;
}

/**
 * Handle USB no-data setup requests.
 * @param request_no The USB request number.
 * @return USB_SUCCESS on success, USB_UNSUPPORT otherwise.
 */
RESULT usb_relay_nodata_setup(uint8_t request_no)
{      
    uint32_t Request_No = pInformation->USBbRequest;
    LOG_DEBUG("USB: usbr nodata request no %x, ty %x", Request_No, Type_Recipient);
    usb_relay_nodata_generic();
    return USB_SUCCESS;
}

/**
 * Handle generic no-data setup requests.
 */
void usb_relay_nodata_generic(void)
{
    USB_SETUP_REQ setup;
    uint16_t plen = 0;

    setup.bRequestType = pInformation->USBbmRequestType;
    setup.bRequest = pInformation->USBbRequest;
    setup.wValue = (uint16_t)(pInformation->USBwValues.bw.bb0 | (pInformation->USBwValues.bw.bb1 << 8));
    setup.wIndex = (uint16_t)(pInformation->USBwIndexs.bw.bb0 | (pInformation->USBwIndexs.bw.bb1 << 8));
    setup.wLength = pInformation->USBwLengths.w;

    memcpy(pUSBFS_SetupRequest, &setup, sizeof(USB_SETUP_REQ));

    if (USBFSH_CtrlTransfer(0, NULL, &plen) != ERR_SUCCESS)
    {
        LOG_DEBUG("USB: usbr control transfer (nodata) failed");
    }
    else
    {
        LOG_DEBUG("USB: usbr done control transfer (nodata)");
    }
}

/**
 * Handle generic USB data setup requests.
 * @param length The length of data to transfer.
 * @return Pointer to descriptor data.
 */
uint8_t *usb_relay_data_generic(uint16_t length)
{
    USB_SETUP_REQ setup;
    uint8_t s;
    bool is_out = !(pInformation->USBbmRequestType & 0x80); // OUT if direction bit is 0

    ep0_size = RootHubDev.bEp0MaxPks;
    pProperty->MaxPacketSize = ep0_size;

    setup.bRequestType = pInformation->USBbmRequestType;
    setup.bRequest = pInformation->USBbRequest;
    setup.wValue = (uint16_t)(pInformation->USBwValues.bw.bb0 | (pInformation->USBwValues.bw.bb1 << 8));
    setup.wIndex = (uint16_t)(pInformation->USBwIndexs.bw.bb0 | (pInformation->USBwIndexs.bw.bb1 << 8));
    setup.wLength = pInformation->USBwLengths.w;

    memcpy(pUSBFS_SetupRequest, &setup, sizeof(USB_SETUP_REQ));

    //LOG_DEBUG("USB: usbr control transfer: type=%02x req=%02x val=%04x idx=%04x len=%04x cstate=%02x %s",
    //                  setup.bRequestType, setup.bRequest, setup.wValue, setup.wIndex, setup.wLength, pInformation->ControlState,
    //                  is_out ? "(OUT)" : "(IN)");


    if (!is_out)
    {
        memset(Setup_Buf, 0, DEF_COM_BUF_LEN);
        Setup_Buf_Len = setup.wLength;

        if (!is_out)
        {
            s = USBFSH_CtrlTransfer(ep0_size, Setup_Buf, &Setup_Buf_Len);
            if (s != ERR_SUCCESS)
            {
                LOG_DEBUG("USB: usbr IN control transfer failed: %d", s);
                return NULL;
            }
        }
    }
    //LOG_DEBUG("USB: usbr after transfer: offset=%d Setup_Buf=", pInformation->Ctrl_Info.Usb_wOffset);
    //for (uint32_t i = 0; i < (Setup_Buf_Len < 16 ? Setup_Buf_Len : 16); i++) 
    //{
    //    printf("%02x ", Setup_Buf[i]);
    //}
    //printf("\n\r");

    // OUT: Return Setup_Buf for stack to copy PC data during DataStageOut
    // IN: Return descriptor for stack to send to PC
    if (is_out)
    {
        LOG_DEBUG("USB: usbr returning Setup_Buf for OUT data copy");
        return Setup_Buf;
    }
    else
    {
        ONE_DESCRIPTOR Descriptor = {
            .Descriptor = (uint8_t*)Setup_Buf,
            .Descriptor_Size = Setup_Buf_Len
        };
        return Standard_GetDescriptorData(length, &Descriptor);
    }
}

/**
 * Get USB interface setting.
 * @param Interface The interface number.
 * @param AlternateSetting The alternate setting.
 * @return Result of the interface setting check.
 */
RESULT usb_relay_get_interface_setting(uint8_t Interface, uint8_t AlternateSetting)
{
    return usbd_get_interface_setting(Interface, AlternateSetting);
}

/**
 * Set USB configuration.
 */
void usb_relay_set_configuration(void)
{
    DEVICE_INFO *pInfo = &Device_Info;
    uint8_t cfg;

    cfg = ((USB_CFG_DESCR *)USBD_ConfigDescriptor)->bConfigurationValue;

    if (USBFSH_SetUsbConfig(ep0_size, cfg) != ERR_SUCCESS)
    {
        LOG_DEBUG("USB: usbr failed to set configuration");
        return;
    }

    if (cfg != 0)
    {
        bDeviceState = CONFIGURED;
    }
}

void usb_relay_set_device_feature(void)
{
}

void usb_relay_clear_feature(void)
{
}