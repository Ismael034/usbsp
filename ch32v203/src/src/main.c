#include "debug.h"
#include "debug_log.h"
#include "usb_lib.h"
#include "usbd.h"
#include "usbh.h"
#include "relay.h"
#include "queue.h"
#include "user.h"
#include "app.h"
#include "eeprom.h"
#include "spi_link.h"
#include <stdbool.h>
#include <string.h>

static void handle_device_disconnection(void);
static uint8_t handle_device_connection(void);
static uint8_t handle_endpoint_transfer(void);
static uint8_t reenum_requested = 0;

typedef struct
{
    uint8_t valid;
    uint16_t len;
    uint8_t buf[MAX_PKT_SZ];
} relay_in_slot_t;

static relay_in_slot_t relay_in_slots[MAX_EP_NUM];

static void stop_relay_communication(void)
{

    usb_hw_set_port(DISABLE, DISABLE);
    bDeviceState = UNCONNECTED;

    isr_out_pending = 0;
    memset(isr_out_queue, 0, sizeof(isr_out_queue));
    memset(Host_OutBusy, 0, sizeof(Host_OutBusy));
    memset(Host_OutToggle, 0, sizeof(Host_OutToggle));
    memset(Host_InBusy, 0, sizeof(Host_InBusy));
    memset(Host_InToggle, 0, sizeof(Host_InToggle));
    memset(relay_in_slots, 0, sizeof(relay_in_slots));
}

static void print_banner(void)
{
    LOG_INFO("--------------------------------------------------------");
    LOG_INFO("                      usbsp v0.1.0                      ");
    LOG_INFO("--------------------------------------------------------");
}

static void log_reset_cause(uint32_t rstsckr_snapshot)
{
    bool matched = false;

    LOG_INFO("reset flags (rstsckr): 0x%08lX", (unsigned long)rstsckr_snapshot);

    if ((rstsckr_snapshot & RCC_PORRSTF) != 0U)
    {
        LOG_INFO("reset cause: POR/PDR (power reset)");
        matched = true;
    }
    if ((rstsckr_snapshot & RCC_PINRSTF) != 0U)
    {
        LOG_INFO("reset cause: NRST pin");
        matched = true;
    }
    if ((rstsckr_snapshot & RCC_SFTRSTF) != 0U)
    {
        LOG_INFO("reset cause: software reset");
        matched = true;
    }
    if ((rstsckr_snapshot & RCC_IWDGRSTF) != 0U)
    {
        LOG_INFO("reset cause: IWDG watchdog");
        matched = true;
    }
    if ((rstsckr_snapshot & RCC_WWDGRSTF) != 0U)
    {
        LOG_INFO("reset cause: WWDG watchdog");
        matched = true;
    }
    if ((rstsckr_snapshot & RCC_LPWRRSTF) != 0U)
    {
        LOG_INFO("reset cause: low-power reset");
        matched = true;
    }

    if (!matched)
    {
        LOG_INFO("reset cause: unknown (no reset flags set)");
    }
}

/* Initialize USB clock */
static uint8_t init_usb_clock(void)
{
    LOG_INFO("usb: Configuring USB clock, sysclock: %ld", SystemCoreClock);
    usbd_hw_set_clk();
    usbh_hw_set_clk();
    return ERR_SUCCESS;
}

/* Initialize USB host driver */
static uint8_t init_usb_host(void)
{
    LOG_INFO("usbh: Initializing USB host driver");
    usbh_init(ENABLE);
    return ERR_SUCCESS;
}

/* Configure USB interrupts */
static uint8_t init_usb_interrupts(void)
{
    LOG_INFO("usbd: Configuring USB interrupts");
    usb_hw_set_isr_config();
    return ERR_SUCCESS;
}

/* Initialize user peripherals */
static uint8_t init_peripherals(void)
{
    LOG_INFO("sys: enabling the USER button");
    user_btn_init();
    LOG_INFO("sys: enabling the USER LED");
    user_led_init();
    LOG_INFO("sys: reading EEPROM");
    AT24C02_init();
    AT24C02_read_usb_info();
    return ERR_SUCCESS;
}

/* Initialize system components */
static uint8_t init_system(void)
{
    uint32_t reset_flags = RCC->RSTSCKR;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    Delay_Init();
    USART_Debug_Init(115200);
    tim2_init((SystemCoreClock / 100) - 1);

    print_banner();
    LOG_INFO("systemclk: %ld", SystemCoreClock);
    log_reset_cause(reset_flags);
    RCC_ClearFlag();

    uint8_t status;
    if ((status = init_usb_clock()) != ERR_SUCCESS) return status;
    if ((status = init_usb_host()) != ERR_SUCCESS) return status;
    if ((status = init_usb_interrupts()) != ERR_SUCCESS) return status;
    if ((status = init_peripherals()) != ERR_SUCCESS) return status;
    spi_link_init();
    user_clear_connected_usb_snapshot();

    memset(&RootHubDev, 0, sizeof(ROOT_HUB_DEVICE));
    memset(&HostCtl[DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL], 0,
           DEF_ONE_USB_SUP_DEV_TOTAL * sizeof(HOST_CTL));
    return ERR_SUCCESS;
}

/* Log device type based on class */
static void log_device_type(uint8_t device_type)
{
    switch (device_type)
    {
        case USB_DEV_CLASS_HID:      LOG_INFO("usbh: root device is HID"); break;
        case USB_DEV_CLASS_STORAGE:  LOG_INFO("usbh: root device is storage"); break;
        case USB_DEV_CLASS_PRINTER:  LOG_INFO("usbh: root device is printer"); break;
        case USB_DEV_CLASS_VEN_SPEC: LOG_INFO("usbh: root device is vendor specific"); break;
        case USB_DEV_CLASS_HUB:      LOG_INFO("usbh: root device is hub"); break;
        default:                     LOG_INFO("usbh: root device class %d", device_type); break;
    }
}

/* Handle USB device connection */
static uint8_t handle_device_connection(void)
{
    const uint8_t index = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;

    uint8_t status = usbh_enumerate_root_device();
    if (status == ERR_USB_DISCON)
    {
        LOG_WARN("usbh: device disconnected during enumeration");
        handle_device_disconnection();
        return status;
    }
    if (status != ERR_SUCCESS)
    {
        LOG_ERROR("usbh: enumeration failed with error code %x", status);
        RootHubDev.bStatus = ROOT_DEV_FAILED;
        return status;
    }

    LOG_DEBUG("usbh: success, device type %d", RootHubDev.bType);
    log_device_type(RootHubDev.bType);

    LOG_INFO("usb: Initializing USB driver");
    app_analyze_config_descriptor(index, RootHubDev.bEp0MaxPks);
    LOG_INFO("usb: Max packet size: %d", RootHubDev.bEp0MaxPks);

    usbh_get_string_descriptors(RootHubDev.bEp0MaxPks);
    eeprom_apply_usb_overrides();
    stop_relay_communication();

    if (eeprom_usb_info.has_attach_delay_ms && eeprom_usb_info.attach_delay_ms != 0u)
    {
        LOG_INFO("usb: delaying upstream attach by %u ms", eeprom_usb_info.attach_delay_ms);
        Delay_Ms(eeprom_usb_info.attach_delay_ms);
    }

    if ((eeprom_usb_info.has_flags == 0u) || ((eeprom_usb_info.flags & EEPROM_FLAG_BOOT_CONNECTED) != 0u))
    {
        usb_relay_driver_init();
        LOG_INFO("usb: Relay side initialized");
    }
    else
    {
        LOG_INFO("usb: Relay kept disconnected by EEPROM bootConnected=0");
    }

    LOG_DEBUG("number of interfaces: %d", HostCtl[index].InterfaceNum);
    RootHubDev.bStatus = ROOT_DEV_SUCCESS;
    reenum_requested = 0u;
    return ERR_SUCCESS;
}

/* Handle USB device disconnection */
static void handle_device_disconnection(void)
{
    const uint8_t device_index = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;

    stop_relay_communication();
    memset(&HostCtl[device_index], 0, sizeof(HOST_CTL));
    memset(&RootHubDev, 0, sizeof(ROOT_HUB_DEVICE));
    user_clear_connected_usb_snapshot();
    reenum_requested = 0u;

    LOG_INFO("usb: port dev out");
    LOG_INFO("usb: relay stopped; waiting for a new device connection");
}

/* Handle endpoint TX transfer */
static uint8_t handle_endpoint_tx(uint8_t index, uint8_t ep_out, uint8_t ep_type, uint8_t *temp_buf, uint16_t *len)
{
    uint8_t tx_status = ERR_USB_TRANSFER;
    uint8_t sent_any = 0;
    uint8_t has_space = 0u;

    if (index >= DEF_ONE_USB_SUP_DEV_TOTAL)
    {
        LOG_ERROR("usb: invalid index %d or endpoint %d", index, ep_out);
        return ERR_USB_UNKNOWN;
    }
    if (ep_out == 0)
    {
        return ERR_SUCCESS;
    }
    if (ep_out >= MAX_EP_NUM)
    {
        return ERR_SUCCESS;
    }
    if (temp_buf == NULL || len == NULL)
    {
        LOG_ERROR("usb: null buffer or length pointer");
        return ERR_USB_UNKNOWN;
    }

    if (Host_OutBusy[ep_out])
    {
        return ERR_USB_TRANSFER;
    }

    while (peek_packet_for_main(ep_out, temp_buf, len) == 0)
    {
        LOG_DEBUG("usb: ep%u TX dequeue len=%u depth_before=%u pending=%u", ep_out,
                  (unsigned)*len, (unsigned)isr_out_queue[ep_out].count,
                  (unsigned)isr_out_pending);

        Host_OutBusy[ep_out] = 1;
        tx_status = USBFSH_SendEndpData(ep_out, &Host_OutToggle[ep_out], temp_buf, *len);
        Host_OutBusy[ep_out] = 0;

        LOG_DEBUG("usb: ep%u TX status=0x%02X", ep_out, tx_status);

        if (tx_status == ERR_SUCCESS)
        {
            spi_link_capture_packet(0u, ep_out, ep_type, temp_buf, *len);
            has_space = 0u;
            (void)pop_packet_for_main_and_check_space(ep_out, &has_space);
            LOG_DEBUG("usb: ep%u TX pop depth_after=%u pending=%u", ep_out,
                      (unsigned)isr_out_queue[ep_out].count, (unsigned)isr_out_pending);

            if (has_space != 0u)
            {
                SetEPRxStatus(ep_out, EP_RX_VALID);
                LOG_DEBUG("usb: ep%u RX->VALID (space available depth=%u)", ep_out,
                          (unsigned)isr_out_queue[ep_out].count);
            }
            else
            {
                SetEPRxStatus(ep_out, EP_RX_NAK);
                LOG_DEBUG("usb: ep%u RX->NAK (queue full depth=%u)", ep_out,
                          (unsigned)isr_out_queue[ep_out].count);
            }
            sent_any = 1;
            continue;
        }

        if (tx_status == ERR_USB_DISCON)
        {
            LOG_INFO("usbh: device disconnected from host port");
                handle_device_disconnection();
                return tx_status;
        }

        if (tx_status == (USB_PID_STALL | ERR_USB_TRANSFER))
        {
            LOG_WARN("usbh: host OUT endpoint %u stalled, clearing halt", ep_out);
            (void)USBFSH_ClearEndpStall(RootHubDev.bEp0MaxPks, ep_out);
            Host_OutToggle[ep_out] = 0x00;
        }
        else if (tx_status != (USB_PID_NAK | ERR_USB_TRANSFER))
        {
            LOG_DEBUG("usbh: host OUT endpoint %u tx error %u", ep_out, tx_status);
        }
        break;
    }

    if (sent_any) return ERR_SUCCESS;
    return tx_status;
}

/* Handle endpoint RX transfer */
static uint8_t handle_endpoint_rx(uint8_t index, uint8_t intf_num, uint8_t ep_num, uint8_t ep_addr, uint8_t ep_type)
{
    relay_in_slot_t *slot;
    uint8_t status;

    if (index >= DEF_ONE_USB_SUP_DEV_TOTAL || intf_num >= HostCtl[index].InterfaceNum)
    {
        LOG_ERROR("usb: invalid index %d or interface %d", index, intf_num);
        return ERR_USB_UNKNOWN;
    }
    slot = &relay_in_slots[ep_addr];

    if (slot->valid != 0u)
    {
        if (bDeviceState == CONFIGURED && _GetEPTxStatus(ep_addr) == EP_TX_NAK &&
            UDBD_ENDP_Busy(ep_addr) == 0)
        {
            status = USBD_ENDP_DataUp(ep_addr, slot->buf, slot->len);
            if (status == ERR_SUCCESS)
            {
                slot->valid = 0u;
            }
            return status;
        }

        return ERR_USB_TRANSFER;
    }

    status = USBFSH_GetEndpData(ep_addr,
        &HostCtl[index].Interface[intf_num].InEndpTog[ep_num], Com_Buf, &Com_Buf_Len);
    
    if (status == ERR_SUCCESS)
    {
        if (Com_Buf == NULL || Com_Buf_Len > MAX_PKT_SZ)
        {
            HostCtl[index].Interface[intf_num].InEndpTog[ep_num] = 0x00;
            return ERR_USB_TRANSFER;
        }
        spi_link_capture_packet(1u, ep_addr, ep_type, Com_Buf, Com_Buf_Len);

        slot->len = Com_Buf_Len;
        if (Com_Buf_Len != 0u)
        {
            memcpy(slot->buf, Com_Buf, Com_Buf_Len);
        }
        slot->valid = 1u;

        if (bDeviceState == CONFIGURED && _GetEPTxStatus(ep_addr) == EP_TX_NAK &&
            UDBD_ENDP_Busy(ep_addr) == 0)
        {
            status = USBD_ENDP_DataUp(ep_addr, slot->buf, slot->len);
            if (status == ERR_SUCCESS)
            {
                slot->valid = 0u;
            }
        }
    } else if (status == ERR_USB_DISCON)
    {
        LOG_INFO("usbh: device disconnected from host port");
        handle_device_disconnection();
    } else if (status == (USB_PID_STALL | ERR_USB_TRANSFER))
    {
        LOG_ERROR("usbh: abnormal event on host port endpoint %d", ep_addr);
        USBFSH_ClearEndpStall(RootHubDev.bEp0MaxPks, ep_addr | 0x80);
        HostCtl[index].Interface[intf_num].InEndpTog[ep_num] = 0x00;
        HostCtl[index].ErrorCount++;
        if (HostCtl[index].ErrorCount >= 20)
        {
            LOG_INFO("usbh: too many errors, scheduling re-enumeration");
            reenum_requested = 1u;
            HostCtl[index].ErrorCount = 0;
        }
    }
    return status;
}

/* Handle endpoint transfers for all interfaces and endpoints */
static uint8_t handle_endpoint_transfer(void)
{
    const uint8_t index = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;

    uint8_t temp_buf[MAX_PKT_SZ];
    uint16_t len;

    for (uint8_t intf_num = 0; intf_num < HostCtl[index].InterfaceNum; intf_num++)
    {
        uint8_t out_endp_num = HostCtl[index].Interface[intf_num].OutEndpNum;
        uint8_t in_endp_num = HostCtl[index].Interface[intf_num].InEndpNum;

        for (uint8_t ep_num = 0; ep_num < out_endp_num; ep_num++)
        {
            uint8_t ep_out = HostCtl[index].Interface[intf_num].OutEndpAddr[ep_num];
            uint8_t ep_type = HostCtl[index].Interface[intf_num].OutEndpType[ep_num] & 0x03u;
            uint8_t tx_status = handle_endpoint_tx(index, ep_out, ep_type, temp_buf, &len);
            if (tx_status == ERR_USB_DISCON)
            {
                return tx_status;
            }
        }

        for (uint8_t ep_num = 0; ep_num < in_endp_num; ep_num++)
        {
            uint8_t ep_addr = HostCtl[index].Interface[intf_num].InEndpAddr[ep_num];
            uint8_t ep_type = HostCtl[index].Interface[intf_num].InEndpType[ep_num] & 0x03u;
            uint32_t last_poll = HostCtl[index].Interface[intf_num].LastInPollTime[ep_num];
            uint32_t interval  = HostCtl[index].Interface[intf_num].InEndpInterval[ep_num];

            uint32_t current_time = bIntPackSOF;
            uint8_t should_poll = 0u;

            /* Bulk/control IN endpoints should be serviced as fast as possible. */
            if ((ep_type == 0x02u) || (ep_type == 0x00u))
            {
                should_poll = 1u;
            }
            else
            {
                if (interval == 0u)
                {
                    interval = 1u;
                }
                if ((current_time - last_poll) >= interval)
                {
                    should_poll = 1u;
                }
            }

            if (should_poll)
            {
                uint8_t rx_status;

                HostCtl[index].Interface[intf_num].LastInPollTime[ep_num] = current_time;
                /* Handle RX transfer */
                rx_status = handle_endpoint_rx(index, intf_num, ep_num, ep_addr, ep_type);
                if (rx_status == ERR_USB_DISCON) {
                    return rx_status;
                }
            }
            /* Set TX status if endpoint is not busy */
            if (UDBD_ENDP_Busy(ep_addr) == 0)
            {
                SetEPTxStatus(ep_addr, EP_TX_NAK);
            }
        }
    }

    return ERR_SUCCESS;
}

/* Main program loop */
int main(void)
{
    if (init_system() != ERR_SUCCESS)
    {
        LOG_ERROR("sys: system initialization failed");
        while (1); /* Halt on failure */
    }

    while (1)
    {
        uint8_t status = USBFSH_CheckRootHubPortStatus(RootHubDev.bStatus);

        if (status == ROOT_DEV_CONNECTED)
        {
            LOG_INFO("usb: port dev in");
            RootHubDev.bStatus = ROOT_DEV_CONNECTED;
            RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;
            handle_device_connection();
        } else if (status == ROOT_DEV_DISCONNECT)
        {
            handle_device_disconnection();
        }

        if ((reenum_requested != 0u) && (RootHubDev.bStatus >= ROOT_DEV_SUCCESS))
        {
            (void)handle_device_connection();
        }

        if (RootHubDev.bStatus >= ROOT_DEV_SUCCESS && bDeviceState == CONFIGURED)
        {
            uint8_t relay_status = usb_relay_poll();
            if (relay_status == ERR_USB_DISCON)
            {
                handle_device_disconnection();
                continue;
            }
            (void)handle_endpoint_transfer();
        }

        spi_link_task();

        if (user_reset_requested() != 0u)
        {
            user_clear_reset_request();
            LOG_INFO("system: resetting now");
            NVIC_SystemReset();
        }
    }
}




