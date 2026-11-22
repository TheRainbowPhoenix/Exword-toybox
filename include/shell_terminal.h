#ifndef TOYBOX_SHELL_TERMINAL_H
#define TOYBOX_SHELL_TERMINAL_H
/* Bounded terminal/ANSI emulator, kept in resident D01 memory.
   No dynamic allocation and no operating-system streams. */
#define SHELL_TERM_COLS 64u
#define SHELL_TERM_HISTORY 32u
void shell_term_init(void);
void shell_term_feed(const void *data,unsigned size);
void shell_term_put(int ch);
void shell_term_page(int pages,unsigned height);
const char *shell_term_line(unsigned visible_row,unsigned height);
unsigned shell_term_back(void);
#endif
