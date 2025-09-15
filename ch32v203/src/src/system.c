#include "system.h"

uint32_t Get_SysTick(void) {
    return SysTick->CNT / (SystemCoreClock / 1000); // Assuming SysTick is configured for 1ms
}