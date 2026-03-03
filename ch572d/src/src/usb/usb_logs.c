#include "usb_logs.h"
#include "usb_cdc.h"
#include "CH57x_common.h"
#include <string.h>

#define USB_LOGS_MAX_LEN 64
#define USB_LOGS_QUEUE_LEN 16

static uint8_t q_buf[USB_LOGS_QUEUE_LEN][USB_LOGS_MAX_LEN];
static uint8_t q_len[USB_LOGS_QUEUE_LEN];
static uint8_t q_head;
static uint8_t q_tail;
static uint8_t q_count;
static uint8_t usb_configured;

void usb_logs_init(void)
{
    usb_configured = 0;
    q_head = 0;
    q_tail = 0;
    q_count = 0;
}

void usb_logs_on_config(uint8_t configured)
{
    usb_configured = configured ? 1 : 0;
    if (!usb_configured) {
        q_head = 0;
        q_tail = 0;
        q_count = 0;
    }
}

void usb_logs_on_ep1_in_ready(uint8_t ready)
{
    if (!ready || !usb_configured) {
        return;
    }
    if (q_count) {
        usb_cdc_send_ep1(q_buf[q_tail], q_len[q_tail]);
        q_tail++;
        if (q_tail >= USB_LOGS_QUEUE_LEN) {
            q_tail = 0;
        }
        q_count--;
    }
}

void usb_logs_task(uint8_t ep1_ready)
{
    usb_logs_on_ep1_in_ready(ep1_ready);
}

void usb_logs_push(const char *msg)
{
    size_t len;
    if (!msg || !usb_configured) {
        return;
    }
    len = strlen(msg);
    if (len > USB_LOGS_MAX_LEN) {
        len = USB_LOGS_MAX_LEN;
    }

    // If the queue is full, drop the oldest message so we keep the latest context.
    if (q_count >= USB_LOGS_QUEUE_LEN) {
        q_tail++;
        if (q_tail >= USB_LOGS_QUEUE_LEN) {
            q_tail = 0;
        }
        q_count--;
    }

    memcpy(q_buf[q_head], msg, len);
    q_len[q_head] = (uint8_t)len;
    q_head++;
    if (q_head >= USB_LOGS_QUEUE_LEN) {
        q_head = 0;
    }
    q_count++;
}
