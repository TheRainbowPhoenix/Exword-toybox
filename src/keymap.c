#include "shell_keys.h"
#include <gint/keycodes.h>

/* The EX-word physical keycodes Q..M are contiguous QWERTY order.
   Number row is ALPHA+Q..P, as independently used by EXWord-Explorer. */
int shell_key_translate(int key, int shift, int alpha)
{
    if (key >= KEY_EX_Q && key <= KEY_EX_M) {
        int i = key - KEY_EX_Q;
        if (alpha) {
            if (key == KEY_EX_A) return '.';
            if (i < 10) return "1234567890"[i];
        }
        char c = "qwertyuiopasdfghjklzxcvbnm"[i];
        return shift ? c - ('a'-'A') : c;
    }
    switch (key) {
        case KEY_EXE: return SHELL_KEY_ENTER;
        case KEY_DEL: return SHELL_KEY_BACK;
        case KEY_EXIT: case KEY_ACON: case KEY_MENU: return SHELL_KEY_ESC;
        case KEY_UP: return SHELL_KEY_UP;
        case KEY_DOWN: return SHELL_KEY_DOWN;
        case KEY_LEFT: return SHELL_KEY_LEFT;
        case KEY_RIGHT: return SHELL_KEY_RIGHT;
        case KEY_PAGEUP: return SHELL_KEY_PAGEUP;
        case KEY_PAGEDOWN: return SHELL_KEY_PAGEDOWN;
        /* EX-word speaker/audio and layout buttons: hardware keyboard toggle.
           Some firmware may not expose a separate volume-rocker event. */
        case KEY_EX_AUDIO: case KEY_EX_LAYOUT: return SHELL_KEY_KBD_TOGGLE;
        case KEY_EX_JUMP: return '\t';
        case KEY_EX_ZOOM: return ' '; /* Convenient physical space key. */
        case KEY_EX_HISTORY: return SHELL_KEY_UP;
        /* The calculator-style 0 key is a convenient physical SPACE;
           SHIFT+0 is TAB when there's no dedicated tab key. */
        case KEY_0: return shift ? '\t' : ' ';
        case KEY_DOT: return '.';
        case KEY_COMMA: return ',';
        case KEY_LEFTP: return '(';
        case KEY_RIGHTP: return ')';
        case KEY_ADD: return '+';
        case KEY_SUB: return '-';
        case KEY_MUL: return '*';
        case KEY_DIV: return '/';
        case KEY_POWER: return '^';
        case KEY_FRAC: return '_';
        case KEY_FD: return ':';
        case KEY_ARROW: return '>';
        case KEY_NEG: return '~';
        case KEY_EXP: return '=';
        default: return 0;
    }
}
