#include "debug.h"
#include "usb_lib.h"
#include "usbd.h"
#include "usbh.h"
#include "user.h"
#include "eeprom.h"
#include "usb_desc.h"
#include <stdbool.h>
#include <string.h>

#define ANSI_COLOR_RED   "\033[31m"
#define ANSI_COLOR_GREEN "\033[32m"
#define ANSI_COLOR_RESET "\033[0m"


static void print_header(void)
{
    printf("\033[H");
    printf("\033[2J");
    printf("[INFO] ----------------------------------------\r\n");
    printf("[INFO] CH32V203 hardware self-test\r\n");
    printf("[INFO] ----------------------------------------\r\n");
    printf("[INFO] System clock: %ld Hz\r\n", SystemCoreClock);
    printf("\r\n");
}

static void print_section(const char *name)
{
    printf("[INFO] %s\r\n", name);
}

static void print_result(const char *name, uint8_t ok)
{
    printf("[INFO] Test: %s\r\n", name);
    if (ok != 0u)
    {
        printf(ANSI_COLOR_GREEN "[PASS] %s" ANSI_COLOR_RESET "\r\n\r\n", name);
    }
    else
    {
        printf(ANSI_COLOR_RED "[FAIL] %s" ANSI_COLOR_RESET "\r\n\r\n", name);
    }
}

static void print_summary(const char *const *names, const uint8_t *results, uint8_t count)
{
    uint8_t i;
    uint8_t pass_count = 0;

    print_section("-------- Summary --------");

    for (i = 0; i < count; i++)
    {
        if (results[i] != 0u)
        {
            pass_count++;
        }
        if (results[i] != 0u)
        {
            printf(ANSI_COLOR_GREEN "[PASS] %s" ANSI_COLOR_RESET "\r\n", names[i]);
        }
        else
        {
            printf(ANSI_COLOR_RED "[FAIL] %s" ANSI_COLOR_RESET "\r\n", names[i]);
        }
    }

    printf("\r\n");
    if (pass_count == count)
    {
        printf("[INFO] Result: ALL PASS (%u/%u)\r\n", pass_count, count);
    }
    else
    {
        printf("[INFO] Result: HAS FAIL (%u/%u)\r\n", pass_count, count);
    }
}

static void init_user_button_input_only(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = BUTTON_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(BUTTON_PORT, &gpio);
}

static uint8_t wait_user_button_press(uint16_t timeout_ms)
{
    while (timeout_ms != 0u)
    {
        if (GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN) != Bit_RESET)
        {
            Delay_Ms(30);
            return (GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN) != Bit_RESET) ? 1u : 0u;
        }
        Delay_Ms(1);
        timeout_ms--;
    }
    return 0u;
}

static uint8_t usbd_init_test(void)
{
    DeviceDescParams dev_params;
    ConfigDescParams config_params;
    InterfaceDescParams interfaces[1];
    EndpointDescParams endpoints[1];
    uint8_t num_endpoints_per_interface[1];
    ClassSpecificParams class_params[1];
    StringDescParams str_params;

    static const uint8_t hid_report_desc[] = {
        0x05, 0x01,
        0x09, 0x06,
        0xA1, 0x01,
        0x05, 0x07,
        0x19, 0xE0,
        0x29, 0xE7,
        0x15, 0x00,
        0x25, 0x01,
        0x75, 0x01,
        0x95, 0x08,
        0x81, 0x02,
        0x95, 0x01,
        0x75, 0x08,
        0x81, 0x01,
        0x95, 0x05,
        0x75, 0x01,
        0x05, 0x08,
        0x19, 0x01,
        0x29, 0x05,
        0x91, 0x02,
        0x95, 0x01,
        0x75, 0x03,
        0x91, 0x01,
        0xC0
    };

    memset(&dev_params, 0, sizeof(dev_params));
    dev_params.device_version = 1001;
    dev_params.max_packet_size = 64;
    dev_params.product_id = 0x1234;
    dev_params.vendor_id = 0x5678;

    memset(&config_params, 0, sizeof(config_params));
    config_params.config_value = 0x01;
    config_params.attributes = 0x80;
    config_params.max_power = 0x32;
    config_params.num_interfaces = 1;

    memset(interfaces, 0, sizeof(interfaces));
    interfaces[0].interface_number = 0x00;
    interfaces[0].num_endpoints = 1;
    interfaces[0].class = 0x03;
    interfaces[0].subclass = 0x01;
    interfaces[0].protocol = 0x01;

    memset(endpoints, 0, sizeof(endpoints));
    endpoints[0].endpoint_address = 0x81;
    endpoints[0].attributes = 0x03;
    endpoints[0].max_packet_size = 64;
    endpoints[0].interval = 10;

    num_endpoints_per_interface[0] = 1;

    memset(class_params, 0, sizeof(class_params));
    class_params[0].class = 0x03;
    class_params[0].data.hid.report_descriptor = (uint8_t *)hid_report_desc;
    class_params[0].data.hid.report_desc_size = (uint16_t)sizeof(hid_report_desc);

    memset(&str_params, 0, sizeof(str_params));
    str_params.lang_id = 0x0409;
    str_params.vendor_str = "TestVendor";
    str_params.product_str = "TestHIDDevice";
    str_params.serial_str = "TESTSN0001";

    return usbd_test_descriptors_init(&dev_params,
                                      &config_params,
                                      interfaces,
                                      (uint8_t)config_params.num_interfaces,
                                      endpoints,
                                      num_endpoints_per_interface,
                                      &str_params,
                                      class_params);
}

int main(void)
{
    enum {
        TEST_USER_LED = 0,
        TEST_USER_BUTTON,
        TEST_EEPROM,
        TEST_DESC_SETUP,
        TEST_USBD,
        TEST_USBH,
        TEST_COUNT
    };

    static const char *const test_names[TEST_COUNT] = {
        "User LED blink",
        "User button press detection",
        "AT24C02 EEPROM read/write",
        "USB device descriptor setup",
        "USB device stack",
        "USB host stack"
    };

    uint8_t results[TEST_COUNT] = {0};

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    Delay_Init();
    USART_Debug_Init(115200);

    print_header();

    tim2_init((SystemCoreClock / 100) - 1);
    usbd_hw_set_clk();
    usbh_hw_set_clk();
    usb_hw_set_isr_config();

    printf("[INFO] Press ENTER to start tests\r\n\r\n");
    scanf("%*c");

    print_section("Peripherals");
    user_led_init();
    for (uint8_t i = 0; i < 10u; i++)
    {
        user_led_toggle();
        Delay_Ms(200);
        user_led_toggle();
        Delay_Ms(200);
    }
    results[TEST_USER_LED] = 1u;
    print_result(test_names[TEST_USER_LED], results[TEST_USER_LED]);

    init_user_button_input_only();
    printf("[INFO] Press USER button now...\r\n");
    results[TEST_USER_BUTTON] = wait_user_button_press(10000u);
    print_result(test_names[TEST_USER_BUTTON], results[TEST_USER_BUTTON]);

    AT24C02_init();
    results[TEST_EEPROM] = AT24C02_test();
    print_result(test_names[TEST_EEPROM], results[TEST_EEPROM]);

    print_section("USB device");
    results[TEST_DESC_SETUP] = (usbd_init_test() == 0u) ? 1u : 0u;
    print_result(test_names[TEST_DESC_SETUP], results[TEST_DESC_SETUP]);

    usbd_driver_init();
    results[TEST_USBD] = usbd_test();
    print_result(test_names[TEST_USBD], results[TEST_USBD]);

    print_section("USB host");
    usbh_init(ENABLE);
    results[TEST_USBH] = usbh_test();
    print_result(test_names[TEST_USBH], results[TEST_USBH]);

    print_summary(test_names, results, TEST_COUNT);

    print_section("Done");
    printf("[INFO] Test finished. Press RESET to restart\r\n");

    while (1)
    {
    }
}












