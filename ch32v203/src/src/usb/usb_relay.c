#include "usb_relay.h"

DEVICE device_table_relay =
{
	EP_NUM,
	1
};

DEVICE_PROP device_property_relay =
{
	usb_relay_init,
	usb_relay_reset,
	usb_relay_status_in,
	usb_relay_status_out,
	usb_relay_data_setup,
	usb_relay_nodata_setup,
	usb_relay_get_interface_setting,
	usb_relay_get_device_descriptor,
	usb_relay_get_config_descriptor,
	usb_relay_get_string_descriptor,
	0,
	DEF_USBD_UEP0_SIZE                                 
};

USER_STANDARD_REQUESTS user_standard_requests_relay =
{
	usbd_get_configuration,
	usb_relay_set_configuration,
	usbd_get_interface,
	usbd_set_interface,
	usbd_get_status, 
	usb_relay_clear_feature,
	usbd_set_endpoint_feature,
	usb_relay_set_device_feature,
	usbd_set_device_address
};

DEVICE_INFO	device_info_relay;


void usb_relay_driver_init()
{
	USB_Init(&device_info_relay, &device_table_relay, &device_property_relay, &user_standard_requests_relay);
}

uint8_t usb_relay_usbd_endp_data_up(uint8_t endp, uint8_t *pbuf, uint16_t len)
{
	return USBD_ENDP_DataUp(endp, pbuf, len);
}