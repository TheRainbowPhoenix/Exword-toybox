#ifndef TOYBOX_UTILS_UTIL_H
#define TOYBOX_UTILS_UTIL_H
#include "module_util.h"
#include <stdint.h>
#include <limits.h>
#define U_LINE 256u
struct u_reader {
    const struct shell_api *sh;
    char path[EDL_PATH_MAX];
    uint32_t size, offset;
    unsigned pos,used;
    unsigned char buf[128];
};
int u_open(struct u_reader *r,const struct shell_api *sh,const char *path);
int u_get(struct u_reader *r); /* byte 0..255; -1 EOF, -2 native I/O failure */
int u_line(struct u_reader *r,char dst[U_LINE]); /* 1 line, 0 EOF, -1 error/too long */
int u_out(const struct shell_api *sh,const void *buf,unsigned n);
int u_char(const struct shell_api *sh,char c);
int u_uint(const char *s,unsigned *n);
int u_int(const char *s,int32_t *n);
int u_signed(const struct shell_api *sh,int32_t n);
int u_join(char out[EDL_PATH_MAX],const char *dir,const char *name);
int u_glob(const char *pattern,const char *name);
/* Edelweiss's freestanding EDL runtime does not export libc's str* routines.
   Redirect these only inside the optional utility pack; no host ABI impact. */
int u_strcmp(const char *a,const char *b);
int u_strncmp(const char *a,const char *b,unsigned n);
const char *u_strchr(const char *s,int c);
const char *u_strrchr(const char *s,int c);
#define strcmp  u_strcmp
#define strncmp u_strncmp
#define strchr  u_strchr
#define strrchr u_strrchr

#endif
