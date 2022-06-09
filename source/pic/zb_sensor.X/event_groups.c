#include "event_groups.h"
#include "sys_tick.h"

EventBits_t uxBits;

void xEventGroupCreate(void) {
    uxBits = 0;
}

EventBits_t xEventGroupWaitBits(const EventBits_t uxBitsToWaitFor,
                                const sys_tick_t ticks_to_wait) {

    sys_tick_t last_tick = sys_tick_get_tick();
    while (sys_tick_get_tick() - last_tick <= ticks_to_wait)
        if (uxBits & uxBitsToWaitFor) return uxBits & uxBitsToWaitFor;
    return uxBits;
}

EventBits_t xEventGroupSetBits(const EventBits_t uxBitsToSet) {
    do {
        uxBits |= uxBitsToSet;
    } while ((uxBits & uxBitsToSet) != uxBitsToSet);
    return uxBits;
}

EventBits_t xEventGroupClearBits(const EventBits_t uxBitsToClear) {
    do {
        uxBits &= ~uxBitsToClear;
    } while (uxBits & uxBitsToClear);
    return uxBits;
}

EventBits_t xEventGroupGetBits(void) {
    return uxBits;
}
