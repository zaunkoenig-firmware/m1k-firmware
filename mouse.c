#include "mouse.h"

#define TICKS_FROM_US(us) ((us)/125)

#define PROGRAMMING_TIMEOUT TICKS_FROM_US(3000000)

#define ANIMATION_LENGTH_SQUARE 100
#define ANIMATION_DURATION_SQUARE TICKS_FROM_US(400000)

#define ANIMATION_LENGTH_SPIKE_SHORT 100
#define ANIMATION_LENGTH_SPIKE_LONG  300
#define ANIMATION_DURATION_SPIKE TICKS_FROM_US(100000)
#define ANIMATION_DURATION_SPIKE_INDICATION TICKS_FROM_US(150000)

#define CPI_DEFAULT 800
#define AS_DEFAULT 0
#define LOD_DEFAULT 2;

#define CPI_MAX 12000
#define CPI_MIN 100
#define CPI_LARGE_STEP 1000
#define CPI_SMALL_STEP 100

#define DFU_MODE_TIMEOUT TICKS_FROM_US(10000000)

enum state {
    POWERON,
    IDLE,
    ANIM_WAIT,
    ANIM_RIGHT,
    ANIM_DOWN,
    ANIM_LEFT,
    ANIM_UP,
    SHOW_CPI_HUNDREDS,
    WAIT_FOR_RELEASE,
    CPI
};

#ifdef TEST_MODE
#    include <stdio.h>
#    include <stdlib.h>
#    include <assert.h>
#    define DBG(fmt, ...) printf("\n" fmt, ##__VA_ARGS__)
#    define run_bootloader() {DBG("RUN BOOTLOADER\n"); exit(0);}
#    define store_config(cpi, as, lod) DBG("STORING CONFIG cpi=%i, as=%i, lod=%i\n", (int)(cpi), (int)(as), (int)lod)
#    define load_config(cpi, as, lod) { *(cpi) = CPI_DEFAULT; *(as) = 0; *(lod) = 2; DBG("LOADING CONFIG cpi=%i, as=%i, lod=%i\n", *cpi, *as, *lod); }
const char *stname(enum state s)
{
    switch (s) {
    case POWERON:           return "POWERON";
    case IDLE:              return "IDLE";
    case ANIM_WAIT:         return "ANIM_WAIT";
    case ANIM_RIGHT:        return "ANIM_RIGHT";
    case ANIM_DOWN:         return "ANIM_DOWN";
    case ANIM_LEFT:         return "ANIM_LEFT";
    case ANIM_UP:           return "ANIM_UP";
    case SHOW_CPI_HUNDREDS: return "SHOW_CPI_HUNDREDS";
    case WAIT_FOR_RELEASE:  return "WAIT_FOR_RELEASE";
    case CPI:               return "CPI";
    default:
        assert(false);
    }
}
#else
#    include "avr/eeprom.h"
#    include "atmel_bootloader.h"
#    define DBG(...)
#    define assert(x)
#    define stname(...)

static uint16_t EEMEM cpi_ee = CPI_DEFAULT;
static uint8_t  EEMEM as_ee = 0;
static uint8_t  EEMEM lod_ee = 2;

void store_config(int16_t cpi, bool as, int8_t lod)
{
    eeprom_write_word(&cpi_ee, (uint16_t)cpi);
    eeprom_busy_wait();

    eeprom_write_byte(&as_ee, (uint8_t)as);
    eeprom_busy_wait();

    eeprom_write_byte(&lod_ee, (uint8_t)lod);
    eeprom_busy_wait();
}

void load_config(int16_t *cpi, bool *as, int8_t *lod)
{
    *cpi = (int16_t)eeprom_read_word(&cpi_ee);
    *as  = (bool)eeprom_read_byte(&as_ee);
    *lod = (int8_t)eeprom_read_byte(&lod_ee);
}
#endif

static int16_t config_cpi;
static bool config_as;
static int8_t config_lod;

static bool mode_changed(int32_t time, bool left, bool right, bool tracking)
{
    enum state {
        WAIT_RELEASE_BOTH,
        WAIT_PRESS_BOTH,
        WAIT_TIMEOUT,
    };
    static enum state state = WAIT_RELEASE_BOTH;

    static int32_t start_wait = -1;

    bool rv = false;
    if (tracking) {
        state = WAIT_RELEASE_BOTH;
    } else {
        switch (state) {
        case WAIT_RELEASE_BOTH:
            if (!left && !right)
                state = WAIT_PRESS_BOTH;
            break;
        case WAIT_PRESS_BOTH:
            if (left && right) {
                state = WAIT_TIMEOUT;
                start_wait = time;
            }
            break;
        case WAIT_TIMEOUT:
            if (!(left && right)) {
                state = WAIT_RELEASE_BOTH;
            } else if (time > start_wait + PROGRAMMING_TIMEOUT) {
                rv = true;
                state = WAIT_RELEASE_BOTH;
            }
            break;
        }
    }

    return rv;
}

/**
 * @return -1 after small CPI decrease, -2 after large CPI decrease,
 *          1 after small CPI increase,  2 after large CPI increase,
 *          0 after no CPI increase
 */
static int8_t cpi_mode_step(bool left, bool right, bool tracking)
{
    static uint8_t ret = 0;
    static bool prev_left  = false,
                prev_right = false;
    static bool wait_for_release = true;
    static int8_t lock = 0;

    static int16_t prev_cpi;

    prev_cpi = config_cpi;
    ret = 0;
    if (!tracking) {
        wait_for_release = true;
    } else if (wait_for_release) {
        if (!left && !right) {
            wait_for_release = false;
            lock = 0;
        }
    } else {
        if (!lock && left && right && !prev_right) {
            lock = 1;
        } else if (!lock && right && left && !prev_left) {
            lock = -1;
        } else if (!left && prev_left) {
            if (lock == 1) {
                wait_for_release = true;
            } else if (right) {
                if (lock == -1) {
                    config_cpi -= CPI_LARGE_STEP;
                    ret = -2;
                }
            } else if (!lock) {
                config_cpi -= CPI_SMALL_STEP;
                ret = -1;
            }
        } else if (!right && prev_right) {
            if (lock == -1) {
                wait_for_release = true;
            } else if (left) {
                if (lock == 1) {
                    config_cpi += CPI_LARGE_STEP;
                    ret = 2;
                }
            } else if (!lock) {
                config_cpi += CPI_SMALL_STEP;
                ret = 1;
            }
        }
        if (!right && !left)
            lock = 0;
    }

    if (config_cpi > CPI_MAX)
        config_cpi = CPI_MAX;
    if (config_cpi < CPI_MIN)
        config_cpi = CPI_MIN;
    if (config_cpi == prev_cpi)
        ret = 0;

    prev_left = left;
    prev_right = right;

    return ret;
}

bool mouse_step(int32_t time, bool left, bool right, bool tracking,
                bool *out_left, bool *out_right, int16_t *out_dx, int16_t *out_dy)
{
    enum anim_type {
        ANIM_SQUARE,
        ANIM_SQUARE_INV,
        ANIM_SPIKE_DOWN,
        ANIM_SPIKE_UP,
        ANIM_SPIKE_RIGHT
    };
    static enum state state = POWERON;
    static enum anim_type anim_type;
    static enum state next_state = IDLE;
    static int8_t anim_repeat;
    static int16_t anim_length;
    static int16_t anim_length_wait;
    static int16_t actual_length;
    static int16_t anim_step;
    static int16_t actual_step;
    static int16_t anim_pos;
    static int32_t last_anim;
    static int32_t boot_press_time = -1;
    static enum state next_next_state;

#define SET_ANIM_FIRST_STATE(type, nstate)                              \
    {                                                                   \
        if (type == ANIM_SQUARE) {                                      \
            DBG("state %s -> ANIM_RIGHT (-> ANIM_DOWN -> ANIM_LEFT"     \
                " -> ANIM_UP -> %s)\n",                                 \
                stname(state), stname(nstate));                         \
            state = ANIM_RIGHT;                                         \
        } else if (type == ANIM_SQUARE_INV) {                           \
            DBG("state %s -> ANIM_LEFT (-> ANIM_DOWN -> ANIM_RIGHT"     \
                " -> ANIM_UP -> %s)\n",                                 \
                stname(state), stname(nstate));                         \
            state = ANIM_LEFT;                                          \
        } else if (type == ANIM_SPIKE_RIGHT) {                          \
            DBG("state %s -> ANIM_RIGHT (-> ANIM_LEFT -> %s)\n",        \
                stname(state), stname(nstate));                         \
            state = ANIM_RIGHT;                                         \
        } else if (type == ANIM_SPIKE_UP) {                             \
            DBG("state %s -> ANIM_UP (-> ANIM_DOWN -> %s)\n",           \
                stname(state), stname(nstate));                         \
            state = ANIM_UP;                                            \
        } else if (type == ANIM_SPIKE_DOWN) {                           \
            DBG("state %s -> ANIM_DOWN (-> ANIM_UP -> %s)\n",           \
                stname(state), stname(nstate));                         \
            state = ANIM_DOWN;                                          \
        } else {                                                        \
            assert(false);                                              \
        }                                                               \
    }

#define ANIMATE(type, len, dur, repeat, wait, nstate)                           \
    if ((repeat) > 0) {                                                         \
        next_state = (nstate);                                                  \
        anim_type = (type);                                                     \
        SET_ANIM_FIRST_STATE(type, nstate);                                     \
        anim_length = (len);                                                    \
        anim_length_wait = (wait);                                              \
        anim_step = dur / ((((type) == ANIM_SQUARE || (type) == ANIM_SQUARE_INV) ? 4 : 2)*(anim_length)); \
        anim_pos = 0;                                                           \
        anim_repeat = (repeat);                                                 \
        last_anim = time;                                                       \
    } else {                                                                    \
        state = nstate;                                                         \
    }

#define ANIMATE_CONTINUE()                      \
    {                                           \
        state = ANIM_WAIT;                      \
        last_anim = time;                       \
        --anim_repeat;                          \
    }

    bool rv = false;

    *out_left = left;
    *out_right = right;

    switch (state) {
    case POWERON:
        *out_left = *out_right = false;
        if (boot_press_time == -1) {
            load_config(&config_cpi, &config_as, &config_lod);
            if (left && right) {
                config_as = false;
                config_lod = 2;
                store_config(config_cpi, config_as, config_lod);
                boot_press_time = time;
                ANIMATE(ANIM_SQUARE_INV, ANIMATION_LENGTH_SQUARE, ANIMATION_DURATION_SQUARE, 1, TICKS_FROM_US(1000000), POWERON);
                state = ANIM_WAIT;
            } else if (left) {
                config_as = true;
                store_config(config_cpi, config_as, config_lod);
                boot_press_time = time;
                ANIMATE(ANIM_SQUARE, ANIMATION_LENGTH_SQUARE, ANIMATION_DURATION_SQUARE, 1, TICKS_FROM_US(1000000), POWERON);
                state = ANIM_WAIT;
            } else if (right) {
                config_lod = 3;
                store_config(config_cpi, config_as, config_lod);
                boot_press_time = time;
                ANIMATE(ANIM_SQUARE, ANIMATION_LENGTH_SQUARE, ANIMATION_DURATION_SQUARE, 1, TICKS_FROM_US(1000000), POWERON);
                state = ANIM_WAIT;
            } else {
                DBG("state POWERON -> IDLE\n");
                state = IDLE;
            }
        } else {
            if (left && right && time > boot_press_time + DFU_MODE_TIMEOUT) {
                config_lod = LOD_DEFAULT;
                config_as = AS_DEFAULT;
                config_cpi = CPI_DEFAULT;
                store_config(config_cpi, config_as, config_lod);
                DBG("Running bootloader\n");
                run_bootloader();
            } else if (!left && !right) {
                state = IDLE;
            }
        }
        break;
    case IDLE:
        if (mode_changed(time, left, right, tracking)) {
            ANIMATE(ANIM_SPIKE_RIGHT, ANIMATION_LENGTH_SPIKE_SHORT, ANIMATION_DURATION_SPIKE_INDICATION, config_cpi / 1000, ANIMATION_DURATION_SPIKE_INDICATION,
                    SHOW_CPI_HUNDREDS);
            next_next_state = CPI;
        }
        break;
    case CPI:
        *out_left = *out_right = false;
        if (mode_changed(time, left, right, tracking)) {
            store_config(config_cpi, config_as, config_lod);
            ANIMATE(ANIM_SPIKE_RIGHT, ANIMATION_LENGTH_SPIKE_SHORT, ANIMATION_DURATION_SPIKE_INDICATION, config_cpi / 1000, ANIMATION_DURATION_SPIKE_INDICATION,
                    SHOW_CPI_HUNDREDS);
            next_next_state = IDLE;
        } else {
            const int8_t ret = cpi_mode_step(left, right, tracking);
            if (ret == 0)
                break;
            if (ret > 0) {
                if (ret == 1) {
                    ANIMATE(ANIM_SPIKE_UP, ANIMATION_LENGTH_SPIKE_SHORT, ANIMATION_DURATION_SPIKE, 1, 0, CPI);
                } else {
                    ANIMATE(ANIM_SPIKE_UP, ANIMATION_LENGTH_SPIKE_LONG,  ANIMATION_DURATION_SPIKE, 1, 0, CPI);
                }
            } else if (ret < 0) {
                if (ret == -1) {
                    ANIMATE(ANIM_SPIKE_DOWN, ANIMATION_LENGTH_SPIKE_SHORT, ANIMATION_DURATION_SPIKE, 1, 0, CPI);
                } else {
                    ANIMATE(ANIM_SPIKE_DOWN, ANIMATION_LENGTH_SPIKE_LONG,  ANIMATION_DURATION_SPIKE, 1, 0, CPI);
                }
            } else {
                assert(false);
            }
        }
        break;
    case ANIM_WAIT:
    case ANIM_RIGHT:
    case ANIM_DOWN:
    case ANIM_LEFT:
    case ANIM_UP:
        *out_left = *out_right = false;
        *out_dx = *out_dy = 0;
        rv = true;
        actual_length = (state == ANIM_WAIT) ? anim_length_wait : anim_length;
        actual_step = (state == ANIM_WAIT) ? 1 : anim_step;
        if (anim_pos < actual_length && time > last_anim + actual_step) {
            switch (state) {
            case ANIM_WAIT:  break;
            case ANIM_RIGHT: *out_dx = 1;  break;
            case ANIM_DOWN:  *out_dy = 1;  break;
            case ANIM_LEFT:  *out_dx = -1; break;
            case ANIM_UP:    *out_dy = -1; break;
            default:
                assert(false);
            }
            ++anim_pos;
            last_anim = time;
        } else if (anim_pos >= actual_length) {
            anim_pos = 0;
            switch (state) {
            case ANIM_WAIT:
                SET_ANIM_FIRST_STATE(anim_type, next_state);
                break;
            case ANIM_RIGHT:
                if (anim_type == ANIM_SQUARE) {
                    DBG("state ANIM_RIGHT -> ANIM_DOWN\n");
                    state = ANIM_DOWN; break;
                } else if (anim_type == ANIM_SQUARE_INV) {
                    DBG("state ANIM_RIGHT -> ANIM_UP\n");
                    state = ANIM_UP; break;
                } else if (anim_type == ANIM_SPIKE_RIGHT) {
                    DBG("state ANIM_RIGHT -> ANIM_LEFT\n");
                    state = ANIM_LEFT; break;
                } else {
                    assert(false);
                }
            case ANIM_DOWN:
                if (anim_type == ANIM_SQUARE) {
                    DBG("state ANIM_DOWN -> ANIM_LEFT\n");
                    state = ANIM_LEFT; break;
                } else if (anim_type == ANIM_SQUARE_INV) {
                    DBG("state ANIM_DOWN -> ANIM_RIGHT\n");
                    state = ANIM_RIGHT; break;
                } else if (anim_type == ANIM_SPIKE_DOWN) {
                    DBG("state ANIM_DOWN -> ANIM_UP\n");
                    state = ANIM_UP; break;
                } else if (anim_type == ANIM_SPIKE_UP) {
                    if (anim_repeat == 1) {
                        DBG("state ANIM_DOWN -> %s\n", stname(next_state));
                        state = next_state;
                    } else {
                        ANIMATE_CONTINUE();
                    }
                    break;
                } else {
                    assert(false);
                }
            case ANIM_LEFT:
                if (anim_type == ANIM_SQUARE) {
                    DBG("state ANIM_LEFT -> ANIM_UP\n");
                    state = ANIM_UP; break;
                } else if (anim_type == ANIM_SQUARE_INV) {
                    DBG("state ANIM_LEFT -> ANIM_DOWN\n");
                    state = ANIM_DOWN; break;
                } else if (anim_type == ANIM_SPIKE_RIGHT) {
                    if (anim_repeat == 1) {
                        DBG("state ANIM_LEFT -> %s\n", stname(next_state));
                        state = next_state;
                    } else {
                        ANIMATE_CONTINUE();
                    }
                    break;
                }
            case ANIM_UP:
                if (anim_type == ANIM_SQUARE) {
                    DBG("state ANIM_UP -> %s\n", stname(next_state));
                    state = next_state; break;
                } else if (anim_type == ANIM_SQUARE_INV) {
                    DBG("state ANIM_UP -> %s\n", stname(next_state));
                    state = next_state; break;
                } else if (anim_type == ANIM_SPIKE_DOWN) {
                    DBG("state ANIM_UP -> %s\n", stname(next_state));
                    state = next_state; break;
                } else if (anim_type == ANIM_SPIKE_UP) {
                    DBG("state ANIM_UP -> ANIM_DOWN\n");
                    state = ANIM_DOWN; break;
                } else {
                    assert(false);
                }
            default:
                assert(false);
            }
        }
        break;
    case SHOW_CPI_HUNDREDS:
        ANIMATE(ANIM_SPIKE_UP, ANIMATION_LENGTH_SPIKE_SHORT, ANIMATION_DURATION_SPIKE_INDICATION, (config_cpi % 1000)/100, ANIMATION_DURATION_SPIKE_INDICATION,
                WAIT_FOR_RELEASE);
        next_state = next_next_state;
        if (state != WAIT_FOR_RELEASE) {
            state = ANIM_WAIT;
        }
        break;
    case WAIT_FOR_RELEASE:
        *out_left = *out_right = false;
        if (!left && !right) {
            DBG("state WAIT_FOR_RELEASE -> %s\n", stname(next_state));
            state = next_state;
        }
        break;
    }
    return rv;
}

void mouse_get_params(int16_t *out_cpi, bool *out_as, int8_t *out_lod)
{
    *out_cpi = config_cpi;
    *out_as = config_as;
    *out_lod = config_lod;
}
