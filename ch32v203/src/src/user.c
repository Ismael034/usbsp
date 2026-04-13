#include "user.h"
#include "usb_desc.h"
#include "external/microtlv/tlv.h"
#include <string.h>

#define DEBOUNCE_TICKS (50 * (SystemCoreClock / 1000))
static volatile uint32_t last_interrupt_time = 0;
volatile uint8_t button_pressed = 0;
static volatile uint8_t reset_requested = 0;

#define EEPROM_TOTAL_SIZE        256u
#define TLV_TYPE_VID             0x01u
#define TLV_TYPE_PID             0x02u
#define TLV_TYPE_BCD_DEVICE      0x03u
#define TLV_TYPE_MAX_POWER_MA    0x04u
#define TLV_TYPE_FLAGS           0x05u
#define TLV_TYPE_ATTACH_DELAY_MS 0x06u
#define TLV_TYPE_CAPTURE_MAX_B   0x07u
#define TLV_TYPE_MANUFACTURER    0x08u
#define TLV_TYPE_PRODUCT         0x09u
#define TLV_TYPE_SERIAL          0x0Au

#define TLV_FLAG_SELF_POWERED    (1u << 0)
#define TLV_FLAG_REMOTE_WAKEUP   (1u << 1)
#define TLV_FLAG_BOOT_CONNECTED  (1u << 2)
#define TLV_FLAG_CAPTURE_ON_BOOT (1u << 3)

#define TLV_TEXT_MAX_LEN         60u

static uint8_t utf16le_desc_to_ascii(const USBD_StringDescriptor_s *src, char *dst, uint16_t dst_len)
{
    uint16_t i;
    uint16_t b_len;
    uint16_t out = 0;

    if (!src || !dst || dst_len == 0u) {
        return 1u;
    }

    dst[0] = '\0';
    if (src->USBD_StringDescriptorSize < 2u) {
        return 1u;
    }

    b_len = src->USBD_StringDescriptor[0];
    if (b_len > src->USBD_StringDescriptorSize) {
        b_len = src->USBD_StringDescriptorSize;
    }
    if (b_len < 2u || src->USBD_StringDescriptor[1] != 0x03u) {
        return 1u;
    }

    for (i = 2u; (i + 1u) < b_len && (out + 1u) < dst_len; i += 2u) {
        uint8_t ch = src->USBD_StringDescriptor[i];
        if (ch == '\0') {
            break;
        }
        dst[out++] = (char)ch;
    }
    dst[out] = '\0';
    return 0u;
}

static uint8_t tlv_append_u16(uint8_t **p, uint32_t *left, uint8_t type, uint16_t value)
{
    uint8_t v[2];
    v[0] = (uint8_t)(value & 0xFFu);
    v[1] = (uint8_t)((value >> 8) & 0xFFu);
    return (tlv_format(p, left, type, 2u, v) == TLV_RESULT_SUCCESS) ? 0u : 1u;
}

static uint8_t tlv_append_u8(uint8_t **p, uint32_t *left, uint8_t type, uint8_t value)
{
    return (tlv_format(p, left, type, 1u, &value) == TLV_RESULT_SUCCESS) ? 0u : 1u;
}

static uint8_t tlv_append_str(uint8_t **p, uint32_t *left, uint8_t type, const char *s)
{
    uint32_t n = 0u;
    if (!s) {
        s = "";
    }
    while (s[n] != '\0' && n < TLV_TEXT_MAX_LEN) {
        n++;
    }
    return (tlv_format(p, left, type, n, (uint8_t *)s) == TLV_RESULT_SUCCESS) ? 0u : 1u;
}

uint8_t user_build_current_tlv_image(uint8_t *image, uint16_t image_size, uint16_t *used_len)
{
    uint8_t *p = image;
    uint32_t left = image_size;
    uint16_t local_vid = 0;
    uint16_t local_pid = 0;
    uint16_t local_bcd_device = 0;
    uint16_t local_max_power_ma = 100u;
    uint16_t attach_delay_ms = 0u;
    uint16_t capture_max_bytes = 64u;
    uint8_t flags = (uint8_t)(TLV_FLAG_BOOT_CONNECTED | TLV_FLAG_CAPTURE_ON_BOOT);
    char manufacturer[TLV_TEXT_MAX_LEN + 1];
    char product[TLV_TEXT_MAX_LEN + 1];
    char serial[TLV_TEXT_MAX_LEN + 1];
    const USB_ConfigDescriptor *cfg;

    if (!image || image_size == 0u) {
        return 1u;
    }

    memset(image, 0xFF, image_size);
    memset(manufacturer, 0, sizeof(manufacturer));
    memset(product, 0, sizeof(product));
    memset(serial, 0, sizeof(serial));

    if (USBD_SIZE_DEVICE_DESC >= 12u) {
        local_vid = (uint16_t)(((uint16_t)USBD_DeviceDescriptor[9] << 8) | USBD_DeviceDescriptor[8]);
        local_pid = (uint16_t)(((uint16_t)USBD_DeviceDescriptor[11] << 8) | USBD_DeviceDescriptor[10]);
        local_bcd_device = (uint16_t)(((uint16_t)USBD_DeviceDescriptor[13] << 8) | USBD_DeviceDescriptor[12]);
    }

    cfg = (const USB_ConfigDescriptor *)USBD_ConfigDescriptor;
    if (cfg && USBD_ConfigDescSize >= 9u) {
        local_max_power_ma = (uint16_t)cfg->bMaxPower * 2u;
        if ((cfg->bmAttributes & 0x40u) != 0u) flags |= TLV_FLAG_SELF_POWERED;
        if ((cfg->bmAttributes & 0x20u) != 0u) flags |= TLV_FLAG_REMOTE_WAKEUP;
    }

    (void)utf16le_desc_to_ascii(&USBD_StringDescriptor[1], manufacturer, (uint16_t)sizeof(manufacturer));
    (void)utf16le_desc_to_ascii(&USBD_StringDescriptor[2], product, (uint16_t)sizeof(product));
    (void)utf16le_desc_to_ascii(&USBD_StringDescriptor[3], serial, (uint16_t)sizeof(serial));

    if (tlv_append_u16(&p, &left, TLV_TYPE_VID, local_vid)) return 1u;
    if (tlv_append_u16(&p, &left, TLV_TYPE_PID, local_pid)) return 1u;
    if (tlv_append_u16(&p, &left, TLV_TYPE_BCD_DEVICE, local_bcd_device)) return 1u;
    if (tlv_append_u16(&p, &left, TLV_TYPE_MAX_POWER_MA, local_max_power_ma)) return 1u;
    if (tlv_append_u8(&p, &left, TLV_TYPE_FLAGS, flags)) return 1u;
    if (tlv_append_u16(&p, &left, TLV_TYPE_ATTACH_DELAY_MS, attach_delay_ms)) return 1u;
    if (tlv_append_u16(&p, &left, TLV_TYPE_CAPTURE_MAX_B, capture_max_bytes)) return 1u;
    if (tlv_append_str(&p, &left, TLV_TYPE_MANUFACTURER, manufacturer)) return 1u;
    if (tlv_append_str(&p, &left, TLV_TYPE_PRODUCT, product)) return 1u;
    if (tlv_append_str(&p, &left, TLV_TYPE_SERIAL, serial)) return 1u;

    if (tlv_format(&p, &left, 0u, 0u, NULL) != TLV_RESULT_SUCCESS) {
        return 1u;
    }

    if (used_len) {
        *used_len = (uint16_t)(image_size - left);
    }
    return 0u;
}

static uint8_t write_all_tlv_to_eeprom(void)
{
    uint8_t image[EEPROM_TOTAL_SIZE];

    if (user_build_current_tlv_image(image, EEPROM_TOTAL_SIZE, 0) != 0u) {
        return 1u;
    }

    AT24C02_write(0x00u, image, EEPROM_TOTAL_SIZE);
    return 0u;
}

void user_btn_init()
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    EXTI_InitTypeDef EXTI_InitStruct = {0};
    NVIC_InitTypeDef NVIC_InitStruct = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStruct.GPIO_Pin = BUTTON_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);

    GPIO_EXTILineConfig(BUTTON_PORT_SRC, BUTTON_PIN_SRC);

    EXTI_InitStruct.EXTI_Line = BUTTON_EXTI_LINE;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    NVIC_InitStruct.NVIC_IRQChannel = BUTTON_IRQ;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 10;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}

void user_led_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Pin = LED_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &GPIO_InitStruct);

    GPIO_ResetBits(LED_PORT, LED_PIN);
}

void user_led_toggle(void)
{
    GPIO_WriteBit(LED_PORT, LED_PIN, 
                    (BitAction)(1 - GPIO_ReadOutputDataBit(LED_PORT, LED_PIN)));
}

uint8_t user_btn_test()
{
	uint16_t counter = TIM2->CNT;

	while(counter + 20000 >= TIM2->CNT)
	{
    	if (button_pressed)
		{
			return 1;
		}
	}
	return 0;
}

void user_btn_handler(void)
{
    uint8_t s;

    user_led_toggle();
    LOG_INFO("user: button pressed");
    s = USBFSH_CheckRootHubPortStatus(RootHubDev.bStatus); // Check USB device connection or disconnection

    if(s == ROOT_DEV_CONNECTED || s == ROOT_DEV_FAILED)
    {
        LOG_INFO("usb: port dev in");

        RootHubDev.bStatus = ROOT_DEV_CONNECTED;
        RootHubDev.DeviceIndex = DEF_USBFS_PORT_INDEX * DEF_ONE_USB_SUP_DEV_TOTAL;
        s = usbh_enumerate_root_device(); // Simply enumerate root device
        
        if(s == ERR_SUCCESS)
        {
            if (write_all_tlv_to_eeprom() == 0u) {
                LOG_INFO("eeprom: write OK");
                AT24C02_read_usb_info();
                reset_requested = 1u;
                LOG_INFO("system: reset scheduled after button EEPROM write");
            } else {
                LOG_ERROR("eeprom: write failed");
            }
        } else {
            LOG_ERROR("user: enumerate failed %u", s);
        }
    } else {
        LOG_WARN("usb: no device connected, skip tlv write");
    }
    user_led_toggle();
}

void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(BUTTON_EXTI_LINE) != RESET)
    {
        uint32_t current_time = SysTick->CNT;
        uint32_t delta = (current_time - last_interrupt_time); 
        if (delta >= DEBOUNCE_TICKS)
        {  // Valid press
            last_interrupt_time = current_time;
            button_pressed = 1;
            user_btn_handler();
        } else
        {
            button_pressed = 0;
        }
        EXTI_ClearITPendingBit(BUTTON_EXTI_LINE);
    }
}

uint8_t user_reset_requested(void)
{
    return reset_requested;
}

void user_clear_reset_request(void)
{
    reset_requested = 0u;
}



