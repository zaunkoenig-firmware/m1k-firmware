#ifndef _MOUSE_H_INCLUDED_
#define _MOUSE_H_INCLUDED_


#include <stdbool.h>
#include <stdint.h>

/**
 * Performs one step of mouse logic.
 *
 * @param time system time expressed in ticks (1/8000s)
 * @param left state of left button
 * @param right state of right button
 * @param tracking true iff mouse is tracking (not lifted above surface)
 * @param out_left pointer to store output left button state
 * @param out_right pointer to store output right button state
 * @param out_dx pointer to store output x delta (if return value is true)
 * @param out_dx pointer to store output y delta (if return value is true)
 *
 * @return true if mouse logic takes control over mouse delta, in this case
 *         values *out_dx and *out_dy should be sent as mouse delta;
 *         false if mouse logic does not take control over mouse delta,
 *         in this case actual values from the sensor should be sent as
 *         mouse delta
 */
bool mouse_step(int32_t time, bool left, bool right, bool tracking,
                bool *out_left, bool *out_right, int16_t *out_dx, int16_t *out_dy);
void mouse_get_params(int16_t *out_cpi, bool *out_as, int8_t *out_lod);

#endif
