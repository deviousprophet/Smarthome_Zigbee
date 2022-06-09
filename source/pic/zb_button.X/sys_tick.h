/* 
 * File:   sys_tick.h
 * Author: deviousprophet
 *
 * Created on April 19, 2022, 10:09 PM
 */

#ifndef SYS_TICK_H
#define	SYS_TICK_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

#define SYS_TICK_RESOLUTION 50000U
#define SYS_TICK_RELOAD_VAL UINT16_MAX - SYS_TICK_RESOLUTION
#define MAX_SYS_TICK_HANDLE 2

    typedef uint8_t sys_tick_handle_t;
    typedef uint32_t sys_tick_t;

    typedef void (*sys_tick_cb_t)(void);

    void sys_tick_init(void);
    sys_tick_t sys_tick_get_tick(void);
    sys_tick_handle_t sys_tick_register_callback_handler(sys_tick_cb_t cb);
    void sys_tick_start_oneshot(sys_tick_handle_t handle, sys_tick_t tick);
    void sys_tick_start_periodic(sys_tick_handle_t handle, sys_tick_t tick);
    void sys_tick_stop(sys_tick_handle_t handle);
//    void sys_tick_pause(void);
//    void sys_tick_resume(void);

#ifdef	__cplusplus
}
#endif

#endif	/* SYS_TICK_H */

