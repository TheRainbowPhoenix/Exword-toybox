#ifndef TEST_KEYBOARD_H
#define TEST_KEYBOARD_H
#define KEYEV_NONE 0
#define KEYEV_TOUCH_DOWN 5
#define KEYEV_DOWN 1
#define KEYEV_HOLD 3
#define KEY_EXE 1
#define KEY_EXIT 2
#define KEY_ACON 3
#define KEY_MENU 4
#define KEY_UP 5
#define KEY_DOWN 6
#define KEY_LEFT 7
#define KEY_RIGHT 8
typedef struct {int type,key,x,y;} key_event_t;
key_event_t pollevent(void);
void clearevents(void);
int keycode_digit(int);
#endif
