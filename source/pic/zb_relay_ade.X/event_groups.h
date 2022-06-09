/* 
 * File:   event_groups.h
 * Author: deviousprophet
 *
 * Created on April 9, 2022, 9:43 AM
 */

#ifndef EVENT_GROUPS_H
#define	EVENT_GROUPS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "sys_tick.h"

#define BIT(num)    BIT##num

    typedef uint8_t EventBits_t;

    void xEventGroupCreate(void);

    EventBits_t xEventGroupWaitBits(const EventBits_t uxBitsToWaitFor,
                                const sys_tick_t ticks_to_wait);

    EventBits_t xEventGroupSetBits(const EventBits_t uxBitsToSet);

    EventBits_t xEventGroupClearBits(const EventBits_t uxBitsToClear);

    EventBits_t xEventGroupGetBits(void);

#ifdef	__cplusplus
}
#endif

#endif	/* EVENT_GROUPS_H */

