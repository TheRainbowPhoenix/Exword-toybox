#ifndef TOYBOX_SHELL_HISTORY_H
#define TOYBOX_SHELL_HISTORY_H
#include "shell_format.h"

/* Fixed-size bounded history (no allocator or native filesystem writes).
   Commands are kept verbatim, before shell_parse() changes the input buffer. */
#define SHELL_HISTORY_DEPTH 8u
struct shell_history {
    char entries[SHELL_HISTORY_DEPTH][SHELL_LINE_SIZE];
    char draft[SHELL_LINE_SIZE];
    unsigned start, count, cursor;
    int browsing;
};
void shell_history_init(struct shell_history *h);
void shell_history_push(struct shell_history *h, const char *line);
const char *shell_history_up(struct shell_history *h, const char *draft);
const char *shell_history_down(struct shell_history *h);
unsigned shell_history_count(const struct shell_history *h);
const char *shell_history_get(const struct shell_history *h, unsigned index);
#endif
