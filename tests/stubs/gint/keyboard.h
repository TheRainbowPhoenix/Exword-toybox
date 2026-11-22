#ifndef TEST_KEYBOARD_H
#define TEST_KEYBOARD_H
#include <gint/keycodes.h>
#define KEYEV_NONE 0
#define KEYEV_TOUCH_DOWN 5
#define KEYEV_DOWN 1
#define KEYEV_HOLD 3
typedef struct {int type,key,x,y;int shift,alpha;} key_event_t;
key_event_t pollevent(void);
void clearevents(void);
int keydown(int);
int keycode_digit(int);
#endif
