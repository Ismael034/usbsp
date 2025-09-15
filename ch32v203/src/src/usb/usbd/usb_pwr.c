#include "usb_pwr.h"

__IO uint32_t bDeviceState = UNCONNECTED; 
__IO uint8_t fSuspendEnabled = TRUE;  
__IO uint32_t EP[8];

struct
{
  __IO usb_resume_state_t state;
  __IO uint8_t esof_count;
}
resume_state;

__IO uint32_t remote_wakeup_on = 0;

void usbd_resume_init(void)
{
    uint16_t wCNTR;
  
    wCNTR = _GetCNTR();
    wCNTR &= (~CNTR_LPMODE);
    _SetCNTR(wCNTR);      
    usb_hw_leave_lpm();
    _SetCNTR(IMR_MSK);
}

/**
 * @brief Handles USB resume signaling based on the specified resume state.
 * @param eResumeSetVal The resume state to set or process.
 */
void usbd_resume(usb_resume_state_t resume_set_val)
{
    uint16_t cntr;

    /* Update resume state if not ESOF */
    if (resume_set_val != RESUME_ESOF) {
        resume_state.state = resume_set_val;
    }

    /* Process resume state machine */
    switch (resume_state.state) {
        case RESUME_EXTERNAL:
            if (remote_wakeup_on == 0) {
                usbd_resume_init();
                resume_state.state = RESUME_OFF;
            } else {
                resume_state.state = RESUME_ON;
            }
            break;

        case RESUME_INTERNAL:
            usbd_resume_init();
            resume_state.state = RESUME_START;
            remote_wakeup_on = 1;
            break;

        case RESUME_LATER:
            resume_state.esof_count = 2;
            resume_state.state = RESUME_WAIT;
            break;

        case RESUME_WAIT:
            resume_state.esof_count--;
            if (resume_state.esof_count == 0) {
                resume_state.state = RESUME_START;
            }
            break;

        case RESUME_START:
            cntr = _GetCNTR();
            cntr |= CNTR_RESUME;
            _SetCNTR(cntr);
            resume_state.state = RESUME_ON;
            resume_state.esof_count = 10;
            break;

        case RESUME_ON:
            resume_state.esof_count--;
            if (resume_state.esof_count == 0) {
                cntr = _GetCNTR();
                cntr &= (~CNTR_RESUME);
                _SetCNTR(cntr);
                resume_state.state = RESUME_OFF;
                remote_wakeup_on = 0;
            }
            break;

        case RESUME_OFF:
        case RESUME_ESOF:
        default:
            resume_state.state = RESUME_OFF;
            break;
    }
}

void usbd_power_on(usb_state_t *status)
{
    _SetCNTR(CNTR_FRES);
    _SetCNTR(0);
    _SetISTR(0);
    _SetCNTR(CNTR_RESETM | CNTR_SUSPM | CNTR_WKUPM);

    *status = USB_SUCCESS;
}

void usbd_power_off(usb_state_t *status)
{
    _SetCNTR(CNTR_FRES); 
    _SetISTR(0); 
    _SetCNTR(CNTR_FRES + CNTR_PDWN);

    *status = USB_SUCCESS;
}

void usbd_suspend(void)
{
    uint32_t i = 0;
    uint16_t wCNTR;
    uint16_t EP[8];

    // Store current CNTR value
    wCNTR = _GetCNTR();

    // Save endpoint registers
    for (i = 0; i < 8; i++) {
        EP[i] = _GetENDPOINT(i);
    }

    // Enable reset interrupt
    wCNTR |= CNTR_RESETM;
    _SetCNTR(wCNTR);

    // Force USB reset
    wCNTR |= CNTR_FRES;
    _SetCNTR(wCNTR);

    // Clear force reset
    wCNTR &= ~CNTR_FRES;
    _SetCNTR(wCNTR);

    // Wait for reset interrupt
    while ((_GetISTR() & ISTR_RESET) == 0);

    // Clear reset interrupt flag
    _SetISTR((uint16_t)CLR_RESET);

    // Restore endpoint registers
    for (i = 0; i < 8; i++) {
        _SetENDPOINT(i, EP[i]);
    }

    // Enable suspend mode
    wCNTR |= CNTR_FSUSP;
    _SetCNTR(wCNTR);

    // Enable low power mode
    wCNTR = _GetCNTR();
    wCNTR |= CNTR_LPMODE;
    _SetCNTR(wCNTR);

    usb_hw_set_lpm();
}