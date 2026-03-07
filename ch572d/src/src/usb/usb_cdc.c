#include "CH57x_common.h"
#include "usb_cdc.h"
#include "usb_logs.h"
#include "usb_webusb.h"
#include "usb_webusb_cmds.h"
#include "../debug_log.h"

#define THIS_ENDP0_SIZE         64
#define MAX_PACKET_SIZE         64
#define USB_IRQ_FLAG_NUM        4

#define ENDP0                   0x00
#define ENDP1                   0x01
#define ENDP4                   0x04

#define ENDP_TYPE_IN            0x00
#define ENDP_TYPE_OUT           0x01

#define OUT_ACK                 0
#define OUT_NAK                 2
#define IN_ACK                  0
#define IN_NAK                  2

#define DEF_SET_LINE_CODING     0x20
#define DEF_GET_LINE_CODING     0x21
#define DEF_SET_CONTROL_LINE_STATE 0x22

#define USB_DESCR_TYP_DEVICE    0x01
#define USB_DESCR_TYP_CONFIG    0x02
#define USB_DESCR_TYP_STRING    0x03
#define USB_DESCR_TYP_BOS       0x0F

#define LOG_UART_ENABLED        1

static const uint8_t tab_usb_cdc_dev_des[18] = {
    0x12, 0x01, 0x10, 0x01,
    0xEF, 0x02, 0x01, 0x40,
    0x09, 0x12, 0x01, 0x00,
    0x00, 0x30,
    0x01, 0x02, 0x03, 0x01
};

static const uint8_t tab_usb_cdc_cfg_des[] = {
    0x09, 0x02, 0x62, 0x00, 0x03, 0x01, 0x00, 0x80, 0x30,

    0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x04,

    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x04,

    0x05, 0x24, 0x00, 0x10, 0x01,
    0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, 0x00, 0x01,
    0x05, 0x24, 0x01, 0x01, 0x00,

    0x07, 0x05, 0x84, 0x03, 0x08, 0x00, 0x01,

    0x09, 0x04, 0x01, 0x00, 0x02, 0x0a, 0x00, 0x00, 0x00,

    0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00,
    0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00,

    0x09, 0x04, 0x02, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x05,
    0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
    0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00
};

static const uint8_t tab_usb_lid_str_des[] = { 0x04, 0x03, 0x09, 0x04 };

static const uint8_t usb_dev_para_cdc_serial_str[] =     "usbsp-0001";
static const uint8_t usb_dev_para_cdc_product_str[] =    "usbsp cdc+WebUSB";
static const uint8_t usb_dev_para_cdc_manufacture_str[] = "usbsp";
static const uint8_t usb_dev_para_cdc_logs_str[] =       "usbsp logs (cdc)";
static const uint8_t usb_dev_para_webusb_str[] =         "usbsp WebUSB";

static uint8_t tab_cdc_line_coding[] = {
    0x85, 0x20, 0x00, 0x00,
    0x00,
    0x00,
    0x08
};

typedef struct {
    uint8_t usb_config;
    uint8_t usb_address;
    uint8_t setup_req;
    uint8_t setup_len;
} dev_info_t;

static dev_info_t dev_info;
static uint8_t setup_req_code;
static uint8_t setup_len;
static const uint8_t *p_descr;

__attribute__((aligned(4))) static uint8_t ep0_buffer[MAX_PACKET_SIZE];
__attribute__((aligned(4))) static uint8_t ep1_buffer[2 * MAX_PACKET_SIZE];
__attribute__((aligned(4))) static uint8_t ep2_buffer[2 * MAX_PACKET_SIZE];
__attribute__((aligned(4))) static uint8_t ep3_buffer[2 * MAX_PACKET_SIZE];

static uint8_t ep1_data_out_flag;
static uint8_t ep1_data_out_len;
__attribute__((aligned(4))) static uint8_t ep1_out_data_buf[MAX_PACKET_SIZE];
static uint8_t ep1_data_in_flag;

static uint8_t ep2_data_out_flag;
static uint8_t ep2_data_out_len;
__attribute__((aligned(4))) static uint8_t ep2_out_data_buf[MAX_PACKET_SIZE];
static uint8_t ep2_data_in_flag;

static uint8_t ep3_data_in_flag;
static uint8_t ep4_data_in_flag;

static uint8_t usb_irq_w_idx;
static uint8_t usb_irq_r_idx;
static volatile uint8_t usb_irq_len[USB_IRQ_FLAG_NUM];
static volatile uint8_t usb_irq_pid[USB_IRQ_FLAG_NUM];
static volatile uint8_t usb_irq_flag[USB_IRQ_FLAG_NUM];

typedef struct __attribute__((packed)) {
    uint32_t baud_rate;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t data_bits;
} line_code_t;

static line_code_t cdc_line_code;

static __attribute__((unused)) void uart_log(const char *msg)
{
#if LOG_UART_ENABLED
    if (msg) {
        UART_SendString((uint8_t *)msg, (uint16_t)strlen(msg));
    }
#else
    (void)msg;
#endif
}

static uint8_t build_string_desc(const uint8_t *ascii, uint8_t *buf)
{
    uint8_t i = 0;
    if (!ascii || !buf) {
        return 0;
    }

    while (ascii[i] && (2 + i * 2) < MAX_PACKET_SIZE) {
        buf[2 + i * 2] = ascii[i];
        buf[3 + i * 2] = 0x00;
        i++;
    }

    buf[0] = (uint8_t)(2 + i * 2);
    buf[1] = USB_DESCR_TYP_STRING;
    return buf[0];
}

static void usb_dev_epn_in_set_status(uint8_t ep_num, uint8_t type, uint8_t sta)
{
    uint8_t *p_uepn_ctrl = (uint8_t *)(USB_BASE_ADDR + 0x22 + ep_num * 4);
    if (type == ENDP_TYPE_IN) {
        *((PUINT8V)p_uepn_ctrl) = (*((PUINT8V)p_uepn_ctrl) & (~0x03)) | sta;
    } else {
        *((PUINT8V)p_uepn_ctrl) = (*((PUINT8V)p_uepn_ctrl) & (~(0x03 << 2))) | (sta << 2);
    }
}

__attribute__((interrupt("WCH-Interrupt-fast")))
__attribute__((section(".highcode")))
void USB_IRQHandler(void)
{
    uint8_t j;

    if (R8_USB_INT_FG & RB_UIF_TRANSFER) {
        if ((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN) {
            usb_irq_flag[usb_irq_w_idx] = 1;
            usb_irq_pid[usb_irq_w_idx] = R8_USB_INT_ST;
            usb_irq_len[usb_irq_w_idx] = R8_USB_RX_LEN;

            switch (usb_irq_pid[usb_irq_w_idx] & 0x3f) {
                case UIS_TOKEN_OUT | 2:
                    if (R8_USB_INT_FG & RB_U_TOG_OK) {
                        R8_UEP2_CTRL ^= RB_UEP_R_TOG;
                        R8_UEP2_CTRL = (R8_UEP2_CTRL & 0xf3) | 0x08;
                        for (j = 0; j < (MAX_PACKET_SIZE / 4); j++) {
                            ((uint32_t *)ep2_out_data_buf)[j] = ((uint32_t *)ep2_buffer)[j];
                        }
                    } else {
                        usb_irq_flag[usb_irq_w_idx] = 0;
                    }
                    break;
                case UIS_TOKEN_IN | 2:
                    R8_UEP2_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP2_CTRL = (R8_UEP2_CTRL & 0xfc) | IN_NAK;
                    break;
                case UIS_TOKEN_OUT | 1:
                    if (R8_USB_INT_FG & RB_U_TOG_OK) {
                        R8_UEP1_CTRL ^= RB_UEP_R_TOG;
                        R8_UEP1_CTRL = (R8_UEP1_CTRL & 0xf3) | 0x08;
                        for (j = 0; j < (MAX_PACKET_SIZE / 4); j++) {
                            ((uint32_t *)ep1_out_data_buf)[j] = ((uint32_t *)ep1_buffer)[j];
                        }
                    } else {
                        usb_irq_flag[usb_irq_w_idx] = 0;
                    }
                    break;
                case UIS_TOKEN_IN | 1:
                    R8_UEP1_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & 0xfc) | IN_NAK;
                    break;
                case UIS_TOKEN_OUT | 0:
                    if (R8_USB_INT_FG & RB_U_TOG_OK) {
                        R8_UEP0_CTRL = (R8_UEP0_CTRL & 0xf3) | 0x08;
                    } else {
                        usb_irq_flag[usb_irq_w_idx] = 0;
                    }
                    break;
                case UIS_TOKEN_IN | 0:
                    R8_UEP0_CTRL = (R8_UEP0_CTRL & 0xfc) | IN_NAK;
                    break;
                default:
                    break;
            }

            if (usb_irq_flag[usb_irq_w_idx]) {
                usb_irq_w_idx++;
                if (usb_irq_w_idx >= USB_IRQ_FLAG_NUM) {
                    usb_irq_w_idx = 0;
                }
            }

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }

        if (R8_USB_INT_ST & RB_UIS_SETUP_ACT) {
            usb_irq_flag[usb_irq_w_idx] = 1;
            usb_irq_pid[usb_irq_w_idx] = UIS_TOKEN_SETUP | 0;
            usb_irq_len[usb_irq_w_idx] = 8;
            usb_irq_w_idx++;
            if (usb_irq_w_idx >= USB_IRQ_FLAG_NUM) {
                usb_irq_w_idx = 0;
            }
            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
    }
}

uint8_t usb_cdc_send_ep1(const uint8_t *data, uint16_t len)
{
    memcpy(&ep1_buffer[MAX_PACKET_SIZE], data, len);

    ep1_data_in_flag = 0;
    R8_UEP1_T_LEN = (uint8_t)len;
    PFIC_DisableIRQ(USB_IRQn);
    R8_UEP1_CTRL = R8_UEP1_CTRL & 0xfc;
    PFIC_EnableIRQ(USB_IRQn);

    return 0;
}

static uint8_t send_usb_data_ep2(uint8_t *data, uint16_t len)
{
    memcpy(&ep2_buffer[MAX_PACKET_SIZE], data, len);

    ep2_data_in_flag = 0;
    R8_UEP2_T_LEN = (uint8_t)len;
    PFIC_DisableIRQ(USB_IRQn);
    R8_UEP2_CTRL = R8_UEP2_CTRL & 0xfc;
    PFIC_EnableIRQ(USB_IRQn);

    return 0;
}

static uint8_t webusb_send_ep2(const uint8_t *data, uint8_t len)
{
    return send_usb_data_ep2((uint8_t *)data, len);
}

static void usb_para_init(void)
{
    ep1_data_in_flag = 1;
    ep1_data_out_flag = 0;
    ep2_data_in_flag = 1;
    ep2_data_out_flag = 0;
    ep3_data_in_flag = 1;
    ep4_data_in_flag = 1;
}

static void init_cdc_device(void)
{
    usb_para_init();

    R8_USB_CTRL = 0x00;

    R8_UEP4_1_MOD = RB_UEP4_TX_EN | RB_UEP1_TX_EN | RB_UEP1_RX_EN;
    R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN | RB_UEP3_TX_EN;

    R16_UEP0_DMA = (uint32_t)&ep0_buffer[0];
    R16_UEP1_DMA = (uint32_t)&ep1_buffer[0];
    R16_UEP2_DMA = (uint32_t)&ep2_buffer[0];
    R16_UEP3_DMA = (uint32_t)&ep3_buffer[0];

    R8_UEP0_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
    R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP3_CTRL = UEP_T_RES_NAK;
    R8_UEP4_CTRL = UEP_T_RES_NAK;

    R8_USB_DEV_AD = 0x00;

    R16_PIN_ALTERNATE |= RB_PIN_USB_EN;
    R8_UDEV_CTRL = RB_UD_PD_DIS;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;

    R8_USB_INT_FG = 0xFF;
    R8_USB_INT_EN = RB_UIE_TRANSFER;
    PFIC_EnableIRQ(USB_IRQn);

    R8_UDEV_CTRL |= RB_UD_PORT_EN;

    dev_info.usb_config = 0;
    dev_info.usb_address = 0;
}

static void init_usb_dev_para(void)
{
    uint8_t i;
    for (i = 0; i < USB_IRQ_FLAG_NUM; i++) {
        usb_irq_flag[i] = 0;
    }

    cdc_line_code.baud_rate = 115200;
    cdc_line_code.stop_bits = 0;
    cdc_line_code.parity = 0;
    cdc_line_code.data_bits = 8;

    usb_logs_init();
    usb_webusb_cmds_init(webusb_send_ep2);
}

static void usb_irq_process_handler(void)
{
    uint8_t i;
    uint8_t len;
    uint8_t data_dir;
    uint8_t str_buf[MAX_PACKET_SIZE];

    i = usb_irq_r_idx;
    if (usb_irq_flag[i]) {
        usb_irq_r_idx++;
        if (usb_irq_r_idx >= USB_IRQ_FLAG_NUM) {
            usb_irq_r_idx = 0;
        }

        switch (usb_irq_pid[i] & 0x3f) {
            case UIS_TOKEN_IN | 4:
                ep4_data_in_flag = 1;
                break;
            case UIS_TOKEN_IN | 3:
                ep3_data_in_flag = 1;
                break;
            case UIS_TOKEN_OUT | 2:
                ep2_data_out_len = usb_irq_len[i];
                ep2_data_out_flag = 1;
                PFIC_DisableIRQ(USB_IRQn);
                R8_UEP2_CTRL = R8_UEP2_CTRL & 0xf3;
                PFIC_EnableIRQ(USB_IRQn);
                break;
            case UIS_TOKEN_IN | 2:
                ep2_data_in_flag = 1;
                break;
            case UIS_TOKEN_OUT | 1:
                ep1_data_out_len = usb_irq_len[i];
                ep1_data_out_flag = 1;
                PFIC_DisableIRQ(USB_IRQn);
                R8_UEP1_CTRL = R8_UEP1_CTRL & 0xf3;
                PFIC_EnableIRQ(USB_IRQn);
                break;
            case UIS_TOKEN_IN | 1:
                ep1_data_in_flag = 1;
                usb_logs_on_ep1_in_ready(1);
                break;
            case UIS_TOKEN_SETUP | 0:
            {
                PUSB_SETUP_REQ setup_req = (PUSB_SETUP_REQ)ep0_buffer;
                setup_req_code = setup_req->bRequest;
                setup_len = (uint8_t)setup_req->wLength;
                if (setup_req->wLength >> 8) {
                    setup_len = 0xff;
                }

                data_dir = USB_REQ_TYP_OUT;
                if (setup_req->bRequestType & USB_REQ_TYP_IN) {
                    data_dir = USB_REQ_TYP_IN;
                }

                LOG_DEBUG("SETUP rt=%02X r=%02X v=%04X i=%04X l=%u",
                          setup_req->bRequestType,
                          setup_req->bRequest,
                          setup_req->wValue,
                          setup_req->wIndex,
                          (unsigned)setup_req->wLength);

                len = 0;
                if ((setup_req->bRequestType & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD) {
                    switch (setup_req_code) {
                        case USB_GET_DESCRIPTOR:
                            switch (setup_req->wValue >> 8) {
                                case USB_DESCR_TYP_DEVICE:
                                    p_descr = tab_usb_cdc_dev_des;
                                    len = sizeof(tab_usb_cdc_dev_des);
                                    break;
                                case USB_DESCR_TYP_CONFIG:
                                    p_descr = tab_usb_cdc_cfg_des;
                                    len = sizeof(tab_usb_cdc_cfg_des);
                                    break;
                                case USB_DESCR_TYP_BOS:
                                {
                                    uint16_t bos_len = 0;
                                    p_descr = usb_webusb_get_bos(&bos_len);
                                    len = (uint8_t)bos_len;
                                    break;
                                }
                                case USB_DESCR_TYP_STRING:
                                    switch (setup_req->wValue & 0xff) {
                                        case 0:
                                            p_descr = tab_usb_lid_str_des;
                                            len = sizeof(tab_usb_lid_str_des);
                                            break;
                                        case 1:
                                            len = build_string_desc(usb_dev_para_cdc_manufacture_str, str_buf);
                                            p_descr = str_buf;
                                            break;
                                        case 2:
                                            len = build_string_desc(usb_dev_para_cdc_product_str, str_buf);
                                            p_descr = str_buf;
                                            break;
                                        case 3:
                                            len = build_string_desc(usb_dev_para_cdc_serial_str, str_buf);
                                            p_descr = str_buf;
                                            break;
                                        case 4:
                                            len = build_string_desc(usb_dev_para_cdc_logs_str, str_buf);
                                            p_descr = str_buf;
                                            break;
                                        case 5:
                                            len = build_string_desc(usb_dev_para_webusb_str, str_buf);
                                            p_descr = str_buf;
                                            break;
                                        default:
                                            len = 0;
                                            break;
                                    }
                                    break;
                                default:
                                    len = 0;
                                    break;
                            }
                            if (setup_len > len) {
                                setup_len = len;
                            }
                            break;
                        case USB_SET_ADDRESS:
                            dev_info.usb_address = (uint8_t)(setup_req->wValue & 0xff);
                            break;
                        case USB_SET_CONFIGURATION:
                            dev_info.usb_config = (uint8_t)(setup_req->wValue & 0xff);
                            usb_logs_on_config(dev_info.usb_config);
                            if (dev_info.usb_config) {
                                LOG_INFO("USB: CONFIG=%u", dev_info.usb_config);
                            }
                            break;
                        case USB_GET_CONFIGURATION:
                            ep0_buffer[0] = dev_info.usb_config;
                            p_descr = ep0_buffer;
                            setup_len = 1;
                            break;
                        case USB_GET_STATUS:
                            ep0_buffer[0] = 0;
                            ep0_buffer[1] = 0;
                            p_descr = ep0_buffer;
                            setup_len = 2;
                            break;
                        case USB_GET_INTERFACE:
                            ep0_buffer[0] = 0;
                            p_descr = ep0_buffer;
                            setup_len = 1;
                            break;
                        case USB_SET_INTERFACE:
                        case USB_CLEAR_FEATURE:
                        case USB_SET_FEATURE:
                            break;
                        default:
                            setup_len = 0;
                            break;
                    }
                } else if ((setup_req->bRequestType & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) {
                    switch (setup_req_code) {
                        case DEF_GET_LINE_CODING:
                            p_descr = tab_cdc_line_coding;
                            if (setup_len > sizeof(tab_cdc_line_coding)) {
                                setup_len = sizeof(tab_cdc_line_coding);
                            }
                            break;
                        case DEF_SET_LINE_CODING:
                        case DEF_SET_CONTROL_LINE_STATE:
                            break;
                        default:
                            setup_len = 0;
                            break;
                    }
                } else if ((setup_req->bRequestType & USB_REQ_TYP_MASK) == USB_REQ_TYP_VENDOR) {
                    uint16_t tmp_len = 0;
                    if (usb_webusb_handle_vendor_request(setup_req, &p_descr, &tmp_len)) {
                        if (tmp_len > 0xFF) {
                            tmp_len = 0xFF;
                        }
                        setup_len = (uint8_t)tmp_len;
                    } else {
                        setup_len = 0;
                    }
                } else {
                    setup_len = 0;
                }

                if (data_dir == USB_REQ_TYP_IN) {
                    len = (setup_len >= THIS_ENDP0_SIZE) ? THIS_ENDP0_SIZE : setup_len;
                    if (len) {
                        memcpy(ep0_buffer, p_descr, len);
                        setup_len -= len;
                        p_descr += len;
                    }
                    R8_UEP0_T_LEN = len;
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
                    PFIC_EnableIRQ(USB_IRQn);
                } else {
                    R8_UEP0_T_LEN = 0;
                    PFIC_DisableIRQ(USB_IRQn);
                    if (setup_req_code == DEF_SET_LINE_CODING) {
                        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
                    } else {
                        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
                    }
                    PFIC_EnableIRQ(USB_IRQn);
                }
                break;
            }
            case UIS_TOKEN_IN | 0:
            {
                if (setup_req_code == USB_GET_DESCRIPTOR) {
                    len = (setup_len >= THIS_ENDP0_SIZE) ? THIS_ENDP0_SIZE : setup_len;
                    memcpy(ep0_buffer, p_descr, len);
                    setup_len -= len;
                    p_descr += len;
                    if (len) {
                        R8_UEP0_T_LEN = len;
                        PFIC_DisableIRQ(USB_IRQn);
                        R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                        usb_dev_epn_in_set_status(ENDP0, ENDP_TYPE_IN, IN_ACK);
                        PFIC_EnableIRQ(USB_IRQn);
                    } else {
                        R8_UEP0_T_LEN = len;
                        PFIC_DisableIRQ(USB_IRQn);
                        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
                        PFIC_EnableIRQ(USB_IRQn);
                    }
                } else if (setup_req_code == USB_SET_ADDRESS) {
                    R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | dev_info.usb_address;
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
                    PFIC_EnableIRQ(USB_IRQn);
                } else if (setup_req_code == DEF_GET_LINE_CODING) {
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
                    PFIC_EnableIRQ(USB_IRQn);
                } else if (setup_req_code == DEF_SET_LINE_CODING) {
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
                    PFIC_EnableIRQ(USB_IRQn);
                } else {
                    R8_UEP0_T_LEN = 0;
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
                    PFIC_EnableIRQ(USB_IRQn);
                }
                break;
            }
            case UIS_TOKEN_OUT | 0:
            {
                len = usb_irq_len[i];
                if (len) {
                    if (setup_req_code == DEF_SET_LINE_CODING) {
                        memcpy(tab_cdc_line_coding, ep0_buffer, sizeof(tab_cdc_line_coding));
                        memcpy(&cdc_line_code.baud_rate, ep0_buffer, 4);
                        cdc_line_code.stop_bits = ep0_buffer[4];
                        cdc_line_code.parity = ep0_buffer[5];
                        cdc_line_code.data_bits = ep0_buffer[6];
                        PFIC_DisableIRQ(USB_IRQn);
                        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
                        PFIC_EnableIRQ(USB_IRQn);
                    } else {
                        PFIC_DisableIRQ(USB_IRQn);
                        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
                        PFIC_EnableIRQ(USB_IRQn);
                    }
                } else {
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
                    PFIC_EnableIRQ(USB_IRQn);
                }
                break;
            }
            default:
                break;
        }

        PFIC_DisableIRQ(USB_IRQn);
        usb_irq_flag[i] = 0;
        PFIC_EnableIRQ(USB_IRQn);
    }

    if (ep1_data_out_flag) {
        ep1_data_out_flag = 0;
    }

    if (ep2_data_out_flag) {
        ep2_data_out_flag = 0;
        if (ep2_data_in_flag) {
            usb_webusb_cmds_handle(ep2_out_data_buf, ep2_data_out_len);
        }
    }

    usb_logs_task(ep1_data_in_flag);

    if (R8_USB_INT_FG & RB_UIF_BUS_RST) {
        R8_UEP0_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP3_CTRL = UEP_T_RES_NAK;
        R8_UEP4_CTRL = UEP_T_RES_NAK;

        ep1_data_in_flag = 0;
        ep2_data_in_flag = 0;
        ep3_data_in_flag = 0;
        ep4_data_in_flag = 0;

        ep1_data_out_flag = 0;
        ep2_data_out_flag = 0;

        R8_USB_DEV_AD = 0x00;
        dev_info.usb_address = 0;
        dev_info.usb_config = 0;
        usb_logs_on_config(0);
        LOG_INFO("USB: RESET");

        R8_USB_INT_FG = RB_UIF_BUS_RST;
    } else if (R8_USB_INT_FG & RB_UIF_SUSPEND) {
        if (R8_USB_MIS_ST & RB_UMS_SUSPEND) {
            ep1_data_in_flag = 0;
            ep2_data_in_flag = 0;
            ep3_data_in_flag = 0;
            ep4_data_in_flag = 0;

            ep1_data_out_flag = 0;
            ep2_data_out_flag = 0;
            LOG_INFO("USB: SUSPEND");
        } else {
            ep1_data_in_flag = 1;
            ep2_data_in_flag = 1;
            ep3_data_in_flag = 1;
            ep4_data_in_flag = 1;

            ep1_data_out_flag = 0;
            ep2_data_out_flag = 0;
            LOG_INFO("USB: RESUME");
        }
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
}

void usb_cdc_init(void)
{
    init_usb_dev_para();
    init_cdc_device();
}

void usb_cdc_task(void)
{
    usb_irq_process_handler();
}
