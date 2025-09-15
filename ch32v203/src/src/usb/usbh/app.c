#include "usbh.h"
#include "usbd.h"
#include "app.h"

/* Global variables (unchanged for external library compatibility) */
uint8_t  DevDesc_Buf[18];  // Device Descriptor Buffer
uint8_t  Com_Buf[DEF_COM_BUF_LEN];  // General Buffer
ep_config *ep_conf;
uint8_t ep_conf_size;
struct   _ROOT_HUB_DEVICE RootHubDev;
struct   __HOST_CTL HostCtl[DEF_TOTAL_ROOT_HUB * DEF_ONE_USB_SUP_DEV_TOTAL];
uint8_t descriptor_size;

/*******************************************************************************/
/* Interrupt Function Declaration */
void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      tim3_interrupt_handler
 *
 * @brief   Handles TIM3 global interrupt request for USB HID device timing.
 *
 * @return  none
 */
void TIM3_IRQHandler(void)
{
    uint8_t device_index;
    uint8_t hub_port;
    uint8_t interface_index, input_endpoint_index;

    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        /* Clear interrupt flag */
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);

        /* USB HID Device Input Endpoint Timing */
        if (RootHubDev.bStatus >= ROOT_DEV_SUCCESS)
        {
            device_index = RootHubDev.DeviceIndex;
            if (RootHubDev.bType == USB_DEV_CLASS_HID)
            {
                for (interface_index = 0; interface_index < HostCtl[device_index].InterfaceNum; interface_index++)
                {
                    for (input_endpoint_index = 0; input_endpoint_index < HostCtl[device_index].Interface[interface_index].InEndpNum; input_endpoint_index++)
                    {
                        HostCtl[device_index].Interface[interface_index].InEndpTimeCount[input_endpoint_index]++;
                    }
                }
            }
            else if (RootHubDev.bType == USB_DEV_CLASS_HUB)
            {
                HostCtl[device_index].Interface[0].InEndpTimeCount[0]++;
                for (hub_port = 0; hub_port < RootHubDev.bPortNum; hub_port++)
                {
                    if (RootHubDev.Device[hub_port].bStatus >= ROOT_DEV_SUCCESS)
                    {
                        device_index = RootHubDev.Device[hub_port].DeviceIndex;

                        if (RootHubDev.Device[hub_port].bType == USB_DEV_CLASS_HID)
                        {
                            for (interface_index = 0; interface_index < HostCtl[device_index].InterfaceNum; interface_index++)
                            {
                                for (input_endpoint_index = 0; input_endpoint_index < HostCtl[device_index].Interface[interface_index].InEndpNum; input_endpoint_index++)
                                {
                                    HostCtl[device_index].Interface[interface_index].InEndpTimeCount[input_endpoint_index]++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/*********************************************************************
 * @fn      usbh_analyze_device_type
 *
 * @brief   Analyzes USB device type based on descriptors.
 *
 * @param   device_descriptor: Device descriptor buffer
 * @param   config_descriptor: Configuration descriptor buffer
 * @param   device_type: Pointer to store the device type
 *
 * @return  none
 */
void usbh_analyze_device_type(uint8_t *device_descriptor, uint8_t *config_descriptor, uint8_t *device_type)
{
    uint8_t device_class = ((PUSB_DEV_DESCR)device_descriptor)->bDeviceClass;
    uint8_t interface_class = ((PUSB_CFG_DESCR_LONG)config_descriptor)->itf_descr.bInterfaceClass;

    if (device_class == USB_DEV_CLASS_STORAGE || interface_class == USB_DEV_CLASS_STORAGE)
    {
        *device_type = USB_DEV_CLASS_STORAGE;
    }
    else if (device_class == USB_DEV_CLASS_PRINTER || interface_class == USB_DEV_CLASS_PRINTER)
    {
        *device_type = USB_DEV_CLASS_PRINTER;
    }
    else if (device_class == USB_DEV_CLASS_HID || interface_class == USB_DEV_CLASS_HID)
    {
        *device_type = USB_DEV_CLASS_HID;
    }
    else if (device_class == USB_DEV_CLASS_HUB || interface_class == USB_DEV_CLASS_HUB)
    {
        *device_type = USB_DEV_CLASS_HUB;
    }
    else
    {
        *device_type = DEF_DEV_TYPE_UNKNOWN;
    }
}

/*********************************************************************
 * @fn      usbh_configure_endpoints
 *
 * @brief   Configures USB device endpoints from configuration descriptor.
 *
 * @param   common_buffer: Buffer containing configuration descriptor
 *
 * @return  none
 */
void usbh_configure_endpoints(uint8_t *common_buffer)
{
    PUSB_CFG_DESCR config = (PUSB_CFG_DESCR)common_buffer;
    uint16_t total_length = config->wTotalLength;
    uint8_t *ptr = common_buffer + config->bLength;
    uint8_t *end = common_buffer + total_length;

    ep_conf = malloc(sizeof(ep_config) * 8);
    memset(ep_conf, 0, sizeof(ep_config) * 8);

    uint8_t endpoint_index = 0;

    while (ptr + 1 < end)
    {
        uint8_t length = ptr[0];
        uint8_t descriptor_type = ptr[1];

        if (length < 2 || ptr + length > end)
            break;

        if (descriptor_type == 0x05 && endpoint_index < 8)
        {
            USB_EndpointDescriptor *endpoint = (USB_EndpointDescriptor *)ptr;

            ep_conf[endpoint_index].ep_num = endpoint->bEndpointAddress & 0x0F;

            uint8_t attributes = endpoint->bmAttributes & 0x03;

            switch (attributes)
            {
                case 0x00:
                    ep_conf[endpoint_index].ep_type = EP_CONTROL;
                    break;
                case 0x01:
                    ep_conf[endpoint_index].ep_type = EP_ISOCHRONOUS;
                    break;
                case 0x02:
                    ep_conf[endpoint_index].ep_type = EP_BULK;
                    break;
                case 0x03:
                    ep_conf[endpoint_index].ep_type = EP_INTERRUPT;
                    break;
            }

            if (endpoint->bEndpointAddress & 0x80)
            {
                /* IN endpoint */
                ep_conf[endpoint_index].ep_tx_status = EP_TX_NAK;
                ep_conf[endpoint_index].ep_tx_addr = ENDP1_TXADDR;
                ep_conf[endpoint_index].ep_tx_count = 0;
                ep_conf[endpoint_index].ep_rx_status = EP_RX_VALID;
                ep_conf[endpoint_index].ep_rx_addr = ENDP1_RXADDR;
                ep_conf[endpoint_index].ep_rx_count = 0;
            }
            else
            {
                /* OUT endpoint */
                ep_conf[endpoint_index].ep_rx_status = EP_RX_VALID;
                ep_conf[endpoint_index].ep_rx_addr = ENDP2_RXADDR;
                ep_conf[endpoint_index].ep_rx_count = endpoint->wMaxPacketSize;
                ep_conf[endpoint_index].ep_tx_status = EP_TX_DIS;
            }

            endpoint_index++;
        }

        ptr += length;
    }

    ep_conf_size = endpoint_index;
}

/*********************************************************************
 * @fn      usbh_enumerate_root_device
 *
 * @brief   Enumerates a device connected to the USB host root port.
 *
 * @return  Enumeration result
 */
uint8_t usbh_enumerate_root_device(void)
{
    uint8_t status;
    uint8_t retry_count = 0;
    uint8_t config_value;
    uint16_t descriptor_length;
    uint16_t retry_index;

    do
    {
        /* Delay and wait for the device to stabilize */
        Delay_Ms(100);
        retry_count++;
        Delay_Ms(8 << retry_count);

        /* Reset the USB device and wait for reconnection */
        USBFSH_ResetRootHubPort(0);
        for (retry_index = 0, status = 0; retry_index < DEF_RE_ATTACH_TIMEOUT; retry_index++)
        {
            if (USBFSH_EnableRootHubPort(&RootHubDev.bSpeed) == ERR_SUCCESS)
            {
                retry_index = 0;
                status++;
                if (status > 6)
                {
                    break;
                }
            }
            Delay_Ms(1);
        }
        if (retry_index)
        {
            if (retry_count <= 5)
            {
                continue;
            }
            return ERR_USB_DISCON;
        }

        /* Get USB device descriptor */
        status = USBFSH_GetDeviceDescr(&RootHubDev.bEp0MaxPks, DevDesc_Buf);
        if (status == ERR_SUCCESS)
        {
            memcpy(USBD_DeviceDescriptor, DevDesc_Buf, USBD_SIZE_DEVICE_DESC);
        }
        else
        {
            if (retry_count <= 5)
            {
                continue;
            }
            return DEF_DEV_DESCR_GETFAIL;
        }

        /* Set the USB device address */
        RootHubDev.bAddress = USB_DEVICE_ADDR;
        status = USBFSH_SetUsbAddress(RootHubDev.bEp0MaxPks, RootHubDev.bAddress);
        if (status == ERR_SUCCESS)
        {
            RootHubDev.bAddress = USB_DEVICE_ADDR;
        }
        else
        {
            if (retry_count <= 5)
            {
                continue;
            }
            return DEF_DEV_ADDR_SETFAIL;
        }
        Delay_Ms(5);

        /* Get USB configuration descriptor */
        status = USBFSH_GetConfigDescr(RootHubDev.bEp0MaxPks, Com_Buf, DEF_COM_BUF_LEN, &descriptor_length);
        if (status == ERR_SUCCESS)
        {
            USBD_ConfigDescriptor = (uint8_t *)malloc(descriptor_length);
            memcpy(USBD_ConfigDescriptor, Com_Buf, descriptor_length);
            USBD_ConfigDescSize = descriptor_length;

            config_value = ((PUSB_CFG_DESCR)Com_Buf)->bConfigurationValue;

            /* Analyze USB device type */
            usbh_analyze_device_type(DevDesc_Buf, Com_Buf, &RootHubDev.bType);
        }
        else
        {
            if (retry_count <= 5)
            {
                continue;
            }
            return DEF_CFG_DESCR_GETFAIL;
        }

        /* Set USB device configuration value */
        status = USBFSH_SetUsbConfig(RootHubDev.bEp0MaxPks, config_value);
        if (status != ERR_SUCCESS)
        {
            if (retry_count <= 5)
            {
                continue;
            }
            return ERR_USB_UNSUPPORT;
        }

        usbh_configure_endpoints(Com_Buf);
        return ERR_SUCCESS;
    } while (retry_count <= 5);

    return ERR_USB_UNSUPPORT;
}

/*********************************************************************
 * @fn      km_analyze_config_descriptor
 *
 * @brief   Analyzes keyboard and mouse configuration descriptor.
 *
 * @param   host_index: USB host port index
 * @param   ep0_size: Endpoint 0 max packet size
 *
 * @return  Analysis result
 */
uint8_t km_analyze_config_descriptor(uint8_t host_index, uint8_t ep0_size)
{
    uint8_t status = ERR_SUCCESS;
    uint16_t buffer_index;
    uint8_t interface_count, input_endpoint_count, output_endpoint_count;

    interface_count = 0;
    for (buffer_index = 0; buffer_index < (Com_Buf[2] + ((uint16_t)Com_Buf[3] << 8));)
    {
        if (Com_Buf[buffer_index + 1] == DEF_DECR_CONFIG)
        {
            /* Save the number of interfaces, capped at maximum */
            uint8_t num_interfaces = ((PUSB_CFG_DESCR)(&Com_Buf[buffer_index]))->bNumInterfaces;
            HostCtl[host_index].InterfaceNum = num_interfaces > DEF_INTERFACE_NUM_MAX ? DEF_INTERFACE_NUM_MAX : num_interfaces;
            buffer_index += Com_Buf[buffer_index];
        }
        else if (Com_Buf[buffer_index + 1] == DEF_DECR_INTERFACE)
        {
            if (interface_count == DEF_INTERFACE_NUM_MAX)
            {
                return status;
            }
            if (((PUSB_ITF_DESCR)(&Com_Buf[buffer_index]))->bInterfaceClass == 0x03)
            {
                /* HID devices (keyboard or mouse) */
                PUSB_ITF_DESCR interface = (PUSB_ITF_DESCR)(&Com_Buf[buffer_index]);
                if (interface->bInterfaceSubClass <= 0x01 && interface->bInterfaceProtocol <= 2)
                {
                    if (interface->bInterfaceProtocol == 0x01) /* Keyboard */
                    {
                        HostCtl[host_index].Interface[interface_count].Type = DEC_KEY;
                        HID_SetIdle(ep0_size, interface_count, 0, 0);
                    }
                    else if (interface->bInterfaceProtocol == 0x02) /* Mouse */
                    {
                        HostCtl[host_index].Interface[interface_count].Type = DEC_MOUSE;
                        HID_SetIdle(ep0_size, interface_count, 0, 0);
                    }
                    status = ERR_SUCCESS;
                    buffer_index += Com_Buf[buffer_index];
                    input_endpoint_count = 0;
                    output_endpoint_count = 0;
                    while (1)
                    {
                        if (buffer_index >= Com_Buf[2] || Com_Buf[buffer_index + 1] == DEF_DECR_INTERFACE)
                        {
                            break;
                        }
                        if (Com_Buf[buffer_index + 1] == DEF_DECR_ENDPOINT)
                        {
                            PUSB_ENDP_DESCR endpoint = (PUSB_ENDP_DESCR)(&Com_Buf[buffer_index]);
                            if (endpoint->bEndpointAddress & 0x80)
                            {
                                /* IN endpoint */
                                HostCtl[host_index].Interface[interface_count].InEndpAddr[input_endpoint_count] = endpoint->bEndpointAddress & 0x0F;
                                HostCtl[host_index].Interface[interface_count].InEndpType[input_endpoint_count] = endpoint->bmAttributes;
                                HostCtl[host_index].Interface[interface_count].InEndpSize[input_endpoint_count] = endpoint->wMaxPacketSizeL + ((uint16_t)endpoint->wMaxPacketSizeH << 8);
                                HostCtl[host_index].Interface[interface_count].InEndpInterval[input_endpoint_count] = endpoint->bInterval;
                                HostCtl[host_index].Interface[interface_count].InEndpNum++;
                                input_endpoint_count++;
                            }
                            else
                            {
                                /* OUT endpoint */
                                HostCtl[host_index].Interface[interface_count].OutEndpAddr[output_endpoint_count] = endpoint->bEndpointAddress & 0x0F;
                                HostCtl[host_index].Interface[interface_count].OutEndpType[output_endpoint_count] = endpoint->bmAttributes;
                                HostCtl[host_index].Interface[interface_count].OutEndpSize[output_endpoint_count] = endpoint->wMaxPacketSizeL + ((uint16_t)endpoint->wMaxPacketSizeH << 8);
                                HostCtl[host_index].Interface[interface_count].OutEndpNum++;
                                output_endpoint_count++;
                            }
                            buffer_index += Com_Buf[buffer_index];
                        }
                        else if (Com_Buf[buffer_index + 1] == DEF_DECR_HID)
                        {
                            PUSB_HID_DESCR hid_desc = (PUSB_HID_DESCR)(&Com_Buf[buffer_index]);
                            HostCtl[host_index].Interface[interface_count].HidDescLen = hid_desc->wDescriptorLengthL | ((uint16_t)hid_desc->wDescriptorLengthH << 8);
                            buffer_index += Com_Buf[buffer_index];
                        }
                        else
                        {
                            buffer_index += Com_Buf[buffer_index];
                        }
                    }
                    if (output_endpoint_count == 1 && HostCtl[host_index].Interface[interface_count].Type == DEC_KEY)
                    {
                        HostCtl[host_index].Interface[interface_count].SetReport_Swi = 0xFF;
                    }
                }
                else
                {
                    HostCtl[host_index].Interface[interface_count].Type = DEC_UNKNOW;
                    buffer_index += Com_Buf[buffer_index];
                }
            }
            else
            {
                HostCtl[host_index].Interface[interface_count].Type = DEC_UNKNOW;
                buffer_index += Com_Buf[buffer_index];
                break;
            }
            interface_count++;
        }
        else
        {
            buffer_index += Com_Buf[buffer_index];
        }
    }

    return status;
}

/*********************************************************************
 * @fn      km_analyze_hid_report_descriptor
 *
 * @brief   Analyzes keyboard and mouse report descriptor.
 *
 * @param   host_index: USB host port index
 * @param   interface_index: Interface number
 *
 * @return  none
 */
void km_analyze_hid_report_descriptor(uint8_t host_index, uint8_t interface_index)
{
    uint8_t report_id = 0x00;
    uint8_t led_state = 0x00;
    uint8_t item_size, item_type, item_tag;
    uint8_t report_size;
    uint8_t report_count;
    uint16_t report_bits;

    uint16_t buffer_index = 0;

    /* Usage Page(Generic Desktop), Usage(Keyboard) */
    if ((Com_Buf[buffer_index + 0] == 0x05) && (Com_Buf[buffer_index + 1] == 0x01) &&
        (Com_Buf[buffer_index + 2] == 0x09) && (Com_Buf[buffer_index + 3] == 0x06))
    {
        buffer_index += 4;
        report_size = 0;
        report_count = 0;
        report_bits = 0;

        while (buffer_index < HostCtl[host_index].Interface[interface_index].HidDescLen)
        {
            /* Item Size, Item Type, Item Tag */
            item_size = Com_Buf[buffer_index] & 0x03;
            item_type = Com_Buf[buffer_index] & 0x0C;
            item_tag = Com_Buf[buffer_index] & 0xF0;

            switch (item_type)
            {
                case 0x00: /* MAIN */
                    switch (item_tag)
                    {
                        case 0x90: /* Output */
                            if (led_state)
                            {
                                report_bits += report_count * report_size;
                                if (report_id != 0 && HostCtl[host_index].Interface[interface_index].IDFlag == 0)
                                {
                                    HostCtl[host_index].Interface[interface_index].IDFlag = 1;
                                    HostCtl[host_index].Interface[interface_index].ReportID = report_id;
                                }
                            }
                            buffer_index++;
                            break;
                        default:
                            buffer_index++;
                            break;
                    }
                    break;

                case 0x04: /* Global */
                    switch (item_tag)
                    {
                        case 0x80: /* Report ID */
                            buffer_index++;
                            report_id = Com_Buf[buffer_index];
                            break;
                        case 0x90: /* Report Count */
                            buffer_index++;
                            report_count = Com_Buf[buffer_index];
                            break;
                        case 0x70: /* Report Size */
                            buffer_index++;
                            report_size = Com_Buf[buffer_index];
                            break;
                        case 0x00: /* Usage Page */
                            buffer_index++;
                            led_state = (Com_Buf[buffer_index] == 0x08) ? 1 : 0; /* LED */
                            break;
                        default:
                            buffer_index++;
                            break;
                    }
                    break;

                case 0x08: /* Local */
                    switch (item_tag)
                    {
                        case 0x10: /* Usage Minimum */
                            buffer_index++;
                            if (led_state)
                            {
                                HostCtl[host_index].Interface[interface_index].LED_Usage_Min = Com_Buf[buffer_index];
                            }
                            break;
                        case 0x20: /* Usage Maximum */
                            buffer_index++;
                            if (led_state)
                            {
                                HostCtl[host_index].Interface[interface_index].LED_Usage_Max = Com_Buf[buffer_index];
                            }
                            break;
                        default:
                            buffer_index++;
                            break;
                    }
                    break;

                default:
                    buffer_index++;
                    break;
            }
            buffer_index += item_size;
        }

        if (report_bits == 8)
        {
            if (HostCtl[host_index].Interface[interface_index].SetReport_Swi == 0)
            {
                HostCtl[host_index].Interface[interface_index].SetReport_Swi = 1;
            }
        }
        else
        {
            HostCtl[host_index].Interface[interface_index].SetReport_Swi = 0;
        }
    }
}

/*********************************************************************
 * @fn      km_deal_hid_report_descriptor
 *
 * @brief   Gets and analyzes keyboard and mouse report descriptor.
 *
 * @param   host_index: USB host port index
 * @param   ep0_size: Endpoint 0 max packet size
 *
 * @return  Result of acquisition and analysis
 */
uint8_t km_deal_hid_report_descriptor(uint8_t host_index, uint8_t ep0_size)
{
    uint8_t status;
    uint8_t interface_index, remaining_interfaces;
    uint8_t retry_count = 0;
    uint16_t buffer_index;

    remaining_interfaces = HostCtl[host_index].InterfaceNum;
    while (remaining_interfaces)
    {
        interface_index = HostCtl[host_index].InterfaceNum - remaining_interfaces;
        if (HostCtl[host_index].Interface[interface_index].HidDescLen)
        {
            retry_count = 0;
            do
            {
                retry_count++;
                printf("Get Interface%d, Len: %d RepDesc: ", interface_index, HostCtl[host_index].Interface[interface_index].HidDescLen);
                status = HID_GetHidDesr(ep0_size, interface_index, Com_Buf, &HostCtl[host_index].Interface[interface_index].HidDescLen);

                if (status == ERR_SUCCESS)
                {
                    if (USBD_HIDReportDescriptor[interface_index] != NULL)
                    {
                        free(USBD_HIDReportDescriptor[interface_index]);
                        USBD_HIDReportDescriptor[interface_index] = NULL;
                    }

                    USBD_HIDReportDescriptor[interface_index] = (uint8_t *)malloc(HostCtl[host_index].Interface[interface_index].HidDescLen);
                    if (USBD_HIDReportDescriptor[interface_index] != NULL)
                    {
                        memcpy(USBD_HIDReportDescriptor[interface_index], Com_Buf, HostCtl[host_index].Interface[interface_index].HidDescLen);
                        USBD_HIDReportDescSize = HostCtl[host_index].Interface[interface_index].HidDescLen;
                    }
                    else
                    {
                        printf("Memory allocation failed for HID report descriptor\r\n");
                    }

                    for (buffer_index = 0; buffer_index < HostCtl[host_index].Interface[interface_index].HidDescLen; buffer_index++)
                    {
                        printf("%02x ", Com_Buf[buffer_index]);
                    }
                    printf("\r\n");

                    km_analyze_hid_report_descriptor(host_index, interface_index);
                    remaining_interfaces--;
                }
                else
                {
                    printf("Err(%02x)\r\n", status);
                    if (retry_count <= 5)
                    {
                        continue;
                    }
                    return DEF_REP_DESCR_GETFAIL;
                }
            } while (status != ERR_SUCCESS && retry_count <= 5);
        }
        else
        {
            remaining_interfaces--;
        }
    }

    return status;
}

/*********************************************************************
 * @fn      usbh_enumerate_hid_device
 *
 * @brief   Enumerates HID device.
 *
 * @param   host_index: USB host port index
 * @param   ep0_size: Endpoint 0 max packet size
 *
 * @return  Enumeration result
 */
uint8_t usbh_enumerate_hid_device(uint8_t host_index, uint8_t ep0_size)
{
    uint8_t status;
    uint8_t interface_index;
#if DEF_DEBUG_PRINTF
    uint8_t buffer_index;
#endif

    printf("Enum Hid:\r\n");

    printf("Analyze CfgDesc: ");
    status = km_analyze_config_descriptor(host_index, ep0_size);
    if (status == ERR_SUCCESS)
    {
        printf("OK\r\n");
    }
    else
    {
        printf("Err(%02x)\r\n", status);
        return status;
    }

    if (Com_Buf[6])
    {
        printf("Get StringDesc4: ");
        status = USBFSH_GetStrDescr(ep0_size, Com_Buf[6], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(USBD_StringDescriptor[3], 0, descriptor_size);
            memcpy(USBD_StringDescriptor[3], Com_Buf, descriptor_size);
#if DEF_DEBUG_PRINTF
            for (buffer_index = 0; buffer_index < Com_Buf[0]; buffer_index++)
            {
                printf("%02x ", Com_Buf[buffer_index]);
            }
            printf("\r\n");
#endif
        }
        else
        {
            printf("Err(%02x)\r\n", status);
        }
    }

    status = km_deal_hid_report_descriptor(host_index, ep0_size);

    if (DevDesc_Buf[14])
    {
        printf("Get StringDesc1, id=%u: ", DevDesc_Buf[14]);
        status = USBFSH_GetStrDescr(ep0_size, DevDesc_Buf[14], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(USBD_StringDescriptor[0], 0, descriptor_size);
            memcpy(USBD_StringDescriptor[0], Com_Buf, descriptor_size);
#if DEF_DEBUG_PRINTF
            for (buffer_index = 0; buffer_index < Com_Buf[0]; buffer_index++)
            {
                printf("%02x ", Com_Buf[buffer_index]);
            }
            printf("\r\n");
#endif
        }
        else
        {
            printf("Err(%02x)\r\n", status);
        }
    }

    if (DevDesc_Buf[15])
    {
        printf("Get StringDesc2: ");
        status = USBFSH_GetStrDescr(ep0_size, DevDesc_Buf[15], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(USBD_StringDescriptor[1], 0, descriptor_size);
            memcpy(USBD_StringDescriptor[1], Com_Buf, descriptor_size);
#if DEF_DEBUG_PRINTF
            for (buffer_index = 0; buffer_index < Com_Buf[0]; buffer_index++)
            {
                printf("%02x ", Com_Buf[buffer_index]);
            }
            printf("\r\n");
#endif
        }
        else
        {
            printf("Err(%02x)\r\n", status);
        }
    }

    if (DevDesc_Buf[16])
    {
        printf("Get StringDesc3: ");
        status = USBFSH_GetStrDescr(ep0_size, DevDesc_Buf[16], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(USBD_StringDescriptor[2], 0, descriptor_size);
            memcpy(USBD_StringDescriptor[2], Com_Buf, descriptor_size);
#if DEF_DEBUG_PRINTF
            for (buffer_index = 0; buffer_index < Com_Buf[0]; buffer_index++)
            {
                printf("%02x ", Com_Buf[buffer_index]);
            }
            printf("\r\n");
#endif
        }
        else
        {
            printf("Err(%02x)\r\n", status);
        }
    }

    for (interface_index = 0; interface_index < HostCtl[host_index].InterfaceNum; interface_index++)
    {
        if (HostCtl[host_index].Interface[interface_index].Type == DEC_KEY)
        {
            HostCtl[host_index].Interface[interface_index].SetReport_Value = 0x00;
            kb_set_report(host_index, ep0_size, interface_index);
        }
    }

    return ERR_SUCCESS;
}

/*********************************************************************
 * @fn      usbh_enumerate_hub_device
 *
 * @brief   Enumerates HUB device.
 *
 * @return  Enumeration result
 */
uint8_t usbh_enumerate_hub_device(void)
{
    uint8_t status, retry;
    uint16_t descriptor_length;
    uint16_t port_index;

    printf("Enum Hub:\r\n");

    printf("Analyze CfgDesc: ");
    status = hub_analyze_config_descriptor(RootHubDev.DeviceIndex);
    if (status == ERR_SUCCESS)
    {
        printf("OK\r\n");
    }
    else
    {
        printf("Err(%02x)\r\n", status);
        return status;
    }

    if (Com_Buf[6])
    {
        printf("Get StringDesc4: ");
        status = USBFSH_GetStrDescr(RootHubDev.bEp0MaxPks, Com_Buf[6], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(USBD_StringDescriptor[3], 0, descriptor_size);
            memcpy(USBD_StringDescriptor[3], Com_Buf, descriptor_size);
#if DEF_DEBUG_PRINTF
            for (port_index = 0; port_index < Com_Buf[0]; port_index++)
            {
                printf("%02x ", Com_Buf[port_index]);
            }
            printf("\r\n");
#endif
        }
        else
        {
            printf("Err(%02x)\r\n", status);
        }
    }

    if (DevDesc_Buf[14])
    {
        printf("Get StringDesc1: ");
        status = USBFSH_GetStrDescr(RootHubDev.bEp0MaxPks, DevDesc_Buf[14], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(USBD_StringDescriptor[0], 0, descriptor_size);
            memcpy(USBD_StringDescriptor[0], Com_Buf, descriptor_size);
#if DEF_DEBUG_PRINTF
            for (port_index = 0; port_index < Com_Buf[0]; port_index++)
            {
                printf("%02x ", Com_Buf[port_index]);
            }
            printf("\r\n");
#endif
        }
        else
        {
            printf("Err(%02x)\r\n", status);
        }
    }

    if (DevDesc_Buf[15])
    {
        printf("Get StringDesc2: ");
        status = USBFSH_GetStrDescr(RootHubDev.bEp0MaxPks, DevDesc_Buf[15], Com_Buf, NULL);
        if (status == ERR_SUCCESS)
        {
            memset(USBD_StringDescriptor[1], 0, descriptor_size);
            memcpy(USBD_StringDescriptor[1], Com_Buf, descriptor_size);
#if DEF_DEBUG_PRINTF
            for (port_index = 0; port_index < Com_Buf[0]; port_index++)
            {
                printf("%02x ", Com_Buf[port_index]);
            }
            printf("\r\n");
#endif
        }
        else
        {
            printf("Err(%02x)\r\n", status);
        }
    }

    if (DevDesc_Buf[16])
    {
        printf("Get StringDesc3: ");
        status = USBFSH_GetStrDescr(RootHubDev.bEp0MaxPks, DevDesc_Buf[16], Com_Buf, NULL);
        if (status == ERR_SUCCESS)
        {
            memset(USBD_StringDescriptor[2], 0, descriptor_size);
            memcpy(USBD_StringDescriptor[2], Com_Buf, descriptor_size);
#if DEF_DEBUG_PRINTF
            for (port_index = 0; port_index < Com_Buf[0]; port_index++)
            {
                printf("%02x ", Com_Buf[port_index]);
            }
            printf("\r\n");
#endif
        }
        else
        {
            printf("Err(%02x)\r\n", status);
        }
    }

    printf("Get Hub Desc: ");
    for (retry = 0; retry < 5; retry++)
    {
        status = HUB_GetClassDevDescr(RootHubDev.bEp0MaxPks, Com_Buf, &descriptor_length);
        if (status == ERR_SUCCESS)
        {
#if DEF_DEBUG_PRINTF
            for (port_index = 0; port_index < descriptor_length; port_index++)
            {
                printf("%02x ", Com_Buf[port_index]);
            }
            printf("\r\n");
#endif
            RootHubDev.bPortNum = ((PUSB_HUB_DESCR)Com_Buf)->bNbrPorts;
            if (RootHubDev.bPortNum > DEF_NEXT_HUB_PORT_NUM_MAX)
            {
                RootHubDev.bPortNum = DEF_NEXT_HUB_PORT_NUM_MAX;
            }
            printf("RootHubDev.bPortNum: %02x\r\n", RootHubDev.bPortNum);
            break;
        }
        else
        {
            printf("Err(%02x)\r\n", status);
            if (retry == 4)
            {
                return ERR_USB_UNKNOWN;
            }
        }
    }

    for (retry = 0, port_index = 1; port_index <= RootHubDev.bPortNum; port_index++)
    {
        status = HUB_SetPortFeature(RootHubDev.bEp0MaxPks, port_index, HUB_PORT_POWER);
        if (status == ERR_SUCCESS)
        {
            continue;
        }
        else
        {
            Delay_Ms(5);
            port_index--;
            retry++;
            if (retry >= 5)
            {
                return ERR_USB_UNKNOWN;
            }
        }
    }

    return ERR_SUCCESS;
}

/*********************************************************************
 * @fn      usbh_enumerate_hub_port_device
 *
 * @brief   Enumerates a device connected to a hub port.
 *
 * @param   hub_port: Hub port index
 * @param   device_address: Pointer to store device address
 * @param   device_type: Pointer to store device type
 *
 * @return  Enumeration result
 */
uint8_t usbh_enumerate_hub_port_device(uint8_t hub_port, uint8_t *device_address, uint8_t *device_type)
{
    uint8_t status;
    uint8_t retry_count;
    uint16_t descriptor_length;
    uint8_t config_value;
#if DEF_DEBUG_PRINTF
    uint16_t buffer_index;
#endif

    printf("(S1)Get DevDesc: \r\n");
    retry_count = 0;
    do
    {
        retry_count++;
        status = USBFSH_GetDeviceDescr(&RootHubDev.Device[hub_port].bEp0MaxPks, DevDesc_Buf);
        if (status == ERR_SUCCESS)
        {
#if DEF_DEBUG_PRINTF
            for (buffer_index = 0; buffer_index < 18; buffer_index++)
            {
                printf("%02x ", DevDesc_Buf[buffer_index]);
            }
            printf("\r\n");
#endif
        }
        else
        {
            printf("Err(%02x)\r\n", status);
            if (retry_count >= 10)
            {
                return DEF_DEV_DESCR_GETFAIL;
            }
        }
    } while (status != ERR_SUCCESS && retry_count < 10);

    printf("Set DevAddr: \r\n");
    retry_count = 0;
    do
    {
        retry_count++;
        status = USBFSH_SetUsbAddress(RootHubDev.Device[hub_port].bEp0MaxPks,
                                      RootHubDev.Device[hub_port].DeviceIndex + USB_DEVICE_ADDR);
        if (status == ERR_SUCCESS)
        {
            *device_address = RootHubDev.Device[hub_port].DeviceIndex + USB_DEVICE_ADDR;
        }
        else
        {
            printf("Err(%02x)\r\n", status);
            if (retry_count >= 10)
            {
                return DEF_DEV_ADDR_SETFAIL;
            }
        }
    } while (status != ERR_SUCCESS && retry_count < 10);
    Delay_Ms(5);

    printf("Get DevCfgDesc: \r\n");
    retry_count = 0;
    do
    {
        retry_count++;
        status = USBFSH_GetConfigDescr(RootHubDev.Device[hub_port].bEp0MaxPks, Com_Buf, DEF_COM_BUF_LEN, &descriptor_length);
        if (status == ERR_SUCCESS)
        {
#if DEF_DEBUG_PRINTF
            for (buffer_index = 0; buffer_index < descriptor_length; buffer_index++)
            {
                printf("%02x ", Com_Buf[buffer_index]);
            }
            printf("\r\n");
#endif
            config_value = ((PUSB_CFG_DESCR)Com_Buf)->bConfigurationValue;
            usbh_analyze_device_type(DevDesc_Buf, Com_Buf, device_type);
            printf("DevType: %02x\r\n", *device_type);
        }
        else
        {
            printf("Err(%02x)\r\n", status);
            if (retry_count >= 10)
            {
                return DEF_DEV_DESCR_GETFAIL;
            }
        }
    } while (status != ERR_SUCCESS && retry_count < 10);

    printf("Set CfgValue: \r\n");
    retry_count = 0;
    do
    {
        retry_count++;
        status = USBFSH_SetUsbConfig(RootHubDev.Device[hub_port].bEp0MaxPks, config_value);
        if (status != ERR_SUCCESS)
        {
            printf("Err(%02x)\r\n", status);
            if (retry_count >= 10)
            {
                return DEF_CFG_VALUE_SETFAIL;
            }
        }
    } while (status != ERR_SUCCESS && retry_count < 10);

    return ERR_SUCCESS;
}

/*********************************************************************
 * @fn      kb_analyze_key_value
 *
 * @brief   Handles keyboard lighting based on key values.
 *
 * @param   host_index: USB host port index
 * @param   interface_index: Interface number
 * @param   data_buffer: Data buffer
 * @param   data_length: Data length
 *
 * @return  none
 */
void kb_analyze_key_value(uint8_t host_index, uint8_t interface_index, uint8_t *data_buffer, uint16_t data_length)
{
    uint8_t key_index;
    uint8_t current_value;
    uint8_t bit_position = 0x00;

    current_value = HostCtl[host_index].Interface[interface_index].SetReport_Value;

    for (key_index = HostCtl[host_index].Interface[interface_index].LED_Usage_Min;
         key_index <= HostCtl[host_index].Interface[interface_index].LED_Usage_Max;
         key_index++)
    {
        if (key_index == 0x01)
        {
            if (memchr(data_buffer, DEF_KEY_NUM, data_length))
            {
                HostCtl[host_index].Interface[interface_index].SetReport_Value ^= (1 << bit_position);
            }
        }
        else if (key_index == 0x02)
        {
            if (memchr(data_buffer, DEF_KEY_CAPS, data_length))
            {
                HostCtl[host_index].Interface[interface_index].SetReport_Value ^= (1 << bit_position);
            }
        }
        else if (key_index == 0x03)
        {
            if (memchr(data_buffer, DEF_KEY_SCROLL, data_length))
            {
                HostCtl[host_index].Interface[interface_index].SetReport_Value ^= (1 << bit_position);
            }
        }
        bit_position++;
    }

    HostCtl[host_index].Interface[interface_index].SetReport_Flag =
        (current_value != HostCtl[host_index].Interface[interface_index].SetReport_Value) ? 1 : 0;
}

/*********************************************************************
 * @fn      kb_set_report
 *
 * @brief   Handles keyboard lighting by sending report data.
 *
 * @param   host_index: USB device index
 * @param   ep0_size: Endpoint 0 max packet size
 * @param   interface_index: Interface number
 *
 * @return  Result of handling keyboard lighting
 */
uint8_t kb_set_report(uint8_t host_index, uint8_t ep0_size, uint8_t interface_index)
{
    uint8_t data[2];
    uint16_t data_length;
    uint8_t status = ERR_SUCCESS;

    if (HostCtl[host_index].Interface[interface_index].IDFlag)
    {
        data[0] = HostCtl[host_index].Interface[interface_index].ReportID;
        data[1] = HostCtl[host_index].Interface[interface_index].SetReport_Value;
        data_length = 2;
    }
    else
    {
        data[0] = HostCtl[host_index].Interface[interface_index].SetReport_Value;
        data_length = 1;
    }

    if (HostCtl[host_index].Interface[interface_index].SetReport_Swi == 1)
    {
        status = HID_SetReport(ep0_size, interface_index, data, &data_length);
    }
    else if (HostCtl[host_index].Interface[interface_index].SetReport_Swi == 0xFF)
    {
        status = USBFSH_SendEndpData(HostCtl[host_index].Interface[interface_index].OutEndpAddr[0],
                                     &HostCtl[host_index].Interface[interface_index].OutEndpTog[0],
                                     data, data_length);
    }

    return status;
}

/*********************************************************************
 * @fn      usbh_main_deal
 *
 * @brief   Enumerates USB devices and obtains keyboard/mouse data periodically.
 *
 * @return  none
 */
void usbh_main_deal(void)
{
    uint8_t status;
    uint8_t device_index;
    uint8_t interface_index, input_endpoint_index;
    uint16_t data_length;
#if DEF_DEBUG_PRINTF
    uint16_t buffer_index;
#endif

    status = USBFSH_CheckRootHubPortStatus(RootHubDev.bStatus);
    if (status == ROOT_DEV_CONNECTED)
    {
        printf("USB Port Dev In.\r\n");

        RootHubDev.bStatus = ROOT_DEV_CONNECTED;
        RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;

        status = usbh_enumerate_root_device();
        if (status == ERR_SUCCESS)
        {
            if (RootHubDev.bType == USB_DEV_CLASS_HID)
            {
                printf("Root Device Is HID. ");
                status = usbh_enumerate_hid_device(RootHubDev.DeviceIndex, RootHubDev.bEp0MaxPks);
                printf("Further Enum Result: ");
                if (status == ERR_SUCCESS)
                {
                    printf("OK\r\n");
                    RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                }
                else if (status != ERR_USB_DISCON)
                {
                    printf("Err(%02x)\r\n", status);
                    RootHubDev.bStatus = ROOT_DEV_FAILED;
                }
            }
            else if (RootHubDev.bType == USB_DEV_CLASS_HUB)
            {
                printf("Root Device Is HUB. ");
                status = usbh_enumerate_hub_device();
                printf("Further Enum Result: ");
                if (status == ERR_SUCCESS)
                {
                    printf("OK\r\n");
                    RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                }
                else if (status != ERR_USB_DISCON)
                {
                    printf("Err(%02x)\r\n", status);
                    RootHubDev.bStatus = ROOT_DEV_FAILED;
                }
            }
            else
            {
                printf("Root Device Is ");
                switch (RootHubDev.bType)
                {
                    case USB_DEV_CLASS_STORAGE:
                        printf("Storage. ");
                        break;
                    case USB_DEV_CLASS_PRINTER:
                        printf("Printer. ");
                        break;
                    case DEF_DEV_TYPE_UNKNOWN:
                        printf("Unknown. ");
                        break;
                }
                printf("End Enum.\r\n");
                RootHubDev.bStatus = ROOT_DEV_SUCCESS;
            }
        }
        else if (status != ERR_USB_DISCON)
        {
            printf("Enum Fail with Error Code:%x\r\n", status);
            RootHubDev.bStatus = ROOT_DEV_FAILED;
        }
    }
    else if (status == ROOT_DEV_DISCONNECT)
    {
        printf("USB Port Dev Out.\r\n");
        device_index = RootHubDev.DeviceIndex;
        memset(&RootHubDev.bStatus, 0, sizeof(struct _ROOT_HUB_DEVICE));
        memset(&HostCtl[device_index].InterfaceNum, 0, sizeof(struct __HOST_CTL));
    }

    if (RootHubDev.bStatus >= ROOT_DEV_SUCCESS)
    {
        device_index = RootHubDev.DeviceIndex;

        if (RootHubDev.bType == USB_DEV_CLASS_HID)
        {
            for (interface_index = 0; interface_index < HostCtl[device_index].InterfaceNum; interface_index++)
            {
                for (input_endpoint_index = 0; input_endpoint_index < HostCtl[device_index].Interface[interface_index].InEndpNum; input_endpoint_index++)
                {
                    if (HostCtl[device_index].Interface[interface_index].InEndpTimeCount[input_endpoint_index] >=
                        HostCtl[device_index].Interface[interface_index].InEndpInterval[input_endpoint_index])
                    {
                        HostCtl[device_index].Interface[interface_index].InEndpTimeCount[input_endpoint_index] %=
                            HostCtl[device_index].Interface[interface_index].InEndpInterval[input_endpoint_index];

                        status = USBFSH_GetEndpData(HostCtl[device_index].Interface[interface_index].InEndpAddr[input_endpoint_index],
                                                    &HostCtl[device_index].Interface[interface_index].InEndpTog[input_endpoint_index],
                                                    Com_Buf, &data_length);
                        if (status == ERR_SUCCESS)
                        {
#if DEF_DEBUG_PRINTF
                            for (buffer_index = 0; buffer_index < data_length; buffer_index++)
                            {
                                printf("%02x ", Com_Buf[buffer_index]);
                            }
                            printf("\r\n");
#endif
                            if (HostCtl[device_index].Interface[interface_index].Type == DEC_KEY)
                            {
                                kb_analyze_key_value(device_index, interface_index, Com_Buf, data_length);
                                if (HostCtl[device_index].Interface[interface_index].SetReport_Flag)
                                {
                                    kb_set_report(device_index, RootHubDev.bEp0MaxPks, interface_index);
                                }
                            }
                        }
                        else if (status == ERR_USB_DISCON)
                        {
                            break;
                        }
                        else if (status == (USB_PID_STALL | ERR_USB_TRANSFER))
                        {
                            printf("Abnormal\r\n");
                            USBFSH_ClearEndpStall(RootHubDev.bEp0MaxPks,
                                                  HostCtl[device_index].Interface[interface_index].InEndpAddr[input_endpoint_index] | 0x80);
                            HostCtl[device_index].Interface[interface_index].InEndpTog[input_endpoint_index] = 0x00;

                            HostCtl[device_index].ErrorCount++;
                            if (HostCtl[device_index].ErrorCount >= 10)
                            {
                                memset(&RootHubDev.bStatus, 0, sizeof(struct _ROOT_HUB_DEVICE));
                                status = usbh_enumerate_root_device();
                                if (status == ERR_SUCCESS)
                                {
                                    USBFSH_ClearEndpStall(RootHubDev.bEp0MaxPks,
                                                          HostCtl[device_index].Interface[interface_index].InEndpAddr[input_endpoint_index] | 0x80);
                                    HostCtl[device_index].ErrorCount = 0x00;
                                    RootHubDev.bStatus = ROOT_DEV_CONNECTED;
                                    RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;
                                    memset(&HostCtl[device_index].InterfaceNum, 0, sizeof(struct __HOST_CTL));
                                    status = usbh_enumerate_hid_device(device_index, RootHubDev.bEp0MaxPks);
                                    if (status == ERR_SUCCESS)
                                    {
                                        RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                                    }
                                    else if (status != ERR_USB_DISCON)
                                    {
                                        RootHubDev.bStatus = ROOT_DEV_FAILED;
                                    }
                                }
                                else if (status != ERR_USB_DISCON)
                                {
                                    RootHubDev.bStatus = ROOT_DEV_FAILED;
                                }
                            }
                        }
                    }
                }
                if (status == ERR_USB_DISCON)
                {
                    break;
                }
            }
        }
    }
}

/*********************************************************************
 * @fn      usbh_test
 *
 * @brief   Simplified version of usbh_main_deal to test the USB host port.
 *
 * @return  0 on success, 1 on failure
 */
uint8_t usbh_test(void)
{
    uint8_t status;
    uint8_t device_index;
    memset(&RootHubDev.bStatus, 0, sizeof(struct _ROOT_HUB_DEVICE));
    memset(&HostCtl[DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL].InterfaceNum, 0, DEF_ONE_USB_SUP_DEV_TOTAL * sizeof(struct __HOST_CTL));
    uint16_t counter = TIM2->CNT;

    while (counter + 20000 >= TIM2->CNT)
    {
        status = USBFSH_CheckRootHubPortStatus(RootHubDev.bStatus);
        if (status == ROOT_DEV_CONNECTED)
        {
            usbd_configured = FALSE;
            RootHubDev.bStatus = ROOT_DEV_CONNECTED;
            RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;
            device_index = RootHubDev.DeviceIndex;

            status = usbh_enumerate_root_device();
            if (status == ERR_SUCCESS)
            {
                return 1;
            }
            else if (status != ERR_USB_DISCON)
            {
                RootHubDev.bStatus = ROOT_DEV_FAILED;
                return 0;
            }
        }
        else if (status == ROOT_DEV_DISCONNECT)
        {
            device_index = RootHubDev.DeviceIndex;
            usbd_configured = FALSE;
            memset(&RootHubDev.bStatus, 0, sizeof(struct _ROOT_HUB_DEVICE));
            memset(&HostCtl[device_index].InterfaceNum, 0, sizeof(struct __HOST_CTL));
        }
    }
    return 0;
}