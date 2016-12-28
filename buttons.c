#include "buttons.h"

#include <stddef.h>

#ifdef TEST_MODE
#    include <stdio.h>
#    include <stdlib.h>
#    include <assert.h>
#    define DBG(fmt, ...) printf("\n" fmt, ##__VA_ARGS__)
#else
#    include <avr/io.h>
#    define DBG(...)
#    define assert(x)
#    define stname(...)
#endif

#define SCHMITT

static int32_t debounce_delay_tics = 0;
static bool state[2] = {false, false};

void BUTTONS_init(void)
{
}

void BUTTONS_set_debounce_delay(uint16_t delay)
{
    debounce_delay_tics = delay & 0x7fff;
}

#ifdef SCHMITT
void BUTTONS_task(uint8_t dtime, struct input input[])
{
    for (int i = 0; i < 2; ++i) {
        if (state[i])
            state[i] = !input[i].T || input[i].B;
        else
            state[i] = input[i].B && !input[i].T;
    }
}
#else
void BUTTONS_task(uint8_t dtime, struct input input[])
{
    static uint16_t time = 0;
    static uint16_t tmr[2] = {0, 0};

    const uint16_t ntime = (time + dtime) & 0x7fff;
    if (ntime < time) { /* time is wrapping - clean up timers */
        for (int i = 0; i < 2; ++i) {
            if (tmr[i] & 0x8000)
                tmr[i] &= 0x7fff;
            else
                tmr[i] = 0;
        }
    }
    time = ntime;
    for (int i = 0; i < 2; ++i) {
        if (input[i].B && !input[i].T) {
            state[i] = true;
            tmr[i] = 0;
        }
        if (time < tmr[i])
            continue;
        const bool newstate = !input[i].T;
        if (state[i] != newstate) {
            state[i] = newstate;
            tmr[i] = time + debounce_delay_tics;
        }
    }
}
#endif

bool BUTTONS_get(uint8_t num)
{
    return state[num];
}
