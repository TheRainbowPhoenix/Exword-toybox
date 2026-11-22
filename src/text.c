#include "module_util.h"
#include "shell_posix.h"
#include "shell_format.h"
__attribute__((used,section(".edl.cmd"),aligned(4)))
const struct shell_manifest shell_commands={
    .magic=SHELL_MANIFEST_MAGIC,.version=SHELL_MANIFEST_VERSION,.count=10,
    .names={"wc","head","grep","basename","dirname","uniq","sort","tail","more","hexdump"}
};
static int wc(const struct shell_api *sh)
{
    if (sh->argc!=2) return m_usage(sh,"usage: wc FILE");
    char path[EDL_PATH_MAX];
    if (m_path(sh,sh->argv[1],path)) return 2;
    int size=sh->file_size(path);
    if (size<0) return m_usage(sh,"wc: file not found");
    unsigned lines=0,words=0,inword=0;
    char buf[256];
    for (unsigned off=0;off<(unsigned)size;) {
        unsigned n=(unsigned)size-off;
        if (n>sizeof buf) n=sizeof buf;
        if (sh->edl.read(path,off,buf,n)!=(int)n) return 1;
        for (unsigned i=0;i<n;i++) {
            char c=buf[i];
            if (c=='\n') lines++;
            int blank=c==' '||c=='\n'||c=='\t'||c=='\r';
            if (!blank && !inword) words++;
            inword=!blank;
        }
        off+=n;
    }
    m_num(sh,lines);m_out(sh," ");m_num(sh,words);m_out(sh," ");
    m_num(sh,(unsigned)size);m_out(sh," ");m_line(sh,sh->argv[1]);
    return 0;
}
static int head(const struct shell_api *sh)
{
    if (sh->argc!=2) return m_usage(sh,"usage: head FILE");
    char path[EDL_PATH_MAX];
    if (m_path(sh,sh->argv[1],path)) return 2;
    int size=sh->file_size(path);
    if (size<0) return 1;
    unsigned lines=0;
    char buf[256];
    for (unsigned off=0;off<(unsigned)size && lines<10;) {
        unsigned n=(unsigned)size-off;
        if (n>sizeof buf) n=sizeof buf;
        if (sh->edl.read(path,off,buf,n)!=(int)n) return 1;
        unsigned written=0;
        for (unsigned i=0;i<n;i++) {
            written=i+1;
            if (buf[i]=='\n' && ++lines>=10) break;
        }
        if (written) sh->console_write(buf,written);
        off+=n;
    }
    return 0;
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
