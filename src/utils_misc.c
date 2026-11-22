/* Bounded EX-word implementations of BusyBox-style file and numeric tools.
   No fork, pipes, signals, links, POSIX permission bits, or binary decoding. */
#include "utils_util.h"
#include <string.h>
#include <stdint.h>
#include <limits.h>

struct outbuf {const struct shell_api *sh;char buf[96];unsigned n;};
static int outflush(struct outbuf *o)
{
    if(!o->n)return 0;
    int rc=u_out(o->sh,o->buf,o->n);o->n=0;return rc;
}
static int outchar(struct outbuf *o,int c)
{
    if(o->n==sizeof o->buf && outflush(o))return -1;
    o->buf[o->n++]=(char)c;return 0;
}
static int outstr(struct outbuf *o,const char *s)
{
    for(;*s;s++)if(outchar(o,(unsigned char)*s))return -1;
    return 0;
}
static int outuint(struct outbuf *o,uint32_t number,unsigned base,int upper)
{
    char reverse[33];unsigned n=0;
    const char *digits=upper?"0123456789ABCDEF":"0123456789abcdef";
    do{reverse[n++]=digits[number%base];number/=base;}while(number);
    while(n)if(outchar(o,reverse[--n]))return -1;
    return 0;
}
static int outsigned(struct outbuf *o,int32_t value)
{
    if(value<0 && outchar(o,'-'))return -1;
    return outuint(o,value<0?(uint32_t)(-(int64_t)value):(uint32_t)value,10,0);
}
static int app_seq(const struct shell_api *sh)
{
    int argc=sh->argc;
    if(argc<2 || argc>4)return m_usage(sh,"usage: seq [FIRST [STEP]] LAST");
    int32_t first=1,step=1,last=0;
    if(argc==2) {
        if(u_int(sh->argv[1],&last))return m_usage(sh,"seq: invalid integer");
    }else if(argc==3) {
        if(u_int(sh->argv[1],&first)||u_int(sh->argv[2],&last))
            return m_usage(sh,"seq: invalid integer");
    }else if(u_int(sh->argv[1],&first)||u_int(sh->argv[2],&step)||
             u_int(sh->argv[3],&last))return m_usage(sh,"seq: invalid integer");
    if(!step)return m_usage(sh,"seq: step must not be zero");
    struct outbuf o={.sh=sh};
    int64_t v=first;unsigned count=0;
    for(;(step>0?v<=(int64_t)last:v>=(int64_t)last);v+=step) {
        if(++count>1000u)return m_usage(sh,"seq: limited to 1000 lines");
        if(outsigned(&o,(int32_t)v)||outchar(&o,'\n'))return 1;
        /* Int32 loop bounds guarantee the value printed fits. */
    }
    return outflush(&o)?1:0;
}
static int app_cmp(const struct shell_api *sh)
{
    int i=1,quiet=0;
    if(i<sh->argc && m_equal(sh->argv[i],"-s")){quiet=1;i++;}
    if(sh->argc-i!=2)return m_usage(sh,"usage: cmp [-s] FILE1 FILE2");
    struct u_reader a,b;
    if(u_open(&a,sh,sh->argv[i])||u_open(&b,sh,sh->argv[i+1]))
        return m_usage(sh,"cmp: cannot open file");
    uint32_t off=0;
    for(;;) {
        int x=u_get(&a),y=u_get(&b);
        if(x==-2||y==-2)return m_usage(sh,"cmp: read failed");
        if(x==-1 && y==-1)return 0;
        if(x!=y) {
            if(!quiet) {
                m_out(sh,"cmp: differ at byte ");
                m_num(sh,off+1u);m_out(sh,"\n");
            }
            return 1;
        }
        off++;
    }
}
static uint32_t crc_step(uint32_t crc,unsigned char byte)
{
    crc^=(uint32_t)byte<<24;
    for(int j=0;j<8;j++)
        crc=crc&0x80000000u?(crc<<1)^0x04c11db7u:crc<<1;
    return crc;
}
static int app_cksum(const struct shell_api *sh)
{
    if(sh->argc<2)return m_usage(sh,"usage: cksum FILE...");
    int failed=0;
    for(int i=1;i<sh->argc;i++) {
        struct u_reader r;
        if(u_open(&r,sh,sh->argv[i])) {
            m_out(sh,"cksum: cannot open ");m_line(sh,sh->argv[i]);
            failed=1;continue;
        }
        uint32_t crc=0,length=0;
        int c;
        while((c=u_get(&r))>=0) {crc=crc_step(crc,(unsigned char)c);length++;}
        if(c==-2) {failed=1;continue;}
        uint32_t n=length;
        while(n) {crc=crc_step(crc,(unsigned char)n);n>>=8;}
        crc=~crc;
        m_num(sh,crc);m_out(sh," ");
        m_num(sh,length);m_out(sh," ");m_line(sh,sh->argv[i]);
    }
    return failed;
}
static int app_stat(const struct shell_api *sh)
{
    if(sh->argc<2)return m_usage(sh,"usage: stat FILE...");
    int bad=0;
    for(int i=1;i<sh->argc;i++) {
        char path[EDL_PATH_MAX];
        if(m_path(sh,sh->argv[i],path)) {
            bad=1;continue;
        }
        int size=sh->file_size(path);
        int directory=0;
        if(size<0) {
            if(sh->list_dir(path,0,NULL,0)<0) {
                m_out(sh,"stat: missing ");m_line(sh,sh->argv[i]);bad=1;continue;
            }
            directory=1;size=0;
        }
        m_out(sh,"File: ");m_line(sh,sh->argv[i]);
        m_out(sh,"Type: ");m_line(sh,directory?"directory":"regular file");
        if(!directory) {
            m_out(sh,"Size: ");m_num(sh,(unsigned)size);m_line(sh," bytes");
        }
    }
    return bad;
}
#define WALK_MAX_DEPTH 8u
#define WALK_MAX_ENTRIES 1024u
struct walk {
    const struct shell_api *sh;
    const char *pattern;
    int type,all,failed;
    unsigned visited;
};
static unsigned du_visit(struct walk *w,const char *path,unsigned depth)
{
    int size=w->sh->file_size(path);
    if(size>=0) {
        if(w->all) {
            m_num(w->sh,(unsigned)size);m_out(w->sh," ");m_line(w->sh,path);
        }
        return (unsigned)size;
    }
    int total=w->sh->list_dir(path,0,NULL,0);
    if(total<0 || depth>=WALK_MAX_DEPTH) {w->failed=1;return 0;}
    unsigned accumulated=0;
    for(int i=0;i<total;i++) {
        if(++w->visited>WALK_MAX_ENTRIES) {w->failed=1;break;}
        struct shell_dirent entry={0};
        if(w->sh->list_dir(path,(unsigned)i,&entry,1)<0) {
            w->failed=1;break;
        }
        if(!strcmp(entry.name,".") || !strcmp(entry.name,".."))continue;
        char child[EDL_PATH_MAX];
        if(u_join(child,path,entry.name)) {w->failed=1;continue;}
        unsigned childsize=du_visit(w,child,depth+1);
        if(UINT_MAX-accumulated<childsize)accumulated=UINT_MAX;
        else accumulated+=childsize;
    }
    m_num(w->sh,accumulated);m_out(w->sh," ");m_line(w->sh,path);
    return accumulated;
}
static int app_du(const struct shell_api *sh)
{
    int i=1,all=0;
    if(i<sh->argc && m_equal(sh->argv[i],"-a")){all=1;i++;}
    if(sh->argc-i>1)return m_usage(sh,"usage: du [-a] [PATH]");
    const char *name=sh->argc==i?".":sh->argv[i];
    char path[EDL_PATH_MAX];
    if(m_path(sh,name,path))return m_usage(sh,"du: invalid path");
    struct walk w={.sh=sh,.all=all};
    int size=sh->file_size(path);
    if(size>=0) {
        m_num(sh,(unsigned)size);m_out(sh," ");m_line(sh,path);
        return 0;
    }
    if(sh->list_dir(path,0,NULL,0)<0)return m_usage(sh,"du: path not found");
    du_visit(&w,path,0);
    if(w.failed)return m_usage(sh,"du: depth/entry limit or read error");
    return 0;
}
static void find_visit(struct walk *w,const char *path,unsigned depth)
{
    if(w->failed)return;
    int size=w->sh->file_size(path);
    int directory=size<0;
    if(directory && w->sh->list_dir(path,0,NULL,0)<0) {
        w->failed=1;return;
    }
    const char *leaf=strrchr(path,'\\');
    leaf=leaf?leaf+1:path;
    if((w->type<0 || w->type==directory) &&
       (!w->pattern || u_glob(w->pattern,leaf)))m_line(w->sh,path);
    if(!directory)return;
    if(depth>=WALK_MAX_DEPTH) {w->failed=1;return;}
    int total=w->sh->list_dir(path,0,NULL,0);
    for(int i=0;i<total;i++) {
        if(++w->visited>WALK_MAX_ENTRIES){w->failed=1;return;}
        struct shell_dirent ent={0};
        if(w->sh->list_dir(path,(unsigned)i,&ent,1)<0) {
            w->failed=1;return;
        }
        if(!strcmp(ent.name,".")||!strcmp(ent.name,".."))continue;
        char child[EDL_PATH_MAX];
        if(u_join(child,path,ent.name)){w->failed=1;return;}
        find_visit(w,child,depth+1);
        if(w->failed)return;
    }
}
static int app_find(const struct shell_api *sh)
{
    int i=1,type=-1;
    const char *start=".",*pattern=NULL;
    if(i<sh->argc && sh->argv[i][0]!='-')start=sh->argv[i++];
    for(;i<sh->argc;i++) {
        if(m_equal(sh->argv[i],"-name")) {
            if(++i>=sh->argc)return m_usage(sh,"find: missing name pattern");
            pattern=sh->argv[i];
        }else if(m_equal(sh->argv[i],"-type")) {
            if(++i>=sh->argc)return m_usage(sh,"find: missing type");
            if(m_equal(sh->argv[i],"f"))type=0;
            else if(m_equal(sh->argv[i],"d"))type=1;
            else return m_usage(sh,"find: type must be f or d");
        }else if(!m_equal(sh->argv[i],"-print"))
            return m_usage(sh,"usage: find [PATH] [-name GLOB] [-type f|d]");
    }
    char path[EDL_PATH_MAX];
    if(m_path(sh,start,path))return m_usage(sh,"find: invalid path");
    struct walk w={.sh=sh,.pattern=pattern,.type=type};
    find_visit(&w,path,0);
    return w.failed?m_usage(sh,"find: depth/entry limit or read error"):0;
}
static int escaped(const char **pp)
{
    const char *p=*pp;
    if(!*p)return -1;
    int c=(unsigned char)*p++;
    if(c=='\\' && *p) {
        c=(unsigned char)*p++;
        if(c=='n')c='\n';
        else if(c=='t')c='\t';
        else if(c=='r')c='\r';
        else if(c=='0')c=0;
        else if(c=='b')c='\b';
    }
    *pp=p;return c;
}
static int app_printf(const struct shell_api *sh)
{
    if(sh->argc<2)return m_usage(sh,"usage: printf FORMAT [ARG...]");
    const char *fmt=sh->argv[1];
    struct outbuf o={.sh=sh};
    int arg=2;
    for(const char *p=fmt;*p;) {
        if(*p=='\\') {
            int c=escaped(&p);
            if(c<0||outchar(&o,c))return 1;
            continue;
        }
        if(*p!='%') {
            if(outchar(&o,(unsigned char)*p++))return 1;
            continue;
        }
        p++;
        if(*p=='%'){p++;if(outchar(&o,'%'))return 1;continue;}
        int kind=(unsigned char)*p++;
        if(!kind || !strchr("sduxXc",kind))
            return m_usage(sh,"printf: supports %s %d %u %x %X %c and %%");
        const char *a=arg<sh->argc?sh->argv[arg++]:"";
        int32_t signed_number;
        unsigned unsigned_number;
        switch(kind) {
            case 's':if(outstr(&o,a))return 1;break;
            case 'c':if(outchar(&o,*a?*a:0))return 1;break;
            case 'd':
                if(u_int(a,&signed_number))return m_usage(sh,"printf: invalid decimal");
                if(outsigned(&o,signed_number))return 1;
                break;
            case 'u':case 'x':case 'X':
                if(u_uint(a,&unsigned_number))return m_usage(sh,"printf: invalid unsigned");
                if(outuint(&o,unsigned_number,kind=='u'?10u:16u,kind=='X'))return 1;
                break;
        }
    }
    return outflush(&o)?1:0;
}
int utils_misc_main(const struct shell_api *sh)
{
    if(m_equal(sh->command,"seq"))return app_seq(sh);
    if(m_equal(sh->command,"cmp"))return app_cmp(sh);
    if(m_equal(sh->command,"cksum"))return app_cksum(sh);
    if(m_equal(sh->command,"stat"))return app_stat(sh);
    if(m_equal(sh->command,"du"))return app_du(sh);
    if(m_equal(sh->command,"find"))return app_find(sh);
    if(m_equal(sh->command,"printf"))return app_printf(sh);
    return 127;
}
