#include <linux/input.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mouse.h"

int main ( int argc, char *argv[], char *env[] )
{
    if ( argc != 2 )
        return -1;

    int keys[] = {KEY_Z, KEY_X, KEY_L, KEY_ESC};
    bool states[] = {0, 0, 0, 0};
    FILE *kbd = fopen(argv[1], "r");

    char key_map[KEY_MAX/8 + 1];    //  Create a byte array the size of the number of keys

    bool running = true;
    int32_t time_ticks = 0;
    while (running) {
        memset(key_map, 0, sizeof(key_map));    //  Initate the array to zero's
        ioctl(fileno(kbd), EVIOCGKEY(sizeof(key_map)), key_map);    //  Fill the keymap with the current keyboard state

        int i;
        for (i = 0; i < 4; ++i) {
            const int keyb = key_map[keys[i]/8];
            const int mask = 1 << (keys[i] % 8);
            states[i] = !!(keyb & mask);
        }
        const bool kleft  = states[0],
                   kright = states[1],
                   ktrack = states[2],
                   kquit  = states[3];
        if (kquit)
            running = false;
        static int16_t in_x = 0, in_y = 0;
        ++in_x, --in_y;
        bool out_left, out_right;
        int16_t out_x, out_y;
        bool use_out = mouse_step(time_ticks, kleft, kright, ktrack,
                                  &out_left, &out_right, &out_x, &out_y);
        if (!use_out) {
            out_x = in_x;
            out_y = in_y;
        }
        int16_t cpi;
        bool as;
        int8_t lod;
        mouse_get_params(&cpi, &as, &lod);
        fprintf(stderr, "\rIN l=%i, r=%i, track=%i  OUT l=%i, r=%i, x=%6.5ji, y=%6.5ji, cpi=%i, mode=%i%i",
                kleft, kright, ktrack,
                out_left, out_right, (intmax_t)out_x, (intmax_t)out_y, (int)cpi, (int)as, (int)lod);

        time_ticks += 8;

        usleep(1000);
    }
}
