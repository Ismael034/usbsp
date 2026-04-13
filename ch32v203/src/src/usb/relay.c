#include "relay.h"
#include "usbd.h"
#include "usbh.h"
#include "debug_log.h"
#include <stdbool.h>

uint16_t ep0_size;
static uint8_t ctrl_out_data_pending = 0;
static uint8_t ctrl_out_forward_pending = 0;
static uint16_t ctrl_out_write_offset = 0;
extern __IO uint16_t SaveRState;

#define USBASP_FUNC_SETLONGADDRESS 0x09u

static void usb_relay_nodata_generic(void);

static void relay_reset_ctrl_out_state(void)
{
    ctrl_out_data_pending = 0u;
    ctrl_out_forward_pending = 0u;
    ctrl_out_write_offset = 0u;
}

static uint8_t relay_get_ep0_size(void)
{
    return RootHubDev.bEp0MaxPks ? RootHubDev.bEp0MaxPks : DEFAULT_ENDP0_SIZE;
}

static USB_SETUP_REQ relay_make_setup_req(void)
{
    USB_SETUP_REQ setup;

    setup.bRequestType = pInformation->USBbmRequestType;
    setup.bRequest = pInformation->USBbRequest;
    setup.wValue = (uint16_t)(pInformation->USBwValues.bw.bb0 | (pInformation->USBwValues.bw.bb1 << 8));
    setup.wIndex = (uint16_t)(pInformation->USBwIndexs.bw.bb0 | (pInformation->USBwIndexs.bw.bb1 << 8));
    setup.wLength = pInformation->USBwLengths.w;
    return setup;
}

static bool relay_try_local_descriptor(const USB_SETUP_REQ *setup)
{
    uint8_t desc_type;
    uint8_t desc_index;
    const uint8_t *src = NULL;
    uint16_t src_len;

    if (setup == NULL)
    {
        return false;
    }

    if (setup->bRequestType != (STANDARD_REQUEST | DEVICE_RECIPIENT | 0x80u))
    {
        return false;
    }

    if (setup->bRequest != GET_DESCRIPTOR)
    {
        return false;
    }

    desc_type = (uint8_t)(setup->wValue >> 8);
    desc_index = (uint8_t)(setup->wValue & 0xFFu);

    switch (desc_type)
    {
        case USB_DEVICE_DESCRIPTOR:
            src = USBD_DeviceDescriptor;
            src_len = USBD_SIZE_DEVICE_DESC;
            break;

        case USB_CONFIG_DESCRIPTOR:
            if (USBD_ConfigDescriptor == NULL || USBD_ConfigDescSize == 0u)
            {
                return false;
            }
            src = USBD_ConfigDescriptor;
            src_len = USBD_ConfigDescSize;
            break;

        case USB_STRING_DESCRIPTOR:
            if (desc_index >= 4u)
            {
                return false;
            }
            src = USBD_StringDescriptor[desc_index].USBD_StringDescriptor;
            src_len = USBD_StringDescriptor[desc_index].USBD_StringDescriptorSize;
            break;

        default:
            return false;
    }

    if (src == NULL || src_len == 0u)
    {
        return false;
    }

    if (src_len > setup->wLength)
    {
        src_len = setup->wLength;
    }
    if (src_len > DEF_COM_BUF_LEN)
    {
        src_len = DEF_COM_BUF_LEN;
    }

    memcpy(Setup_Buf, src, src_len);
    Setup_Buf_Len = src_len;
    return true;
}

static uint8_t *relay_return_setup_buffer_descriptor(uint16_t length)
{
    ONE_DESCRIPTOR descriptor = {
        .Descriptor = (uint8_t *)Setup_Buf,
        .Descriptor_Size = Setup_Buf_Len
    };
    return Standard_GetDescriptorData(length, &descriptor);
}

/**
 * Initialize the USB relay.
 */
void usb_relay_init(void)
{
    relay_reset_ctrl_out_state();
    usbd_init();
}

/**
 * Reset the USB relay.
 */
void usb_relay_reset(void)
{
    relay_reset_ctrl_out_state();
    usbd_reset();
}

/**
 * Handle USB status IN stage.
 */
void usb_relay_status_in(void)
{
    if (ctrl_out_data_pending) {
        ctrl_out_data_pending = 0;
        ctrl_out_forward_pending = 1;
        SaveRState = EP_RX_NAK;
    }
}

/**
 * Handle USB status OUT stage.
 */
void usb_relay_status_out(void)
{
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
    (void)request_no;

    LOG_DEBUG("usb: usbr data request %x, recipient %02X, wLen=%u", 
                      pInformation->USBbRequest, 
                      pInformation->USBbmRequestType,
                      pInformation->USBwLengths.w);

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
    (void)request_no;
    ctrl_out_data_pending = 0u;
    ctrl_out_forward_pending = 0u;
    LOG_DEBUG("usb: usbr nodata request no %x, ty %x", pInformation->USBbRequest, Type_Recipient);
    usb_relay_nodata_generic();
    return USB_SUCCESS;
}

/**
 * Handle generic no-data setup requests.
 */
static void usb_relay_nodata_generic(void)
{
    USB_SETUP_REQ setup;
    uint16_t plen = 0;
    uint8_t s;

    setup = relay_make_setup_req();
    memcpy(pUSBFS_SetupRequest, &setup, sizeof(USB_SETUP_REQ));
    s = USBFSH_CtrlTransfer(relay_get_ep0_size(), NULL, &plen);
    if (s != ERR_SUCCESS)
    {
        LOG_DEBUG("usb: usbr nodata transfer failed: req=%02x type=%02x val=%04x idx=%04x len=%u st=%u",
                  setup.bRequest,
                  setup.bRequestType,
                  setup.wValue,
                  setup.wIndex,
                  setup.wLength,
                  s);
    }
    else
    {
        LOG_DEBUG("usb: usbr done control transfer (nodata)");
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
    bool is_out;
    uint16_t remaining = 0;
    uint16_t chunk = 0;
    uint8_t *dst = Setup_Buf;

    ep0_size = relay_get_ep0_size();
    pProperty->MaxPacketSize = ep0_size;

    setup = relay_make_setup_req();
    is_out = !(setup.bRequestType & 0x80u);
    memcpy(pUSBFS_SetupRequest, &setup, sizeof(USB_SETUP_REQ));
    ctrl_out_data_pending = 0u;

    if (!is_out)
    {
        if (length == 0u)
        {
            if (relay_try_local_descriptor(&setup))
            {
                return relay_return_setup_buffer_descriptor(length);
            }

            memset(Setup_Buf, 0, DEF_COM_BUF_LEN);
            Setup_Buf_Len = setup.wLength;
            if (Setup_Buf_Len > DEF_COM_BUF_LEN) {
                Setup_Buf_Len = DEF_COM_BUF_LEN;
            }

            s = USBFSH_CtrlTransfer(ep0_size, Setup_Buf, &Setup_Buf_Len);
            if (s != ERR_SUCCESS)
            {
                LOG_DEBUG("usb: usbr IN transfer failed: req=%02x type=%02x val=%04x idx=%04x len=%u st=%u",
                          setup.bRequest,
                          setup.bRequestType,
                          setup.wValue,
                          setup.wIndex,
                          setup.wLength,
                          s);
                Setup_Buf_Len = 0u;
                pInformation->Ctrl_Info.Usb_wLength = 0u;
                return Setup_Buf;
            }

            if ((setup.bRequest == USBASP_FUNC_SETLONGADDRESS) &&
                (setup.wLength > 0u) &&
                (Setup_Buf_Len == 0u))
            {
                Setup_Buf[0] = 0x00;
                Setup_Buf_Len = 1u;
            }
        }
    }

    if (is_out)
    {
        if (length == 0u) {
            Setup_Buf_Len = setup.wLength;
            if (Setup_Buf_Len > DEF_COM_BUF_LEN) {
                Setup_Buf_Len = DEF_COM_BUF_LEN;
            }
            ctrl_out_write_offset = 0u;
        }

        if (ctrl_out_write_offset > Setup_Buf_Len) {
            ctrl_out_write_offset = Setup_Buf_Len;
        }

        remaining = Setup_Buf_Len - ctrl_out_write_offset;
        chunk = (length < remaining) ? length : remaining;
        dst = Setup_Buf + ctrl_out_write_offset;
        ctrl_out_write_offset = (uint16_t)(ctrl_out_write_offset + chunk);

        ctrl_out_data_pending = (Setup_Buf_Len > 0u) ? 1u : 0u;
        return dst;
    }
    else
    {
        return relay_return_setup_buffer_descriptor(length);
    }
}

uint8_t usb_relay_poll(void)
{
    uint16_t plen;
    uint8_t s;

    if (ctrl_out_forward_pending == 0u)
    {
        return ERR_SUCCESS;
    }

    plen = Setup_Buf_Len;
    s = USBFSH_CtrlTransfer(ep0_size, Setup_Buf, &plen);
    ctrl_out_forward_pending = 0u;

    if (s != ERR_SUCCESS)
    {
        LOG_DEBUG("usb: usbr deferred OUT transfer failed: req=%02x type=%02x val=%04x idx=%04x len=%u st=%u",
                  pInformation->USBbRequest,
                  pInformation->USBbmRequestType,
                  (uint16_t)(pInformation->USBwValues.bw.bb0 | (pInformation->USBwValues.bw.bb1 << 8)),
                  (uint16_t)(pInformation->USBwIndexs.bw.bb0 | (pInformation->USBwIndexs.bw.bb1 << 8)),
                  pInformation->USBwLengths.w,
                  s);
    }

    SetEPRxCount(ENDP0, pProperty->MaxPacketSize);
    SetEPRxStatus(ENDP0, EP_RX_VALID);
    return s;
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
    uint8_t cfg;

    cfg = ((USB_CFG_DESCR *)USBD_ConfigDescriptor)->bConfigurationValue;

    if (USBFSH_SetUsbConfig(ep0_size, cfg) != ERR_SUCCESS)
    {
        LOG_DEBUG("usb: usbr failed to set configuration");
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



