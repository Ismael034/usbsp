#include "usb_prop.h"

usb_state_t usb_state;
extern uint8_t USBD_Endp_Busy[MAX_USB_IN_ENDPOINTS];
extern ep_config *ep_conf;
extern uint8_t ep_conf_size;

void usbd_init(void)
{
    pInformation->Current_Configuration = 0;
    usbd_power_on(&usb_state);

    for (uint8_t i = 0; i < 8; i++)
    {
        _SetENDPOINT(i,_GetENDPOINT(i) & 0x7F7F & EPREG_MASK);//all clear
    }
    _SetISTR((uint16_t)0x00FF);//all clear
    USB_SIL_Init(IMR_MSK); // Initialize the USB Software Interface Layer
    
    bDeviceState = UNCONNECTED;
    
    usb_hw_set_port(DISABLE, DISABLE);	
    Delay_Ms(20);
    usb_hw_set_port(ENABLE, ENABLE);

    printf_usbd_debug("initialization complete\n\r");
}

void usbd_reset(void)
{
    pInformation->Current_Configuration = 0;
    pInformation->Current_Feature = USBD_ConfigDescriptor[7];
    pInformation->Current_Interface = 0;

    SetBTABLE(BTABLE_ADDRESS);

    SetEPType(ENDP0, EP_CONTROL);
    SetEPTxStatus(ENDP0, EP_TX_STALL);
    SetEPRxAddr(ENDP0, ENDP0_RXADDR);
    SetEPTxAddr(ENDP0, ENDP0_TXADDR);
    Clear_Status_Out(ENDP0);
    SetEPRxCount(ENDP0, pProperty->MaxPacketSize);
    SetEPRxValid(ENDP0);
    _ClearDTOG_RX(ENDP0);
    _ClearDTOG_TX(ENDP0);

    if (ep_conf != NULL)
    {
        usbd_set_endpoint_config(ep_conf, ep_conf_size);
    }
    
    SetDeviceAddress(0);
    bDeviceState = ATTACHED;

    printf_usbd_debug("usb reset\n\r");
}


void usbd_set_endpoint_config(ep_config *ep_config, uint8_t endpoints) {
    for (uint8_t i = 0; i < endpoints; i++) {
        uint8_t ep_num  = ep_config[i].ep_num;
        uint16_t ep_type = ep_config[i].ep_type;

        SetEPType(ep_num, ep_type);

        // Configure TX
        if (ep_config[i].ep_tx_addr != 0xFFFF) {
            SetEPTxAddr(ep_num, ep_config[i].ep_tx_addr);
        }
        if (ep_config[i].ep_tx_status != 0xFFFF) {
            SetEPTxStatus(ep_num, ep_config[i].ep_tx_status);
        }
        if (ep_config[i].ep_tx_count != 0xFFFF) {
            SetEPTxCount(ep_num, ep_config[i].ep_tx_count);
        }

        // Configure RX
        if (ep_config[i].ep_rx_addr != 0xFFFF) {
            SetEPRxAddr(ep_num, ep_config[i].ep_rx_addr);
        }
        if (ep_config[i].ep_rx_count != 0xFFFF) {
            SetEPRxCount(ep_num, ep_config[i].ep_rx_count);
        }
        if (ep_config[i].ep_rx_status != 0xFFFF) {
            SetEPRxStatus(ep_num, ep_config[i].ep_rx_status);
        }
        _ClearDTOG_TX(ep_num);
        _ClearDTOG_RX(ep_num);
    }
}

void usbd_status_in(void)
{
    uint32_t Request_No = pInformation->USBbRequest;
    if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
        printf_usbd_debug("usb inserted\n\r");
        if (Request_No == CDC_SET_LINE_CODING)
        {
            printf("usbd: Line coding request");
        }
    }
}

void usbd_status_out(void)
{
    printf_usbd_debug("usb removed\n\r");

}

RESULT usbd_data_setup(uint8_t request_no)
{
    uint32_t Request_No = pInformation->USBbRequest;
    uint8_t *(*CopyRoutine)(uint16_t) = NULL;

    printf_usbd_debug("usb data request no %x\n\r", Request_No);

    if (Type_Recipient == (STANDARD_REQUEST | DEVICE_RECIPIENT) ||
        Type_Recipient == (STANDARD_REQUEST | INTERFACE_RECIPIENT) ||
        Type_Recipient == (STANDARD_REQUEST | ENDPOINT_RECIPIENT))
    {
        switch (Request_No)
        {
            case GET_DESCRIPTOR:
            {
                uint8_t wValueHi = pInformation->USBwValue1;

                printf_usbd_debug("usb data request %x\n\r", wValueHi);

                if (wValueHi == DEVICE_DESCRIPTOR)
                {
                    CopyRoutine = usbd_get_device_descriptor;
                }
                else if (wValueHi == CONFIG_DESCRIPTOR)
                {
                    CopyRoutine = usbd_get_config_descriptor;
                }
                else if (wValueHi == STRING_DESCRIPTOR)
                {
                    CopyRoutine = usbd_get_string_descriptor;
                }
                else if (wValueHi == HID_DESCRIPTOR) 
                {
                    CopyRoutine = usbd_get_hid_descriptor;
                }
                else if (wValueHi == HID_REPORT_DESCRIPTOR)
                {
                    CopyRoutine = usbd_get_hid_report_descriptor;
                }
                break;
            }
            default:
                return USB_UNSUPPORT;
        }
    }
    else if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
        // Existing CDC logic
        if (Request_No == CDC_GET_LINE_CODING)
        {
            //CopyRoutine = &USB_CDC_GetLineCoding;
        }
        else if (Request_No == CDC_SET_LINE_CODING)
        {
            //CopyRoutine = &USB_CDC_SetLineCoding;
        }
        else
        {
            return USB_UNSUPPORT;
        }
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

RESULT usbd_nodata_setup(uint8_t RequestNo)
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

RESULT usbd_get_interface_setting(uint8_t Interface, uint8_t AlternateSetting)
{
  if (AlternateSetting > 0)
  {
    return USB_UNSUPPORT;
  }
  else if (Interface > 1)
  {
    return USB_UNSUPPORT;
  }
	
  return USB_SUCCESS;
}

uint8_t *usbd_get_device_descriptor(uint16_t length)
{
    static const ONE_DESCRIPTOR Device_Descriptor = {
        .Descriptor = (uint8_t*)USBD_DeviceDescriptor,
        .Descriptor_Size = USBD_SIZE_DEVICE_DESC
    };
    printf_usbd_debug("device descriptor contents requested\n\r");
    return Standard_GetDescriptorData(length, (ONE_DESCRIPTOR*)&Device_Descriptor);
}

uint8_t *usbd_get_config_descriptor(uint16_t Length)
{
    ONE_DESCRIPTOR Config_Descriptor =
    {
        (uint8_t*)USBD_ConfigDescriptor,
        USBD_ConfigDescSize
    };

    printf_usbd_debug("config descriptor contents requested\n\r");
    return Standard_GetDescriptorData(Length, &Config_Descriptor);
}

uint8_t *usbd_get_string_descriptor(uint16_t length)
{
    ONE_DESCRIPTOR USBD_StringDescriptor[4] = {
        {(uint8_t*)USBD_StringLangID, USBD_SIZE_STRING_LANGID},
        {(uint8_t*)USBD_StringVendor, USBD_StringVendorSize},
        {(uint8_t*)USBD_StringProduct, USBD_StringProductSize},
        {(uint8_t*)USBD_StringSerial, USBD_StringSerialSize}
    };
    printf_usbd_debug("string descriptor contents requested\n\r");
    uint8_t wValue0 = pInformation->USBwValue0;

    if (wValue0 >= 4) {
        return NULL;
    }

    return Standard_GetDescriptorData(length, &USBD_StringDescriptor[wValue0]);
}

uint8_t *usbd_get_hid_descriptor(uint16_t length)
{
    printf_usbd_debug("hid descriptor contents requested\n\r");
    return Standard_GetDescriptorData(length, NULL);
}


uint8_t *usbd_get_hid_report_descriptor(uint16_t length)
{
    ONE_DESCRIPTOR Report_Descriptor = {
        (uint8_t*)USBD_HIDReportDescriptor,
        USBD_HIDReportDescSize
    };
    printf_usbd_debug("hid report descriptor contents requested\n\r"); 
    return Standard_GetDescriptorData(length, &Report_Descriptor);
}

void usbd_set_configuration(void)
{
    DEVICE_INFO *pInfo = &Device_Info;

    if (pInfo->Current_Configuration != 0)
    {
        bDeviceState = CONFIGURED;
    }
}


void usbd_set_device_address(void)
{
    bDeviceState = ADDRESSED;
}

void usbd_set_device_feature(void)
{
    
}


void usbd_clear_feature(void)
{

}