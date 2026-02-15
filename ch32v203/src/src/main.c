#include "debug.h"
#include "debug_log.h"
#include "usb_lib.h"
#include "usbd.h"
#include "usbh.h"
#include "queue.h"
#include "user.h"
#include "app.h"
#include "eeprom.h"
#include <stdbool.h>
#include <string.h>

/* USB endpoint types */
#define USB_ENDPOINT_TYPE_CONTROL       0x00
#define USB_ENDPOINT_TYPE_ISOCHRONOUS   0x01
#define USB_ENDPOINT_TYPE_BULK          0x02
#define USB_ENDPOINT_TYPE_INTERRUPT     0x03

/* Device status codes */
#define ROOT_DEV_DISCONNECTED 0
#define ROOT_DEV_CONNECTED    1
#define ROOT_DEV_SUCCESS      2
#define ROOT_DEV_FAILED       3

extern ROOT_HUB_DEVICE RootHubDev;
extern HOST_CTL HostCtl[];
extern uint8_t Host_OutBusy[];
extern uint8_t Host_OutToggle[];
extern uint8_t Com_Buf[];
extern uint16_t Com_Buf_Len;

static void printBanner(void)
{
    LOG_INFO("--------------------------------------------------------");
    LOG_INFO("                      usbsp v0.1.0                      ");
    LOG_INFO("--------------------------------------------------------");
}

/* Initialize USB clock */
static uint8_t initUsbClock(void)
{
    LOG_INFO("usb: Configuring USB clock, sysclock: %ld", SystemCoreClock);
    usbd_hw_set_clk();
    usbh_hw_set_clk();
    return ERR_SUCCESS;
}

/* Initialize USB host driver */
static uint8_t initUsbHost(void)
{
    LOG_INFO("usbh: Initializing USB host driver");
    usbh_init(ENABLE);
    return ERR_SUCCESS;
}

/* Configure USB interrupts */
static uint8_t initUsbInterrupts(void)
{
    LOG_INFO("usbd: Configuring USB interrupts");
    usb_hw_set_isr_config();
    return ERR_SUCCESS;
}

/* Initialize user peripherals */
static uint8_t initPeripherals(void)
{
    LOG_INFO("Enabling the USER button");
    user_btn_init();
    LOG_INFO("Enabling the USER LED");
    user_led_init();
    LOG_INFO("Reading EEPROM");
    AT24C02_init();
    AT24C02_read_usb_info();
    return ERR_SUCCESS;
}

/* Initialize system components */
static uint8_t initSystem(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    Delay_Init();
    USART_Debug_Init(115200);
    tim2_init((SystemCoreClock / 100) - 1);

    printBanner();
    LOG_INFO("SystemClk: %ld", SystemCoreClock);

    uint8_t status;
    if ((status = initUsbClock()) != ERR_SUCCESS) return status;
    if ((status = initUsbHost()) != ERR_SUCCESS) return status;
    if ((status = initUsbInterrupts()) != ERR_SUCCESS) return status;
    if ((status = initPeripherals()) != ERR_SUCCESS) return status;

    memset(&RootHubDev, 0, sizeof(ROOT_HUB_DEVICE));
    memset(&HostCtl[DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL], 0,
           DEF_ONE_USB_SUP_DEV_TOTAL * sizeof(HOST_CTL));
    return ERR_SUCCESS;
}

/* Log device type based on class */
static void logDeviceType(uint8_t deviceType)
{
    switch (deviceType)
    {
        case USB_DEV_CLASS_HID:      LOG_INFO("Root Device Is HID"); break;
        case USB_DEV_CLASS_STORAGE:  LOG_INFO("Root Device Is Storage"); break;
        case USB_DEV_CLASS_PRINTER:  LOG_INFO("Root Device Is Printer"); break;
        case USB_DEV_CLASS_VEN_SPEC: LOG_INFO("Root Device Is Vendor Specific"); break;
        case USB_DEV_CLASS_HUB:      LOG_INFO("Root Device Is HUB"); break;
        default:                     LOG_INFO("Root Device Class %d", deviceType); break;
    }
}

/* Handle USB device connection */
static uint8_t handleDeviceConnection(uint8_t index)
{
    if (index >= DEF_ONE_USB_SUP_DEV_TOTAL)
    {
        LOG_ERROR("Invalid device index: %d", index);
        return ERR_USB_UNKNOWN;
    }

    uint8_t status = usbh_enumerate_root_device();
    if (status != ERR_SUCCESS && status != ERR_USB_DISCON)
    {
        LOG_ERROR("Enumeration failed with error code: %x", status);
        RootHubDev.bStatus = ROOT_DEV_FAILED;
        return status;
    }

    LOG_DEBUG("success, device type: %d", RootHubDev.bType);
    logDeviceType(RootHubDev.bType);

    LOG_INFO("usb: Initializing USB driver");
    app_analyze_config_descriptor(index, RootHubDev.bEp0MaxPks);
    LOG_INFO("usb: Max packet size: %d", RootHubDev.bEp0MaxPks);

    usbh_get_string_descriptors(RootHubDev.bEp0MaxPks);
    usb_relay_driver_init();
    LOG_DEBUG("Number of interfaces: %d", HostCtl[index].InterfaceNum);
    RootHubDev.bStatus = ROOT_DEV_SUCCESS;
    return ERR_SUCCESS;
}

/* Handle USB device disconnection */
static void handleDeviceDisconnection(uint8_t index)
{
    if (index >= DEF_ONE_USB_SUP_DEV_TOTAL)
    {
        LOG_ERROR("Invalid device index: %d", index);
        return;
    }
    LOG_INFO("USB Port Dev Out");
    memset(&RootHubDev, 0, sizeof(ROOT_HUB_DEVICE));
    memset(&HostCtl[index], 0, sizeof(HOST_CTL));
}

/* Handle endpoint TX transfer */
static uint8_t handleEndpointTx(uint8_t index, uint8_t intfNum, uint8_t epNum, uint8_t epOut,
                                uint8_t *tempBuf, uint16_t *len)
    {
    if (index >= DEF_ONE_USB_SUP_DEV_TOTAL || epOut >= MAX_PKT_SZ)
    {
        LOG_ERROR("Invalid index: %d or endpoint: %d", index, epOut);
        return ERR_USB_UNKNOWN;
    }
    if (tempBuf == NULL || len == NULL)
    {
        LOG_ERROR("Null buffer or length pointer");
        return ERR_USB_UNKNOWN;
    }
    if (isr_out_queue[epOut].count > 0 && !Host_OutBusy[epOut])
    {
        if (dequeue_packet_for_main(epOut, tempBuf, len) == 0)
        {
            Host_OutBusy[epOut] = 1;
            USBFSH_SendEndpData(epOut, &Host_OutToggle[epOut], tempBuf, *len);
            Host_OutBusy[epOut] = 0;
            return ERR_SUCCESS;
        }
    }
    return ERR_USB_TRANSFER;
}

/* Handle endpoint RX transfer */
static uint8_t handleEndpointRx(uint8_t index, uint8_t intfNum, uint8_t epNum, uint8_t epAddr)
{
    if (index >= DEF_ONE_USB_SUP_DEV_TOTAL || intfNum >= HostCtl[index].InterfaceNum)
    {
        LOG_ERROR("Invalid index: %d or interface: %d", index, intfNum);
        return ERR_USB_UNKNOWN;
    }
    if (bDeviceState != CONFIGURED || _GetEPTxStatus(epAddr) != EP_TX_NAK)
    {
        return ERR_USB_TRANSFER;
    }
    if (UDBD_ENDP_Busy(epAddr) != 0)
    {
        return ERR_USB_TRANSFER;
    }

    uint8_t status = USBFSH_GetEndpData(epAddr,
        &HostCtl[index].Interface[intfNum].InEndpTog[epNum], Com_Buf, &Com_Buf_Len);
    
    if (status == ERR_SUCCESS)
    {
        if (Com_Buf == NULL || Com_Buf_Len == 0 || Com_Buf_Len > MAX_PKT_SZ)
        {
            LOG_DEBUG("Invalid Com_Buf: ptr=%p, len=%d", Com_Buf, Com_Buf_Len);
            return ERR_USB_UNKNOWN;
        }
        status = USBD_ENDP_DataUp(epAddr, Com_Buf, Com_Buf_Len);
        if (status != ERR_SUCCESS)
        {
            LOG_DEBUG("USBD_ENDP_DataUp failed on ep %d: %d", epAddr, status);
            HostCtl[index].ErrorCount++;
        }
    } else if (status == ERR_USB_DISCON)
    {
        LOG_INFO("Device disconnected from host port");
    } else if (status == (USB_PID_STALL | ERR_USB_TRANSFER))
    {
        LOG_ERROR("Abnormal event on host port endpoint %d", epAddr);
        USBFSH_ClearEndpStall(RootHubDev.bEp0MaxPks, epAddr | 0x80);
        HostCtl[index].Interface[intfNum].InEndpTog[epNum] = 0x00;
        HostCtl[index].ErrorCount++;
        if (HostCtl[index].ErrorCount >= 10)
        {
            LOG_INFO("Too many errors, re-enumerating device on host port");
            handleDeviceConnection(index);
        }
    }
    return status;
}

/* Handle endpoint transfers for all interfaces and endpoints */
static void handleEndpointTransfer(uint8_t index)
{
    if (index >= DEF_ONE_USB_SUP_DEV_TOTAL)
    {
        LOG_ERROR("Invalid device index: %d", index);
        return;
    }

    uint8_t tempBuf[MAX_PKT_SZ];
    uint16_t len;

    for (uint8_t intfNum = 0; intfNum < HostCtl[index].InterfaceNum; intfNum++)
    {
        for (uint8_t epNum = 0; epNum < HostCtl[index].Interface[intfNum].InEndpNum; epNum++)
        {
            uint8_t epAddr = HostCtl[index].Interface[intfNum].InEndpAddr[epNum];
            uint8_t epOut = HostCtl[index].Interface[intfNum].OutEndpAddr[epNum];
            uint32_t lastPoll = HostCtl[index].Interface[intfNum].LastInPollTime[epNum];
            uint32_t interval  = HostCtl[index].Interface[intfNum].InEndpInterval[epNum];

            uint32_t currentTime = bIntPackSOF;
            if ((currentTime - lastPoll) >= interval)
            {
                HostCtl[index].Interface[intfNum].LastInPollTime[epNum] = currentTime;
                /* Handle RX transfer */
                handleEndpointRx(index, intfNum, epNum, epAddr);
                /* Handle TX transfer */
                handleEndpointTx(index, intfNum, epNum, epOut, tempBuf, &len);
            }
            else
            {
                continue;
            }
            /* Set TX status if endpoint is not busy */
            if (UDBD_ENDP_Busy(epAddr) == 0)
            {
                SetEPTxStatus(epAddr, EP_TX_NAK);
            }
        }
    }
}

/* Main program loop */
int main(void)
{
    if (initSystem() != ERR_SUCCESS)
    {
        LOG_ERROR("System initialization failed");
        while (1); /* Halt on failure */
    }

    while (1)
    {
        uint8_t status = USBFSH_CheckRootHubPortStatus(RootHubDev.bStatus);
        uint8_t index = RootHubDev.DeviceIndex;

        if (status == ROOT_DEV_CONNECTED)
        {
            LOG_INFO("USB Port Dev In");
            RootHubDev.bStatus = ROOT_DEV_CONNECTED;
            RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;
            handleDeviceConnection(RootHubDev.DeviceIndex);
        } else if (status == ROOT_DEV_DISCONNECTED)
        {
            handleDeviceDisconnection(index);
        }

        if (RootHubDev.bStatus >= ROOT_DEV_SUCCESS && bDeviceState == CONFIGURED)
        {
            handleEndpointTransfer(RootHubDev.DeviceIndex);
        }
    }
}