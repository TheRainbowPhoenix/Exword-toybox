#ifndef TOYBOX_SHELL_PARSE_H
#define TOYBOX_SHELL_PARSE_H
#include "shell_format.h"
/* Shell grammar v1: spaces, single/double quotes, backslash escapes.
   Tokenizes in-place, returning argc; -1 indicates syntax / size error. */
int shell_parse(char *line, const char *argv[SHELL_MAX_ARGS]);
#endif
