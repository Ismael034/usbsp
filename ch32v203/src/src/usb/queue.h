#ifndef __QUEUE_H__
#define __QUEUE_H__

#include "stdint.h"
#include "ch32v20x_usb.h"

#define ISR_Q_DEPTH   8
#define MAX_PKT_SZ    64
#define MAX_EP_NUM    4


typedef struct {
    uint8_t  buf[ISR_Q_DEPTH][MAX_PKT_SZ];
    uint16_t len[ISR_Q_DEPTH];
    uint16_t  head;
    uint16_t  tail;
    uint16_t  count;
} isr_queue_t;


extern isr_queue_t isr_out_queue[MAX_EP_NUM];


extern  volatile uint16_t isr_out_pending;
extern  uint8_t Host_OutBusy[MAX_EP_NUM];
extern  uint8_t Host_OutToggle[MAX_EP_NUM];
extern  uint8_t Host_InBusy[MAX_EP_NUM];
extern  uint8_t Host_InToggle[MAX_EP_NUM];

uint8_t isr_enqueue_packet(uint8_t ep_num, const uint8_t *data, uint16_t len);
uint8_t peek_packet_for_main(uint8_t ep_num, uint8_t *out_buf, uint16_t *out_len);
uint8_t pop_packet_for_main(uint8_t ep_num);
uint8_t dequeue_packet_for_main(uint8_t ep_num, uint8_t *out_buf, uint16_t *out_len);
uint8_t isr_queue_has_space(uint8_t ep_num);

#endif
