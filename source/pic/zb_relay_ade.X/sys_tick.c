#include "sys_tick.h"
#include "it_handle.h"

struct {

    struct {
        uint8_t _handler_index : 3;
        uint8_t _reserved : 5;
    };

    struct {

        struct {
            uint8_t _en : 1;
            uint8_t _periodic : 1;
            uint8_t _handle : 3;
            uint8_t _reserved : 3;
        };
        sys_tick_t trigger_tick;
        sys_tick_t counter_tick;
        sys_tick_cb_t cb;
    } _handle[MAX_SYS_TICK_HANDLE];

    sys_tick_t tick;
} _sys_tick;

void sys_tick_handler(void) {
    //    Reload Timer1
    TMR1 = SYS_TICK_RELOAD_VAL;
    _sys_tick.tick++;

    for (uint8_t i = 0; i < _sys_tick._handler_index; i++) {
        _sys_tick._handle[i].counter_tick++;
        if (_sys_tick._handle[i]._en) {
            if (_sys_tick._handle[i].counter_tick
                >= _sys_tick._handle[i].trigger_tick) {
                _sys_tick._handle[i].counter_tick = 0;
                _sys_tick._handle[i].cb();
                if (!_sys_tick._handle[i]._periodic)
                    _sys_tick._handle[i]._en = false;
            }
        }
    }
}

void sys_tick_init(void) {

#if (_XTAL_FREQ == 8000000)
    T1CONbits.T1CKPS = 0b10;
#else
    T1CONbits.T1CKPS = 0b01;
#endif

    T1CONbits.TMR1CS = 0;
    TMR1 = SYS_TICK_RELOAD_VAL;
    timer1_add_isr_handler(sys_tick_handler);
    T1CONbits.TMR1ON = 1;
    _sys_tick.tick = 0;
    _sys_tick._handler_index = 0;
}

sys_tick_t sys_tick_get_tick(void) {
    return _sys_tick.tick;
}

sys_tick_handle_t sys_tick_register_callback_handler(sys_tick_cb_t cb) {
    uint8_t _handler_index = _sys_tick._handler_index;
    if (_handler_index < MAX_SYS_TICK_HANDLE) {
        _sys_tick._handle[_handler_index].cb = cb;
        _sys_tick._handle[_handler_index]._en = false;
        _sys_tick._handle[_handler_index]._handle = _handler_index;
        _sys_tick._handler_index++;
        return _sys_tick._handle[_handler_index]._handle;
    } else return 0xFF;
}

void sys_tick_start_oneshot(sys_tick_handle_t handle, sys_tick_t tick) {
    for (uint8_t i = 0; i < _sys_tick._handler_index; i++) {
        if (_sys_tick._handle[i]._handle == handle) {
            _sys_tick._handle[i].trigger_tick = tick;
            _sys_tick._handle[i].counter_tick = 0;
            _sys_tick._handle[i]._periodic = false;
            _sys_tick._handle[i]._en = true;
        }
    }
}

void sys_tick_start_periodic(sys_tick_handle_t handle, sys_tick_t tick) {
    for (uint8_t i = 0; i < _sys_tick._handler_index; i++) {
        if (_sys_tick._handle[i]._handle == handle) {
            _sys_tick._handle[i].trigger_tick = tick;
            _sys_tick._handle[i].counter_tick = 0;
            _sys_tick._handle[i]._periodic = true;
            _sys_tick._handle[i]._en = true;
            return;
        }
    }
}

void sys_tick_stop(sys_tick_handle_t handle) {
    for (uint8_t i = 0; i < _sys_tick._handler_index; i++) {
        if (_sys_tick._handle[i]._handle == handle) {
            _sys_tick._handle[i]._en = false;
            return;
        }
    }
}

//void sys_tick_pause(void) {
//    PIE1bits.TMR1IE = 0;
//}
//
//void sys_tick_resume(void) {
//    PIE1bits.TMR1IE = 1;
//}