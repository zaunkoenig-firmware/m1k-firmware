#include <linux/input.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "buttons.h"

int main ( int argc, char *argv[], char *env[] )
{
    if ( argc != 2 )
        return -1;

    int keys[] = {KEY_A, KEY_Z, KEY_S, KEY_X, KEY_ESC};
    bool states[] = {0, 0, 0, 0, 0};
    FILE *kbd = fopen(argv[1], "r");

    char key_map[KEY_MAX/8 + 1];    //  Create a byte array the size of the number of keys

    bool running = true;
    int32_t time_ticks = 0;

    BUTTONS_set_debounce_delay(4*1000);
    while (running) {
        memset(key_map, 0, sizeof(key_map));    //  Initate the array to zero's
        ioctl(fileno(kbd), EVIOCGKEY(sizeof(key_map)), key_map);    //  Fill the keymap with the current keyboard state

        int i;
        for (i = 0; i < 5; ++i) {
            const int keyb = key_map[keys[i]/8];
            const int mask = 1 << (keys[i] % 8);
            states[i] = !!(keyb & mask);
        }
        const bool kleftT  = states[0],
                   kleftB  = states[1],
                   krightT = states[2],
                   krightB = states[3],
                   kquit   = states[4];
        if (kquit)
            running = false;
        struct input inputs[2] = {
            {.T = kleftT,  .B = kleftB},
            {.T = krightT, .B = krightB}
        };
        time_ticks += 8;
        BUTTONS_task(8, inputs);
        fprintf(stderr, "\rIN lT=%i,lB=%i, rT=%i,rB=%i OUT l=%i, r=%i",
                kleftT, kleftB, krightT, krightB,
                BUTTONS_get(0), BUTTONS_get(1));


        usleep(1000);
    }
}
