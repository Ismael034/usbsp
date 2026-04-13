#include "queue.h"
#include "ch32v20x.h"
#include <string.h>

isr_queue_t isr_out_queue[MAX_EP_NUM];

volatile uint16_t isr_out_pending = 0;
uint8_t Host_OutBusy[MAX_EP_NUM] = {0};
uint8_t Host_OutToggle[MAX_EP_NUM] = {0};

uint8_t Host_InBusy[MAX_EP_NUM] = {0};
uint8_t Host_InToggle[MAX_EP_NUM] = {0};


uint8_t isr_enqueue_packet(uint8_t ep_num, const uint8_t *data, uint16_t len)
{
    if (ep_num == 0 || ep_num >= MAX_EP_NUM) return 1;
    isr_queue_t *q = &isr_out_queue[ep_num];

    if (q->count >= ISR_Q_DEPTH) {
        return 1; // queue full
    }

    if (len > MAX_PKT_SZ) len = MAX_PKT_SZ;
    memcpy(q->buf[q->tail], data, len);
    q->len[q->tail] = len;
    q->tail = (q->tail + 1) % ISR_Q_DEPTH;
    q->count++;
    isr_out_pending++;
    return 0;
}

uint8_t peek_packet_for_main(uint8_t ep_num, uint8_t *out_buf, uint16_t *out_len)
{
    if (ep_num == 0 || ep_num >= MAX_EP_NUM) return 1;
    isr_queue_t *q = &isr_out_queue[ep_num];

    if (q->count == 0) return 1;

    *out_len = q->len[q->head];
    if (*out_len > MAX_PKT_SZ) *out_len = MAX_PKT_SZ;
    memcpy(out_buf, q->buf[q->head], *out_len);
    return 0;
}

uint8_t pop_packet_for_main(uint8_t ep_num)
{
    if (ep_num == 0 || ep_num >= MAX_EP_NUM) return 1;
    isr_queue_t *q = &isr_out_queue[ep_num];

    __disable_irq();
    if (q->count == 0)
    {
        __enable_irq();
        return 1;
    }

    q->head = (q->head + 1) % ISR_Q_DEPTH;
    q->count--;
    isr_out_pending--;
    __enable_irq();
    return 0;
}

uint8_t pop_packet_for_main_and_check_space(uint8_t ep_num, uint8_t *has_space)
{
    if (ep_num == 0 || ep_num >= MAX_EP_NUM || has_space == NULL) return 1;
    isr_queue_t *q = &isr_out_queue[ep_num];

    __disable_irq();
    if (q->count == 0)
    {
        __enable_irq();
        return 1;
    }

    q->head = (q->head + 1) % ISR_Q_DEPTH;
    q->count--;
    isr_out_pending--;
    *has_space = (q->count < ISR_Q_DEPTH) ? 1u : 0u;
    __enable_irq();
    return 0;
}

uint8_t dequeue_packet_for_main(uint8_t ep_num, uint8_t *out_buf, uint16_t *out_len)
{
    if (peek_packet_for_main(ep_num, out_buf, out_len) != 0) return 1;
    return pop_packet_for_main(ep_num);
}

uint8_t isr_queue_has_space(uint8_t ep_num)
{
    uint8_t has_space;

    if (ep_num == 0 || ep_num >= MAX_EP_NUM) return 0;
    __disable_irq();
    has_space = (isr_out_queue[ep_num].count < ISR_Q_DEPTH) ? 1u : 0u;
    __enable_irq();
    return has_space;
}
