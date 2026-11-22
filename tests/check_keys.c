#include "shell_keys.h"
#include <gint/keycodes.h>
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(shell_key_translate(KEY_EX_Q,0,0)=='q');
    assert(shell_key_translate(KEY_EX_M,0,0)=='m');
    assert(shell_key_translate(KEY_EX_A,1,0)=='A');
    assert(shell_key_translate(KEY_EX_Q,0,1)=='1');
    assert(shell_key_translate(KEY_EX_P,0,1)=='0');
    assert(shell_key_translate(KEY_EX_A,0,1)=='.');
    assert(shell_key_translate(KEY_0,0,0)==' ');
    assert(shell_key_translate(KEY_0,1,0)=='\t');
    assert(shell_key_translate(KEY_EX_JUMP,0,0)=='\t');
    assert(shell_key_translate(KEY_EX_ZOOM,0,0)==' ');
    assert(shell_key_translate(KEY_EX_HISTORY,0,0)==SHELL_KEY_UP);
    assert(shell_key_translate(KEY_PAGEUP,0,0)==SHELL_KEY_PAGEUP);
    assert(shell_key_translate(KEY_PAGEDOWN,0,0)==SHELL_KEY_PAGEDOWN);
    assert(shell_key_translate(KEY_EX_AUDIO,0,0)==SHELL_KEY_KBD_TOGGLE);
    assert(shell_key_translate(KEY_EX_LAYOUT,0,0)==SHELL_KEY_KBD_TOGGLE);
    assert(shell_key_translate(KEY_EXIT,0,0)==SHELL_KEY_ESC);
    assert(shell_key_translate(KEY_DEL,0,0)==SHELL_KEY_BACK);
    assert(shell_key_translate(0xff,0,0)==0);
    puts("PASS: physical EX-word QWERTY, punctuation, page and keyboard toggle");
}
