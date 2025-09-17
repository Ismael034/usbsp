#include "debug.h"
#include "usb_lib.h"
#include "usbd.h"
#include "usbh.h"
#include "user.h"
#include "eeprom.h"
#include <stdbool.h>

void print_banner(void)
{
    printf("\n\r--------------------------------------------------------\n\r");
    printf("                      usbsp v0.1.0                      \n\r");
    printf("--------------------------------------------------------\n\r");
}

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    Delay_Init();
    USART_Debug_Init(115200);

    print_banner();
    printf("SystemClk:%ld\r\n", SystemCoreClock);
    printf("usbd: Setting USB descriptors:\r\n");

    tim2_init((SystemCoreClock / 100) - 1); 

    printf("usbh: Initializing USB timer\r\n");
    tim3_init(9, SystemCoreClock / 10000 - 1);

    printf("usb: Configuring USB clock, sysclock: %ld\r\n", SystemCoreClock);
    usbd_hw_set_clk();
    usbh_hw_set_clk();

    printf("usbh: Initializing USB host driver\r\n");
    usbh_init(ENABLE);
    
    printf("usbd: Configuring USB interrupts\r\n");
    usb_hw_set_isr_config();

    printf("Enabling the USER button\r\n");
    user_btn_init();

    printf("Enabling the USER LED\r\n");
    user_led_init();

    printf("Reding EEPROM\r\n");
    AT24C02_init();
    AT24C02_read_usb_info();

    memset(&RootHubDev.bStatus, 0, sizeof(ROOT_HUB_DEVICE));
    memset(&HostCtl[DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL].InterfaceNum, 0, DEF_ONE_USB_SUP_DEV_TOTAL * sizeof(HOST_CTL));

    uint8_t  s;
    uint8_t  index;
    uint8_t  intf_num, in_num;
    uint16_t len;

    while (true)
    {
        //DEVICE_INFO *pInfo = &Device_Info;
        //printf_usbd_debug("DCONFIG:%u\r\n", pInfo->Current_Configuration);

        s = USBFSH_CheckRootHubPortStatus(RootHubDev.bStatus); // Check USB device connection or disconnection

        if(s == ROOT_DEV_CONNECTED)
        {   
            printf("USB Port Dev In.\r\n");

            usbd_configured = FALSE;
            RootHubDev.bStatus = ROOT_DEV_CONNECTED;
            RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;
            index = RootHubDev.DeviceIndex;

            s = usbh_enumerate_root_device(); // Simply enumerate root device
            if(s == ERR_SUCCESS)
            {
                if(RootHubDev.bType == USB_DEV_CLASS_HID) // Further enumerate it if this device is a HID device
                {
                    printf("Root Device Is HID. ");

                    s = usbh_enumerate_hid_device(RootHubDev.DeviceIndex, RootHubDev.bEp0MaxPks);
                    if(s == ERR_SUCCESS)
                    {
                        printf("usb relay: Initializing USB driver\r\n");
                        usb_relay_driver_init();
                        if(RootHubDev.bType == USB_DEV_CLASS_HID) // Further enumerate it if this device is a HID device
                        {
                            printf("Root Device Is HID. ");
                            RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                        }
                        else // Detect that this device is a NON-HID device
                        {
                            printf("Root Device Is ");
                            switch(RootHubDev.bType)
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

                        }
                    }
                }
            }
            else if(s != ERR_USB_DISCON)
            {
                printf("Enum Fail with Error Code:%x\r\n",s);
                RootHubDev.bStatus = ROOT_DEV_FAILED;
            }
        } else if(s == ROOT_DEV_DISCONNECT)
        {
            printf("USB Port Dev Out.\r\n");

            index = RootHubDev.DeviceIndex;
            usbd_configured = FALSE;
            memset(&RootHubDev.bStatus, 0, sizeof(ROOT_HUB_DEVICE));
            memset(&HostCtl[index].InterfaceNum, 0, sizeof(HOST_CTL));
        }
        
         /* Get the data of the HID device connected to the USB host port */
        if(RootHubDev.bStatus >= ROOT_DEV_SUCCESS)
        {
            index = RootHubDev.DeviceIndex;

            if(RootHubDev.bType == USB_DEV_CLASS_HID)
            {
                for(intf_num = 0; intf_num < HostCtl[ index ].InterfaceNum; intf_num++)
                {
                    for(in_num = 0; in_num < HostCtl[ index ].Interface[ intf_num ].InEndpNum; in_num++)
                    {
                        /* Get endpoint data based on the interval time of the device */
                        if(HostCtl[ index ].Interface[ intf_num ].InEndpTimeCount[ in_num ] >= HostCtl[ index ].Interface[ intf_num ].InEndpInterval[ in_num ])
                        {
                            HostCtl[ index ].Interface[ intf_num ].InEndpTimeCount[ in_num ] %= HostCtl[ index ].Interface[ intf_num ].InEndpInterval[ in_num ];
          
                        s = USBFSH_GetEndpData(HostCtl[ index ].Interface[ intf_num ].InEndpAddr[ in_num ],
                            &HostCtl[ index ].Interface[ intf_num ].InEndpTog[ in_num ], Com_Buf, &len);

                        if(s == ERR_SUCCESS)
                        {
                            if (usbd_configured == TRUE){
                                usb_relay_usbd_endp_data_up(HostCtl[ index ].Interface[ intf_num ].InEndpAddr[ in_num ], Com_Buf, len);
                            }
                        } else if(s == ERR_USB_DISCON)
                        {
                            break;
                        }
                        else if(s == (USB_PID_STALL | ERR_USB_TRANSFER))
                        {
                            /* USB device abnormal event */
                            printf("Abnormal\r\n");
                            /* Clear endpoint */
                            USBFSH_ClearEndpStall(RootHubDev.bEp0MaxPks, HostCtl[ index ].Interface[ intf_num ].InEndpAddr[ in_num ] | 0x80);
                            HostCtl[ index ].Interface[ intf_num ].InEndpTog[ in_num ] = 0x00;

                            /* Judge the number of error */
                            HostCtl[ index ].ErrorCount++;
                            if(HostCtl[ index ].ErrorCount >= 10)
                            {
                                /* Re-enumerate the device and clear the endpoint again */
                                memset(&RootHubDev.bStatus, 0, sizeof(struct _ROOT_HUB_DEVICE));
                                s = usbh_enumerate_root_device();
                                if(s == ERR_SUCCESS)
                                {
                                    USBFSH_ClearEndpStall(RootHubDev.bEp0MaxPks, HostCtl[ index ].Interface[ intf_num ].InEndpAddr[ in_num ] | 0x80);
                                    HostCtl[ index ].ErrorCount = 0x00;

                                    RootHubDev.bStatus = ROOT_DEV_CONNECTED;
                                    RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;

                                    memset(&HostCtl[ index ].InterfaceNum, 0, sizeof(struct __HOST_CTL));
                                    s = usbh_enumerate_hid_device(index, RootHubDev.bEp0MaxPks);
                                    if(s == ERR_SUCCESS)
                                    {
                                        RootHubDev.bStatus = ROOT_DEV_SUCCESS;
                                    }
                                    else if(s != ERR_USB_DISCON)
                                    {
                                        RootHubDev.bStatus = ROOT_DEV_FAILED;
                                    }
                                }
                                else if(s != ERR_USB_DISCON)
                                {
                                    RootHubDev.bStatus = ROOT_DEV_FAILED;
                                }
                            }
                        }
                        }
                    }
                }
            }
        }
    }
}