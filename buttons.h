#ifndef _BUTTONS_H_INCLUDED_
#define _BUTTONS_H_INCLUDED_

#include <stdint.h>
#include <stdbool.h>

struct input {
	bool T : 1;
	bool B : 1;
};

void BUTTONS_init(void);
void BUTTONS_set_debounce_delay(uint16_t delay);
void BUTTONS_task(uint8_t dtime, struct input[]);
bool BUTTONS_get(uint8_t num);

#endif /* _BUTTONS_H_INCLUDED_ */
