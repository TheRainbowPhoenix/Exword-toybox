#include "shell_names.h"
#include <stddef.h>

static int letter(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int acceptable(unsigned char c)
{
    return letter(c) || (c >= '0' && c <= '9') || c == '_' ||
           c == '-' || c == '$' || c == '~';
}
int shell_name_valid(const char *name, int directory)
{
    unsigned stem = 0, ext = 0;
    int dot = 0;
    if (!name || !*name) return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (*p == '.' && !directory && !dot && stem) { dot = 1; continue; }
        if (!acceptable(*p)) return 0;
        if (dot) {
            if (++ext > SHELL_FILE_EXT_MAX) return 0;
        } else if (++stem > (directory ? SHELL_DIR_NAME_MAX : SHELL_FILE_STEM_MAX))
            return 0;
    }
    return stem && (!dot || ext);
}
static int is_volume(const char *p, size_t n)
{
    return n == 4 &&
           ((p[0]=='d' && p[1]=='r' && p[2]=='v' && p[3]=='0') ||
            (p[0]=='c' && p[1]=='r' && p[2]=='d' && (p[3]=='0' || p[3]=='1')));
}
int shell_path_valid(const char *path, int directory)
{
    if (!path || path[0] != '\\' || path[1] != '\\') return 0;
    const char *p = path + 2, *q = p;
    while (*q && *q != '\\') q++;
    if (!is_volume(p, (size_t)(q-p)) || !*q || !q[1]) return 0;
    p = q + 1;
    for (;;) {
        char component[SHELL_DIR_NAME_MAX + 1];
        size_t n = 0;
        while (p[n] && p[n] != '\\') {
            if (n >= SHELL_DIR_NAME_MAX) return 0;
            component[n] = p[n];
            n++;
        }
        if (!n) return 0;
        component[n] = 0;
        if (!shell_name_valid(component, p[n] ? 1 : directory)) return 0;
        if (!p[n]) return 1;
        p += n+1;
        if (!*p) return 0;
    }
}
