#ifndef TOYBOX_SHELL_KEYS_H
#define TOYBOX_SHELL_KEYS_H
/* Shell input values >= 0x100 are not printable ASCII bytes. */
#define SHELL_KEY_ENTER       0x100
#define SHELL_KEY_BACK        0x101
#define SHELL_KEY_LEFT        0x102
#define SHELL_KEY_RIGHT       0x103
#define SHELL_KEY_UP          0x104
#define SHELL_KEY_DOWN        0x105
#define SHELL_KEY_ESC         0x106
#define SHELL_KEY_PAGEUP      0x107
#define SHELL_KEY_PAGEDOWN    0x108
#define SHELL_KEY_KBD_TOGGLE  0x109
/* Translate gint's EX-word keycodes and held SHIFT/ALPHA modifiers. 
   Returns 0 for an unmapped key; pure function suitable for host tests. */
int shell_key_translate(int code, int shift, int alpha);
#endif
