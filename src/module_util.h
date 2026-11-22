#ifndef TOYBOX_MODULE_UTIL_H
#define TOYBOX_MODULE_UTIL_H
#include "shell_api.h"
#include <string.h>
static inline int m_equal(const char *a,const char *b)
{ while(*a && *b && *a==*b) {a++;b++;} return !*a && !*b; }
static inline void m_out(const struct shell_api *sh,const char *s)
{ sh->console_write(s,(unsigned)strlen(s)); }
static inline void m_line(const struct shell_api *sh,const char *s)
{ m_out(sh,s);m_out(sh,"\n"); }
static inline void m_num(const struct shell_api *sh,unsigned n)
{
    char digits[12];unsigned k=0;
    do {digits[k++]=(char)('0'+n%10);n/=10;}while(n);
    while(k) sh->console_write(&digits[--k],1);
}
static inline int m_path(const struct shell_api *sh,const char *src,char dst[EDL_PATH_MAX])
{ return sh->resolve_path(src,dst); }
static inline int m_usage(const struct shell_api *sh,const char *msg)
{ m_line(sh,msg);return 2; }
#endif
