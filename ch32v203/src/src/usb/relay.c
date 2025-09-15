#include "relay.h"
#include "usbd.h"
#include "usbh.h"
#include "eeprom.h"


uint16_t ep0_size;
uint8_t *paddr;
uint8_t usbd_configured = 1;
static uint16_t current_interface_num = 0;
extern uint8_t USBD_Endp3_Busy;
extern usb_state_t usb_state;

void usb_relay_init(void)
{
    usbd_init();
}

void usb_relay_reset(void)
{
    usbd_reset();
}

void usb_relay_status_in(void)
{
    usbd_status_in();
}

void usb_relay_status_out(void)
{
    usbd_status_out();
}

RESULT usb_relay_data_setup(uint8_t request_no)
{
    uint8_t wValueHi = pInformation->USBwValue1;
    current_interface_num = pInformation->USBwIndex0;
    
    uint8_t *(*CopyRoutine)(uint16_t) = NULL;

    printf_usbd_debug("usb data request no %x, type_recipient %x\n\r", request_no, Type_Recipient);

    if (Type_Recipient == (STANDARD_REQUEST | DEVICE_RECIPIENT) ||
        Type_Recipient == (STANDARD_REQUEST | INTERFACE_RECIPIENT) ||
        Type_Recipient == (STANDARD_REQUEST | ENDPOINT_RECIPIENT))
    {
        switch (request_no)
        {
            case GET_STATUS:
            {    
                pInformation->Ctrl_Info.Usb_wLength = 2;
                printf_usbd_debug("GET_STATUS (device): Returning 0x%02x%02x\n\r", 0,0);
                return USB_SUCCESS;
            }
            case GET_DESCRIPTOR:
            {

                printf_usbd_debug("usb data request %x\n\r", wValueHi);

                if (wValueHi == DEVICE_DESCRIPTOR)
                {
                    CopyRoutine = usb_relay_get_device_descriptor;
                }
                else if (wValueHi == CONFIG_DESCRIPTOR)
                {
                    CopyRoutine = usb_relay_get_config_descriptor;
                }
                else if (wValueHi == STRING_DESCRIPTOR)
                {
                    CopyRoutine = usb_relay_get_string_descriptor;
                }
                else if (wValueHi == HID_DESCRIPTOR) 
                {
                    CopyRoutine = usbd_get_hid_descriptor;
                }
                else if (wValueHi == HID_REPORT_DESCRIPTOR)
                {
                    CopyRoutine = usb_relay_get_hid_report_descriptor;
                }
                break;
            }
            default:
                return USB_UNSUPPORT;
        }
    }
    else if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {

        switch (request_no)
        {
            case HID_REQ_SET_REPORT:
            {
                CopyRoutine = usb_relay_set_report;
            }
            case HID_REQ_GET_REPORT:
            {
                CopyRoutine = usb_relay_get_report;
            }
            case CDC_GET_LINE_CODING:
            {
                //CopyRoutine = &USB_CDC_GetLineCoding;
            }
            case CDC_SET_LINE_CODING:
            {
                //CopyRoutine = &USB_CDC_SetLineCoding;
            }
            break;
            default:
                return USB_UNSUPPORT;
        }
    } else {
    }

    if (CopyRoutine)
    {
        pInformation->Ctrl_Info.CopyData = CopyRoutine;
        pInformation->Ctrl_Info.Usb_wOffset = 0;
        (*CopyRoutine)(0);
        return USB_SUCCESS;
    }
    return USB_UNSUPPORT;
}

RESULT usb_relay_nodata_setup(uint8_t request_no)
{      
    uint32_t Request_No = pInformation->USBbRequest;
    printf_usbd_debug("usb nodata request no %x\n\r", Request_No);

    if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
        if (Request_No == CDC_SET_LINE_CTLSTE)
        {
            return USB_SUCCESS;
        }
        else if (Request_No == CDC_SEND_BREAK)
        {
            return USB_SUCCESS;
        }
        else if (Request_No == HID_SET_IDLE)
        {
            printf_usbd_debug("HID SET_IDLE: report_id=%d, idle_rate=%d\n\r", report_id, idle_rate);
            return USB_SUCCESS;
        }
        else if (Request_No == HID_SET_PROTOCOL)
        {
            printf_usbd_debug("HID SET_PROTOCOL: protocol=%d\n\r", protocol);
            return USB_SUCCESS;
        }
        else
        {
            return USB_UNSUPPORT;
        }    
    }             
    return USB_SUCCESS;
}

uint8_t *usb_relay_set_report(uint16_t length){
    printf_usbd_debug("usb record type %d\r\n", pInformation->USBwValue1);
    kb_set_report(0, ep0_size, pInformation->USBwIndex0);
    return NULL;
}

uint8_t *usb_relay_get_report(uint16_t length){
    printf_usbd_debug("usb GET REPORT type %d\r\n", pInformation->USBwValue1);
    usbd_configured = TRUE;
    return NULL;
}

RESULT usb_relay_get_interface_setting(uint8_t Interface, uint8_t AlternateSetting)
{
    return usbd_get_interface_setting(Interface, AlternateSetting);
}

uint8_t *usb_relay_get_device_descriptor(uint16_t length)
{
    printf_usbd_debug("relay: device descriptor contents requested\n\r");
    ep0_size = RootHubDev.bEp0MaxPks;
    pProperty->MaxPacketSize = ep0_size;

    USB_DeviceDescriptor *desc = (USB_DeviceDescriptor *)USBD_DeviceDescriptor;
    desc->idProduct = pid;
    desc->idVendor = vid;

    ONE_DESCRIPTOR Device_Descriptor = {
        .Descriptor = (uint8_t*)desc,
        .Descriptor_Size = USBD_SIZE_DEVICE_DESC
    };
    return Standard_GetDescriptorData(length, (ONE_DESCRIPTOR*)&Device_Descriptor);
}

uint8_t *usb_relay_get_config_descriptor(uint16_t length)
{
    printf_usbd_debug("relay: configuration descriptor contents requested\n\r");

    ONE_DESCRIPTOR Config_Descriptor = {
        (uint8_t*)USBD_ConfigDescriptor,
        USBD_ConfigDescSize
    };
    return Standard_GetDescriptorData(length, (ONE_DESCRIPTOR*)&Config_Descriptor);

}

uint8_t *usb_relay_get_string_descriptor(uint16_t length)
{
    uint8_t wValue0 = pInformation->USBwValue0;

    //printf_usbd_debug("relay: string descriptor contents requested, wvalue=%u\n\r", wValue0);

    ONE_DESCRIPTOR USB_StringDescriptor[4] = {
        {USBD_StringDescriptor[0], USB_STRING_DESCRIPTOR_MAX_SIZE},
        {USBD_StringDescriptor[1], USB_STRING_DESCRIPTOR_MAX_SIZE},
        {USBD_StringDescriptor[2], USB_STRING_DESCRIPTOR_MAX_SIZE},
        {USBD_StringDescriptor[3], USB_STRING_DESCRIPTOR_MAX_SIZE}
    };
    //printf_usbd_debug("string descriptor contents requested\n\r");
    

    if (wValue0 >= 4) {
        return NULL;
    }
    return Standard_GetDescriptorData(length, &USB_StringDescriptor[wValue0]);
}

uint8_t *usb_relay_get_hid_report_descriptor(uint16_t length)
{
    uint16_t descriptor_size = 0;

    //printf_usbd_debug("hid: report descriptor contents requested\n\r");
    //printf_usbd_debug("Length %d\n\r", length);

    uint8_t num;
    num = current_interface_num;
    descriptor_size = HostCtl[RootHubDev.DeviceIndex].Interface[num].HidDescLen;

    //printf_usbd_debug("Get Interface%d ep0_size: %d, HidDescLen: %d \r\n", num, ep0_size, descriptor_size);

    ONE_DESCRIPTOR Report_Descriptor = {
        (uint8_t*)USBD_HIDReportDescriptor[num],
        USBD_HIDReportDescSize
    };
    return Standard_GetDescriptorData(length, (ONE_DESCRIPTOR*)&Report_Descriptor);

    return NULL;
}

void usb_relay_set_configuration(void)
{
    DEVICE_INFO *pInfo = &Device_Info;
    uint8_t cfg;

    printf("CONFIGURATION=%u\n\r", pInfo->Current_Configuration);
    if (pInfo->Current_Configuration != 0)
    {
        bDeviceState = CONFIGURED;
    }

    printf_usbd_debug("relay: set configuration\n\r");

    cfg = ((USB_CFG_DESCR *)USBD_ConfigDescriptor)->bConfigurationValue;
    if(USBFSH_SetUsbConfig(ep0_size, cfg) != ERR_SUCCESS)
    {
        printf_usbd_debug("relay: failed to set configuration\n\r");
        return;
    }    
}

void usb_relay_set_device_address(void)
{
    bDeviceState = ADDRESSED;
    printf_usbd_debug("relay: set device address\n\r");

    if(USBFSH_SetUsbAddress(ep0_size, 0 + USB_DEVICE_ADDR) == ERR_SUCCESS)
    {
        *paddr = 0 + USB_DEVICE_ADDR;
    } else
    {
        printf_usbd_debug("relay: failed to set device address\n\r");
    }
}

void usb_relay_set_device_feature(void)
{
    
}


void usb_relay_clear_feature(void)
{

}
