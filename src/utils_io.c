#include "utils_util.h"
#include <string.h>
#include <stdint.h>
#include <limits.h>

int u_open(struct u_reader *r,const struct shell_api *sh,const char *name)
{
    if(!r || !sh || !name || m_path(sh,name,r->path))return -1;
    int size=sh->file_size(r->path);
    if(size<0)return -1;
    r->sh=sh; r->size=(uint32_t)size;
    r->offset=0;r->pos=r->used=0;
    return 0;
}
int u_get(struct u_reader *r)
{
    if(r->pos==r->used) {
        if(r->offset>=r->size)return -1;
        unsigned n=r->size-r->offset;
        if(n>sizeof r->buf)n=sizeof r->buf;
        if(r->sh->edl.read(r->path,r->offset,r->buf,n)!=(int)n)return -2;
        r->offset+=n;r->pos=0;r->used=n;
    }
    return r->buf[r->pos++];
}
int u_line(struct u_reader *r,char dst[U_LINE])
{
    unsigned n=0;
    for(;;) {
        int c=u_get(r);
        if(c==-2)return -1;
        if(c==-1)break;
        if(!c || n+1>=U_LINE)return -1; /* reject binary/oversized lines */
        dst[n++]=(char)c;
        if(c=='\n')break;
    }
    dst[n]=0;
    return n?1:0;
}
int u_out(const struct shell_api *sh,const void *buf,unsigned n)
{
    return sh->console_write(buf,n)==(int)n?0:-1;
}
int u_char(const struct shell_api *sh,char c)
{ return u_out(sh,&c,1); }
int u_uint(const char *p,unsigned *n)
{
    uint32_t v=0;
    if(!p || !*p || !n)return -1;
    for(;*p;p++) {
        if(*p<'0'||*p>'9')return -1;
        unsigned digit=(unsigned)(*p-'0');
        if(v>(UINT32_MAX-digit)/10u)return -1;
        v=10u*v+digit;
    }
    *n=v;return 0;
}
int u_int(const char *s,int32_t *out)
{
    if(!s || !*s || !out)return -1;
    int negative=(*s=='-');
    if(*s=='-'||*s=='+')s++;
    unsigned n;
    if(u_uint(s,&n) || n>(negative?2147483648u:2147483647u))
        return -1;
    if(negative) *out=(int32_t)(-(int64_t)n);
    else *out=(int32_t)n;
    return 0;
}
int u_signed(const struct shell_api *sh,int32_t n)
{
    if(n<0) {
        if(u_char(sh,'-'))return -1;
        m_num(sh,(unsigned)(-(int64_t)n));
    } else m_num(sh,(unsigned)n);
    return 0;
}
int u_join(char out[EDL_PATH_MAX],const char *dir,const char *name)
{
    size_t n=strlen(dir),k=strlen(name);
    if(n+k+2>EDL_PATH_MAX || strchr(name,'\\') || strchr(name,'/'))return -1;
    memcpy(out,dir,n);
    out[n++]='\\';
    memcpy(out+n,name,k+1);
    return 0;
}
int u_glob(const char *p,const char *s)
{
    /* Glob only supports * and ?, not POSIX bracket expressions. */
    while(*p) {
        if(*p=='*') {
            while(p[1]=='*')p++;
            p++;
            if(!*p)return 1;
            for(;;) {
                if(u_glob(p,s))return 1;
                if(!*s)break;
                s++;
            }
            return 0;
        }
        if(!*s || (*p!='?' && *p!=*s))return 0;
        p++;s++;
    }
    return !*s;
}
