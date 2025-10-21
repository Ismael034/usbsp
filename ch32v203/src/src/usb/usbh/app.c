#include "usbh.h"
#include "usbd.h"
#include "app.h"
#include "debug_log.h"

/* Global variables */
uint8_t  DevDesc_Buf[18];  // Device Descriptor Buffer
uint8_t  ConfDesc_Buf[18];  // Device Descriptor Buffer
uint8_t  Com_Buf[DEF_COM_BUF_LEN];  // General Buffer
uint8_t  Setup_Buf[DEF_COM_BUF_LEN];  // Setup Buffer
uint16_t Setup_Buf_Len = 0;  // Setup Buffer
uint16_t Com_Buf_Len = 0;

ep_config *ep_conf;
uint8_t ep_conf_size;
struct   _ROOT_HUB_DEVICE RootHubDev;
struct   __HOST_CTL HostCtl[DEF_TOTAL_ROOT_HUB * DEF_ONE_USB_SUP_DEV_TOTAL];
uint8_t descriptor_size;


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

    // Always configure EP0 (control endpoint)
    ep_conf[0].ep_num = 0;
    ep_conf[0].ep_type = EP_CONTROL;
    ep_conf[0].ep_tx_status = EP_TX_NAK;
    ep_conf[0].ep_tx_addr = ENDP0_TXADDR;
    ep_conf[0].ep_tx_count = 0;
    ep_conf[0].ep_rx_status = EP_RX_VALID;
    ep_conf[0].ep_rx_addr = ENDP0_RXADDR;
    ep_conf[0].ep_rx_count = 8;
    endpoint_index = 1;

    while (ptr + 1 < end)
    {
        uint8_t length = ptr[0];
        uint8_t descriptor_type = ptr[1];

        if (length < 2 || ptr + length > end)
            break;

        if (descriptor_type == 0x05 && endpoint_index < 8)
        {
            USB_EndpointDescriptor *endpoint = (USB_EndpointDescriptor *)ptr;
            uint8_t ep_num = endpoint->bEndpointAddress & 0x0F;

            // Skip EP0 (already configured)
            if (ep_num == 0)
            {
                ptr += length;
                continue;
            }

            ep_conf[endpoint_index].ep_num = ep_num;

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

            uint16_t max_packet_size = endpoint->wMaxPacketSize;
            
            if (endpoint->bEndpointAddress & 0x80)
            {
                /* IN endpoint */
                ep_conf[endpoint_index].is_ep_in = 0;
                ep_conf[endpoint_index].ep_tx_status = EP_TX_NAK;
                ep_conf[endpoint_index].ep_tx_addr = (ep_num == 1) ? ENDP1_TXADDR :
                                                  (ep_num == 2) ? ENDP2_TXADDR :
                                                  (ep_num == 3) ? ENDP3_TXADDR :
                                                  (ep_num == 4) ? ENDP4_TXADDR : 0;
                ep_conf[endpoint_index].ep_tx_count = 0;
                ep_conf[endpoint_index].ep_rx_status = EP_RX_VALID;
                ep_conf[endpoint_index].ep_rx_addr = (ep_num == 1) ? ENDP1_RXADDR :
                                                  (ep_num == 2) ? ENDP2_RXADDR :
                                                  (ep_num == 3) ? ENDP3_RXADDR :
                                                  (ep_num == 4) ? ENDP4_RXADDR : 0;
                ep_conf[endpoint_index].ep_rx_count = max_packet_size;
            }
            else
            {
                /* OUT endpoint */
                ep_conf[endpoint_index].is_ep_in = 1;
                ep_conf[endpoint_index].ep_rx_status = EP_RX_VALID;
                ep_conf[endpoint_index].ep_rx_addr = (ep_num == 1) ? ENDP1_RXADDR :
                                                  (ep_num == 2) ? ENDP2_RXADDR :
                                                  (ep_num == 3) ? ENDP3_RXADDR :
                                                  (ep_num == 4) ? ENDP4_RXADDR : 0;
                ep_conf[endpoint_index].ep_rx_count = max_packet_size;
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
        
        /* Get USB device descriptor */
        status = USBFSH_GetDeviceDescr(&RootHubDev.bEp0MaxPks, DevDesc_Buf);
        LOG_DEBUG("USB: RootHubDev.bEp0MaxPks: %02x", RootHubDev.bEp0MaxPks);
        if (status == ERR_SUCCESS)
        {
            memcpy(USBD_DeviceDescriptor, DevDesc_Buf, USBD_SIZE_DEVICE_DESC);
            LOG_DEBUG("USB: Device descriptor: ");
            printf("        ");
            for(int i = 0; i < USBD_SIZE_DEVICE_DESC; i++)
            {
                printf("%02x", DevDesc_Buf[i]);
            }
            printf("\r\n");
        }
        else
        {
            if (retry_count <= 5)
            {
                continue;
            }
            return DEF_DEV_DESCR_GETFAIL;
        }

        /* Get USB BOS descriptor */
        status = USBFSH_GetBOSDescr(RootHubDev.bEp0MaxPks, Com_Buf, DEF_COM_BUF_LEN, &descriptor_length);
        if (status == ERR_SUCCESS)
        {
            USBD_BOSDescriptor = (uint8_t *)malloc(descriptor_length);
            memcpy(USBD_BOSDescriptor, Com_Buf, descriptor_length);
            LOG_DEBUG("USB: BOS descriptor: ");
            printf("        ");
            for(int i = 0; i < descriptor_length; i++)
            {
                printf("%02x", USBD_BOSDescriptor[i]);
            }
            printf("\r\n");
        }

        
        /* Get USB configuration descriptor */
        status = USBFSH_GetConfigDescr(RootHubDev.bEp0MaxPks, Com_Buf, DEF_COM_BUF_LEN, &descriptor_length);
        LOG_DEBUG("USB: Config descriptor status: %02x", status);
        if (status == ERR_SUCCESS)
        {
            USBD_ConfigDescriptor = (uint8_t *)malloc(descriptor_length);
            memcpy(USBD_ConfigDescriptor, Com_Buf, descriptor_length);
            USBD_ConfigDescSize = descriptor_length;

            LOG_DEBUG("USB: Config descriptor: ");
            printf("        ");
            for(int i = 0; i < descriptor_length; i++)
            {
                printf("%02x", Com_Buf[i]);
            }
            printf("\r\n");

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
 * @fn      app_analyze_config_descriptor
 *
 * @brief   Analyzes keyboard and mouse configuration descriptor.
 *
 * @param   host_index: USB host port index
 * @param   ep0_size: Endpoint 0 max packet size
 *
 * @return  Analysis result
 */
uint8_t app_analyze_config_descriptor(uint8_t host_index, uint8_t ep0_size)
{
    uint8_t status = ERR_SUCCESS;
    uint16_t buffer_index = 0;
    uint8_t interface_count = 0;

    uint16_t total_length = Com_Buf[2] | ((uint16_t)Com_Buf[3] << 8);

    while (buffer_index < total_length)
    {
        uint8_t descriptor_length = Com_Buf[buffer_index];
        uint8_t descriptor_type   = Com_Buf[buffer_index + 1];

        switch (descriptor_type)
        {
            case DEF_DECR_CONFIG: // Configuration descriptor
            {
                PUSB_CFG_DESCR cfg = (PUSB_CFG_DESCR)&Com_Buf[buffer_index];
                HostCtl[host_index].InterfaceNum = 
                    (cfg->bNumInterfaces > DEF_INTERFACE_NUM_MAX) ? DEF_INTERFACE_NUM_MAX : cfg->bNumInterfaces;
                break;
            }

            case DEF_DECR_INTERFACE: // Interface descriptor
            {
                if (interface_count >= DEF_INTERFACE_NUM_MAX)
                {
                    buffer_index += descriptor_length;
                    break;
                }

                HostCtl[host_index].Interface[interface_count].Type = DEC_UNKNOW;

                // Reset endpoint counters
                HostCtl[host_index].Interface[interface_count].InEndpNum  = 0;
                HostCtl[host_index].Interface[interface_count].OutEndpNum = 0;

                buffer_index += descriptor_length;
                uint8_t input_ep_idx  = 0;
                uint8_t output_ep_idx = 0;

                // Parse endpoints belonging to this interface
                while (buffer_index < total_length)
                {
                    uint8_t len  = Com_Buf[buffer_index];
                    uint8_t type = Com_Buf[buffer_index + 1];

                    if (type == DEF_DECR_INTERFACE || type == DEF_DECR_CONFIG)
                        break;

                    if (type == DEF_DECR_ENDPOINT)
                    {
                        PUSB_ENDP_DESCR ep = (PUSB_ENDP_DESCR)&Com_Buf[buffer_index];
                        uint8_t ep_addr = ep->bEndpointAddress & 0x0F;

                        if (ep->bEndpointAddress & 0x80)
                        {
                            // IN endpoint
                            HostCtl[host_index].Interface[interface_count].InEndpAddr[input_ep_idx]     = ep_addr;
                            HostCtl[host_index].Interface[interface_count].InEndpType[input_ep_idx]     = ep->bmAttributes;
                            HostCtl[host_index].Interface[interface_count].InEndpSize[input_ep_idx]     = ep->wMaxPacketSizeL | ((uint16_t)ep->wMaxPacketSizeH << 8);
                            HostCtl[host_index].Interface[interface_count].InEndpInterval[input_ep_idx] = ep->bInterval;
                            HostCtl[host_index].Interface[interface_count].InEndpNum++;
                            input_ep_idx++;
                        }
                        else
                        {
                            // OUT endpoint
                            HostCtl[host_index].Interface[interface_count].OutEndpAddr[output_ep_idx] = ep_addr;
                            HostCtl[host_index].Interface[interface_count].OutEndpType[output_ep_idx] = ep->bmAttributes;
                            HostCtl[host_index].Interface[interface_count].OutEndpSize[output_ep_idx] = ep->wMaxPacketSizeL | ((uint16_t)ep->wMaxPacketSizeH << 8);
                            HostCtl[host_index].Interface[interface_count].OutEndpNum++;
                            output_ep_idx++;
                        }
                    }
                    else if (type == DEF_DECR_HID)
                    {
                        // HID descriptor
                        PUSB_HID_DESCR hid_desc = (PUSB_HID_DESCR)&Com_Buf[buffer_index];
                        HostCtl[host_index].Interface[interface_count].HidDescLen = hid_desc->wDescriptorLengthL | ((uint16_t)hid_desc->wDescriptorLengthH << 8);
                    }

                    buffer_index += len;
                }

                interface_count++;
                continue;
            }

            default:
            {
                // Unknown descriptor: skip
                break;
            }
        }

        buffer_index += descriptor_length;
    }

    return status;
}


/*********************************************************************
 * @fn      usbh_get_string_descriptor
 *
 * @brief   Enumerates USB device to obtain descriptors.
 *
 * @param   ep0_size: Endpoint 0 max packet size
 *
 * @return  Enumeration result
 */
uint8_t usbh_get_string_descriptors(uint8_t ep0_size)
{
    uint8_t status;
    USBD_StringDescriptor_s string_descriptor;

    if (Com_Buf[6])
    {
        LOG_DEBUG("USB: Get StringDesc4: ");
        status = USBFSH_GetStrDescr(ep0_size, Com_Buf[6], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(string_descriptor.USBD_StringDescriptor, 0, descriptor_size);
            memcpy(string_descriptor.USBD_StringDescriptor, Com_Buf, descriptor_size);
            string_descriptor.USBD_StringDescriptorSize = descriptor_size;

            USBD_StringDescriptor[3] = string_descriptor;
        }
        else
        {
            LOG_ERROR("USB: StringDesc4 err(%02x)", status);
        }
    }

    LOG_DEBUG("USB: Get StringDesc0 (lang descriptor), id=%u: ", 0);
    status = USBFSH_GetStrDescr(ep0_size, 0, Com_Buf, &descriptor_size);
    if (status == ERR_SUCCESS)
    {
        memset(string_descriptor.USBD_StringDescriptor, 0, Com_Buf[0]);
        memcpy(string_descriptor.USBD_StringDescriptor, Com_Buf, Com_Buf[0]);
        string_descriptor.USBD_StringDescriptorSize = Com_Buf[0];

        USBD_StringDescriptor[0] = string_descriptor;
    }
    else
    {
        LOG_ERROR("USB: StringDesc0 Err(%02x)", status);
    }

    if (DevDesc_Buf[14])
    {
        LOG_DEBUG("USB: Get StringDesc1, id=%u: ", DevDesc_Buf[14]);
        status = USBFSH_GetStrDescr(ep0_size, DevDesc_Buf[14], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(string_descriptor.USBD_StringDescriptor, 0, Com_Buf[0]);
            memcpy(string_descriptor.USBD_StringDescriptor, Com_Buf, Com_Buf[0]);
            string_descriptor.USBD_StringDescriptorSize = Com_Buf[0];

            USBD_StringDescriptor[1] = string_descriptor;
        }
        else
        {
            LOG_ERROR("USB: StringDesc1 Err(%02x)", status);
        }
    }

    if (DevDesc_Buf[15])
    {
        LOG_DEBUG("USB: Get StringDesc2: ");
        status = USBFSH_GetStrDescr(ep0_size, DevDesc_Buf[15], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(string_descriptor.USBD_StringDescriptor, 0, Com_Buf[0]);
            memcpy(string_descriptor.USBD_StringDescriptor, Com_Buf, Com_Buf[0]);
            string_descriptor.USBD_StringDescriptorSize = Com_Buf[0];

            USBD_StringDescriptor[2] = string_descriptor;
        }
        else
        {
            LOG_ERROR("USB: StringDesc2 Err(%02x)", status);
        }
    }

    if (DevDesc_Buf[16])
    {
        LOG_DEBUG("USB: Get StringDesc3: ");
        status = USBFSH_GetStrDescr(ep0_size, DevDesc_Buf[16], Com_Buf, &descriptor_size);
        if (status == ERR_SUCCESS)
        {
            memset(string_descriptor.USBD_StringDescriptor, 0, Com_Buf[0]);
            memcpy(string_descriptor.USBD_StringDescriptor, Com_Buf, Com_Buf[0]);
            string_descriptor.USBD_StringDescriptorSize = Com_Buf[0];

            USBD_StringDescriptor[3] = string_descriptor;
        }
        else
        {
            LOG_ERROR("USB: StringDesc3 Err(%02x)", status);
        }
    }
    return ERR_SUCCESS;
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
            memset(&RootHubDev.bStatus, 0, sizeof(struct _ROOT_HUB_DEVICE));
            memset(&HostCtl[device_index].InterfaceNum, 0, sizeof(struct __HOST_CTL));
        }
    }
    return 0;
}