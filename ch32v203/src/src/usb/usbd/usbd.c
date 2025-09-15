#include "usbd.h"


DEVICE device_table =
{
	EP_NUM,
	1
};

DEVICE_PROP device_property =
{
	usbd_init,
	usbd_reset,
	usbd_status_in,
	usbd_status_out,
	usbd_data_setup,
	usbd_nodata_setup,
	usbd_get_interface_setting,
	usbd_get_device_descriptor,
	usbd_get_config_descriptor,
	usbd_get_string_descriptor,
	0,
	DEF_USBD_UEP0_SIZE                                 
};

USER_STANDARD_REQUESTS user_standard_requests =
{
	usbd_get_configuration,
	usbd_set_configuration,
	usbd_get_interface,
	usbd_set_interface,
	usbd_get_status, 
	usbd_clear_feature,
	usbd_set_endpoint_feature,
	usbd_set_device_feature,
	usbd_set_device_address
};

DEVICE_INFO	device_info;


void usbd_driver_init()
{
	USB_Init(&device_info, &device_table, &device_property, &user_standard_requests);
}

uint8_t usbd_test()
{
	uint16_t counter = TIM2->CNT;

	while(counter + 20000 >= TIM2->CNT)
	{
    	if (bDeviceState == ADDRESSED)
		{
			return 1;
		}
	}
	return 0;
}
