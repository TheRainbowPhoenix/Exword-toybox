#ifndef TOYBOX_SHELL_HISTORY_H
#define TOYBOX_SHELL_HISTORY_H
#include "shell_format.h"
#include <stdint.h>

/* Bounded command pool: up to 8 full-length entries, with oldest entries
   evicted if their combined length exceeds 512 bytes. Avoids growing D01 BSS
   by 8 * SHELL_LINE_SIZE on a tight 4 KiB heap budget. */
#define SHELL_HISTORY_DEPTH 8u
#define SHELL_HISTORY_BYTES 512u
struct shell_history {
    char pool[SHELL_HISTORY_BYTES];
    char draft[SHELL_LINE_SIZE];
    uint16_t at[SHELL_HISTORY_DEPTH];
    unsigned used, count, cursor;
    int browsing;
};
void shell_history_init(struct shell_history *h);
void shell_history_push(struct shell_history *h, const char *line);
const char *shell_history_up(struct shell_history *h, const char *draft);
const char *shell_history_down(struct shell_history *h);
unsigned shell_history_count(const struct shell_history *h);
const char *shell_history_get(const struct shell_history *h, unsigned index);
#endif
