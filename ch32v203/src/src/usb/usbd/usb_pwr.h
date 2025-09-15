#ifndef __USB_PWR_H
#define __USB_PWR_H

#include "ch32v20x.h"
#include "usbd.h"

typedef enum _RESULT usb_state_t;

typedef enum _device_state
{
  UNCONNECTED,
  ATTACHED,
  POWERED,
  SUSPENDED,
  ADDRESSED,
  CONFIGURED
} device_state;

typedef enum _usb_resume_state_t
{
  RESUME_EXTERNAL,
  RESUME_INTERNAL,
  RESUME_LATER,
  RESUME_WAIT,
  RESUME_START,
  RESUME_ON,
  RESUME_OFF,
  RESUME_ESOF
} usb_resume_state_t;

void usbd_suspend(void);
void usbd_resume_tnit(void);
void usbd_resume(usb_resume_state_t eResumeSetVal);
void usbd_power_on(usb_state_t *status);
void usbd_power_off(usb_state_t *status);

extern  __IO uint32_t bDeviceState; /* USB device status */
extern __IO uint8_t fSuspendEnabled;  /* true when suspend is possible */

#endif