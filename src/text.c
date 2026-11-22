#include "module_util.h"
#include "shell_posix.h"
#include "shell_format.h"
#include <limits.h>
__attribute__((used,section(".edl.cmd"),aligned(4)))
const struct shell_manifest shell_commands={
    .magic=SHELL_MANIFEST_MAGIC,.version=SHELL_MANIFEST_VERSION,.count=10,
    .names={"wc","head","grep","basename","dirname","uniq","sort","tail","more","hexdump"}
};

static int parse_unsigned(const char *s,unsigned *value)
{
    unsigned n=0;
    if(!s || !*s) return -1;
    for(;*s;s++) {
        if(*s<'0' || *s>'9') return -1;
        unsigned d=(unsigned)(*s-'0');
        if(n>(UINT_MAX-d)/10) return -1;
        n=10*n+d;
    }
    *value=n;
    return 0;
}
struct count_result {
    unsigned lines,words,bytes;
};
static int count_file(const struct shell_api *sh,const char *name,
                      struct count_result *result)
{
    char path[EDL_PATH_MAX];
    if(m_path(sh,name,path)) return -1;
    int size=sh->file_size(path);
    if(size<0) return -1;
    unsigned lines=0,words=0,inword=0;
    unsigned char buf[256];
    for(unsigned off=0;off<(unsigned)size;) {
        unsigned n=(unsigned)size-off;
        if(n>sizeof buf)n=sizeof buf;
        if(sh->edl.read(path,off,buf,n)!=(int)n) return -1;
        for(unsigned i=0;i<n;i++) {
            unsigned char c=buf[i];
            if(c=='\n')lines++;
            int blank=(c==' ' || c=='\n' || c=='\r' || c=='\t' ||
                       c=='\f' || c=='\v');
            if(!blank && !inword)words++;
            inword=!blank;
        }
        off+=n;
    }
    *result=(struct count_result){lines,words,(unsigned)size};
    return 0;
}
static void print_counts(const struct shell_api *sh,struct count_result r,
                         unsigned flags,const char *name)
{
    int printed=0;
    if(flags&1u) {m_num(sh,r.lines);printed=1;}
    if(flags&2u) {if(printed)m_out(sh," ");m_num(sh,r.words);printed=1;}
    if(flags&4u) {if(printed)m_out(sh," ");m_num(sh,r.bytes);}
    m_out(sh," ");m_line(sh,name);
}
static int wc(const struct shell_api *sh)
{
    unsigned flags=0; int i=1;
    for(;i<sh->argc;i++) {
        const char *arg=sh->argv[i];
        if(m_equal(arg,"--")) {i++;break;}
        if(arg[0]!='-' || !arg[1])break;
        for(const char *p=arg+1;*p;p++) {
            if(*p=='l')flags|=1u;
            else if(*p=='w')flags|=2u;
            else if(*p=='c' || *p=='m')flags|=4u;
            else return m_usage(sh,"usage: wc [-lwc] FILE...");
        }
    }
    if(!flags)flags=7;
    if(i>=sh->argc) return m_usage(sh,"usage: wc [-lwc] FILE...");
    struct count_result total={0,0,0};
    int failures=0,success=0;
    int files=sh->argc-i;
    for(;i<sh->argc;i++) {
        struct count_result r;
        if(count_file(sh,sh->argv[i],&r)<0) {
            m_out(sh,"wc: cannot read ");m_line(sh,sh->argv[i]);failures=1;
            continue;
        }
        print_counts(sh,r,flags,sh->argv[i]);success++;
        total.lines+=r.lines;total.words+=r.words;total.bytes+=r.bytes;
    }
    if(files>1 && success)print_counts(sh,total,flags,"total");
    return failures ? 1 : 0;
}
static int head_file(const struct shell_api *sh,const char *name,
                     unsigned limit,int bytes)
{
    char path[EDL_PATH_MAX];
    if(m_path(sh,name,path))return -1;
    int size=sh->file_size(path);
    if(size<0)return -1;
    unsigned lines=0,remaining=limit;
    unsigned char buf[256];
    if(!limit)return 0;
    for(unsigned off=0;off<(unsigned)size;) {
        unsigned n=(unsigned)size-off;
        if(n>sizeof buf)n=sizeof buf;
        if(bytes && n>remaining)n=remaining;
        if(!n)break;
        if(sh->edl.read(path,off,buf,n)!=(int)n) return -1;
        unsigned emit=n;
        if(!bytes) {
            for(unsigned j=0;j<n;j++) {
                if(buf[j]=='\n' && ++lines>=limit) {emit=j+1;break;}
            }
        }
        if(emit && sh->console_write(buf,emit)!=(int)emit)return -1;
        off+=emit;
        if(bytes)remaining-=emit;
        if((bytes && !remaining) || (!bytes && lines>=limit))break;
    }
    return 0;
}
static int head(const struct shell_api *sh)
{
    int i=1,bytes=0;
    unsigned limit=10;
    while(i<sh->argc) {
        const char *arg=sh->argv[i];
        if(m_equal(arg,"--")) {i++;break;}
        if(arg[0]!='-' || !arg[1])break;
        if(arg[1]=='n' || arg[1]=='c') {
            bytes=arg[1]=='c';
            const char *n=arg+2;
            if(!*n) {
                if(++i>=sh->argc)return m_usage(sh,"head: missing count");
                n=sh->argv[i];
            }
            if(parse_unsigned(n,&limit))return m_usage(sh,"head: invalid count");
        } else if(parse_unsigned(arg+1,&limit)==0) {
            bytes=0; /* historical head -2 shorthand */
        } else return m_usage(sh,"usage: head [-n N|-c N] FILE...");
        i++;
    }
    if(i>=sh->argc)return m_usage(sh,"usage: head [-n N|-c N] FILE...");
    int files=sh->argc-i,err=0;
    for(;i<sh->argc;i++) {
        if(files>1) {
            if(i>sh->argc-files)m_out(sh,"\n");
            m_out(sh,"==> ");m_out(sh,sh->argv[i]);m_line(sh," <==");
        }
        if(head_file(sh,sh->argv[i],limit,bytes)) {
            m_out(sh,"head: cannot read ");m_line(sh,sh->argv[i]);err=1;
        }
    }
    return err;
}
static int grep(const struct shell_api *sh)
{
    if (sh->argc!=3) return m_usage(sh,"usage: grep PATTERN FILE");
    const char *needle=sh->argv[1];
    unsigned pattern=(unsigned)strlen(needle);
    if (!pattern) return 2;
    char path[EDL_PATH_MAX];
    if (m_path(sh,sh->argv[2],path)) return 2;
    int size=sh->file_size(path);
    if (size<0) return 2;
    char line[256];unsigned used=0,found=0;
    char buf[128];
    for (unsigned off=0;off<=(unsigned)size;) {
        unsigned n=(unsigned)size-off;
        if (n>sizeof buf) n=sizeof buf;
        if (n && sh->edl.read(path,off,buf,n)!=(int)n) return 2;
        for (unsigned i=0;i<=n;i++) {
            if (i==n && n) break;
            char ch=n?buf[i]:'\n';
            if (ch=='\n' || used>=sizeof line-1) {
                line[used]=0;
                for (unsigned j=0;j+pattern<=used;j++) {
                    if (!memcmp(line+j,needle,pattern)) {
                        sh->console_write(line,used);m_out(sh,"\n");found=1;break;
                    }
                }
                used=0;
            } else line[used++]=ch;
        }
        off+=n;
        if (!n) break;
    }
    return found?0:1;
}
int bb_text_main(const struct shell_api *sh);
int posix_tool_main(const struct shell_api *sh);
int edl_main(const struct edl_api *edl)
{
    const struct shell_api *sh=shell_from_edl(edl);
    if (!sh) return -1;
    sp_bind(sh);
    if (m_equal(sh->command,"wc")) return wc(sh);
    if (m_equal(sh->command,"head")) return head(sh);
    if (m_equal(sh->command,"grep")) return grep(sh);
    if (m_equal(sh->command,"more") || m_equal(sh->command,"hexdump"))
        return posix_tool_main(sh);
    return bb_text_main(sh);
}
