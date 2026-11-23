#pragma once
#include <stdint.h>
static inline void vTaskDelay(uint32_t ticks)
{
    (void)ticks;
}
