#include "usb_prop.h"
#include "debug_log.h"

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
        _SetENDPOINT(i,_GetENDPOINT(i) & 0x7F7F & EPREG_MASK); //all clear
    }
    _SetISTR((uint16_t)0x00FF); //all clear
    USB_SIL_Init(IMR_MSK); // initialize the USB Software Interface Layer
    
    bDeviceState = UNCONNECTED;
    
    usb_hw_set_port(DISABLE, DISABLE);	
    Delay_Ms(20);
    usb_hw_set_port(ENABLE, ENABLE);

    LOG_INFO("USB: Initialization complete");
}

void usbd_reset(void)
{
    pInformation->Current_Configuration = 0;
    pInformation->Current_Feature = USBD_ConfigDescriptor[7];
    pInformation->Current_Interface = 0;

    SetBTABLE(BTABLE_ADDRESS);

    SetEPType(ENDP0, EP_CONTROL);
    SetEPTxStatus(ENDP0, EP_TX_NAK);
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

    LOG_DEBUG("USB: usbd reset");
}


void usbd_set_endpoint_config(ep_config *ep_config, uint8_t endpoints)
{
    LOG_DEBUG("Configuring %d endpoints", endpoints);

    for (uint8_t i = 0; i < endpoints; i++) {
        uint8_t ep_addr = ep_config[i].ep_num;         // full addr (0x81 etc)
        uint8_t ep_idx  = ep_addr & 0x0F;              // numeric index (0..)
        uint8_t is_in   = ep_config[i].is_ep_in;

        SetEPType(ep_idx, ep_config[i].ep_type);

        if (is_in == 0) {
            //printf("    IN ep_ep_idx=%d ep_addr=%d, type=%d, ep_rx_count=%d\n\r", ep_idx, ep_addr, ep_config[i].ep_type, ep_config[i].ep_rx_count);
            // IN endpoint: set TX registers
            SetEPTxAddr(ep_idx, ep_config[i].ep_tx_addr);
            SetEPTxCount(ep_idx, ep_config[i].ep_tx_count);
            SetEPTxStatus(ep_idx, ep_config[i].ep_tx_status);
            // If user provided RX registers for a bi-directional EP, set them too:
            SetEPRxAddr(ep_idx, ep_config[i].ep_rx_addr);
            SetEPRxCount(ep_idx, ep_config[i].ep_rx_count);
            SetEPRxStatus(ep_idx, ep_config[i].ep_rx_status);
        } else {
            printf("    OUT ep_ep_idx=%d ep_addr=%d, type=%d, ep_rx_count=%d\n\r", ep_idx, ep_addr, ep_config[i].ep_type, ep_config[i].ep_rx_count);
            // OUT endpoint: set RX registers
            SetEPRxAddr(ep_idx, ep_config[i].ep_rx_addr);
            SetEPRxCount(ep_idx, ep_config[i].ep_rx_count);
            SetEPRxStatus(ep_idx, ep_config[i].ep_rx_status);
            // If user provided TX registers for a bi-directional EP, set them too:
            SetEPTxAddr(ep_idx, ep_config[i].ep_tx_addr);
            SetEPTxCount(ep_idx, ep_config[i].ep_tx_count);
            SetEPTxStatus(ep_idx, ep_config[i].ep_tx_status);
        }

        _ClearDTOG_TX(ep_idx);
        _ClearDTOG_RX(ep_idx);
    }
}


void usbd_status_in(void)
{
    LOG_DEBUG("USB: usbd inserted (in)");
}

void usbd_status_out(void)
{
    LOG_DEBUG("USB: usbd removed (out)");

}

RESULT usbd_data_setup(uint8_t request_no)
{
    uint32_t Request_No = pInformation->USBbRequest;
    uint8_t *(*CopyRoutine)(uint16_t) = NULL;

    LOG_DEBUG("USB: usbd data request no %x", Request_No);

    if (Type_Recipient == (STANDARD_REQUEST | DEVICE_RECIPIENT) ||
        Type_Recipient == (STANDARD_REQUEST | INTERFACE_RECIPIENT) ||
        Type_Recipient == (STANDARD_REQUEST | ENDPOINT_RECIPIENT))
    {
        switch (Request_No)
        {
            case GET_DESCRIPTOR:
            {
                uint8_t wValueHi = pInformation->USBwValue1;

                LOG_DEBUG("USB: usbd data request %x", wValueHi);

                if (wValueHi == USB_DEVICE_DESCRIPTOR)
                {
                    CopyRoutine = usbd_get_device_descriptor;
                }
                else if (wValueHi == USB_CONFIG_DESCRIPTOR)
                {
                    CopyRoutine = usbd_get_config_descriptor;
                }
                else if (wValueHi == USB_STRING_DESCRIPTOR)
                {
                    CopyRoutine = usbd_get_string_descriptor;
                }
                break;
            }
            default:
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
    LOG_DEBUG("USB: usbd nodata request no %x", Request_No);         
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
    LOG_DEBUG("USB: usbd device descriptor contents requested");
    return Standard_GetDescriptorData(length, (ONE_DESCRIPTOR*)&Device_Descriptor);
}

uint8_t *usbd_get_config_descriptor(uint16_t Length)
{
    ONE_DESCRIPTOR Config_Descriptor =
    {
        (uint8_t*)USBD_ConfigDescriptor,
        USBD_ConfigDescSize
    };

    LOG_DEBUG("USB: usbd config descriptor contents requested");
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
    LOG_DEBUG("USB: usbd string descriptor contents requested");
    uint8_t wValue0 = pInformation->USBwValue0;

    if (wValue0 >= 4) {
        return NULL;
    }

    return Standard_GetDescriptorData(length, &USBD_StringDescriptor[wValue0]);
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