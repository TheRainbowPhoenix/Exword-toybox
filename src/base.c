/* BusyBox-style multicall applet bundle, adapted to EX-word's native filesystem.
   Initial commands are clean-room implementations, NOT the upstream BusyBox C
   functions. They intentionally expose only operations the EDL ABI supports. */
#include "module_util.h"
#include "shell_format.h"

__attribute__((used,section(".edl.cmd"),aligned(4)))
const struct shell_manifest shell_commands = {
    .magic=SHELL_MANIFEST_MAGIC,.version=SHELL_MANIFEST_VERSION,.count=10,
    .names={"ls","cat","echo","cp","mv","rm","touch","mkdir","date","true"}
};

static int app_ls(const struct shell_api *sh)
{
    if (sh->argc>2) return m_usage(sh,"usage: ls [directory]");
    char path[EDL_PATH_MAX];
    if (m_path(sh,sh->argc>1?sh->argv[1]:".",path)) return m_usage(sh,"ls: invalid path");
    struct shell_dirent entries[12];
    int total=sh->list_dir(path,0,entries,12);
    if (total<0) return m_usage(sh,"ls: unable to list directory");
    for (unsigned first=0;first<(unsigned)total;first+=12) {
        if (sh->list_dir(path,first,entries,12)<0) return 1;
        for (unsigned i=0;i<12 && first+i<(unsigned)total;i++) {
            m_out(sh,entries[i].name);
            if (entries[i].directory) m_out(sh,"/");
            m_out(sh,"\n");
        }
    }
    return 0;
}
static int app_cat(const struct shell_api *sh)
{
    if (sh->argc<2) return m_usage(sh,"usage: cat FILE...");
    unsigned char buf[256];
    for (int i=1;i<sh->argc;i++) {
        char path[EDL_PATH_MAX];
        if (m_path(sh,sh->argv[i],path)) return 1;
        int length=sh->file_size(path);
        if (length<0) return m_usage(sh,"cat: cannot open file");
        for (unsigned off=0;off<(unsigned)length;) {
            unsigned n=(unsigned)length-off;
            if (n>sizeof buf) n=sizeof buf;
            if (sh->edl.read(path,off,buf,n)!=(int)n || sh->console_write(buf,n)<0) return 1;
            off+=n;
        }
    }
    return 0;
}
static int app_echo(const struct shell_api *sh)
{
    int i=1, newline=1;
    if (i<sh->argc && m_equal(sh->argv[i],"-n")) { newline=0;i++; }
    for (;i<sh->argc;i++) {
        if (i>1 && !(i==2 && !newline)) m_out(sh," ");
        m_out(sh,sh->argv[i]);
    }
    if (newline) m_out(sh,"\n");
    return 0;
}
static int app_cp(const struct shell_api *sh)
{
    if (sh->argc!=3) return m_usage(sh,"usage: cp SOURCE DEST");
    char src[EDL_PATH_MAX],dst[EDL_PATH_MAX];
    if (m_path(sh,sh->argv[1],src) || m_path(sh,sh->argv[2],dst)) return 2;
    if (m_equal(src,dst)) return m_usage(sh,"cp: source and destination identical");
    int size=sh->file_size(src);
    if (size<0) return m_usage(sh,"cp: cannot read source");
    /* A bounded static buffer avoids consuming the resident D01's heap. */
    unsigned char buf[1024];
    if (!size && sh->file_write(dst,0,buf,0,SHELL_WRITE_CREATE|SHELL_WRITE_TRUNC)<0)
        return m_usage(sh,"cp: cannot create destination");
    for (unsigned off=0;off<(unsigned)size;) {
        unsigned n=(unsigned)size-off;
        if (n>sizeof buf) n=sizeof buf;
        if (sh->edl.read(src,off,buf,n)!=(int)n) return m_usage(sh,"cp: read failed");
        unsigned flags=off?0:SHELL_WRITE_CREATE|SHELL_WRITE_TRUNC;
        if (sh->file_write(dst,off,buf,n,flags)!=(int)n) return m_usage(sh,"cp: write failed");
        off+=n;
    }
    return 0;
}
static int app_mv(const struct shell_api *sh)
{
    if (sh->argc!=3) return m_usage(sh,"usage: mv SOURCE DEST");
    char from[EDL_PATH_MAX],to[EDL_PATH_MAX];
    if (m_path(sh,sh->argv[1],from) || m_path(sh,sh->argv[2],to)) return 2;
    if (m_equal(from,to)) return 0;
    return sh->file_rename(from,to)<0?m_usage(sh,"mv: rename failed"):0;
}
static int app_rm(const struct shell_api *sh)
{
    if (sh->argc<2) return m_usage(sh,"usage: rm FILE...");
    int error=0;
    for (int i=1;i<sh->argc;i++) {
        char path[EDL_PATH_MAX];
        if (m_path(sh,sh->argv[i],path) || sh->file_delete(path)<0) {
            m_line(sh,"rm: delete failed");error=1;
        }
    }
    return error;
}
static int app_touch(const struct shell_api *sh)
{
    if (sh->argc<2) return m_usage(sh,"usage: touch FILE...");
    int error=0;
    for (int i=1;i<sh->argc;i++) {
        char path[EDL_PATH_MAX];
        if (m_path(sh,sh->argv[i],path)) return 2;
        if (sh->file_size(path)<0 && sh->file_create(path,0)<0) {
            m_line(sh,"touch: cannot create file");error=1;
        }
    }
    return error;
}
static int app_mkdir(const struct shell_api *sh)
{
    if (sh->argc!=2) return m_usage(sh,"usage: mkdir DIRECTORY");
    char path[EDL_PATH_MAX];
    if (m_path(sh,sh->argv[1],path)) return 2;
    return sh->file_create(path,1)<0?m_usage(sh,"mkdir: creation failed"):0;
}
static int app_date(const struct shell_api *sh)
{
    struct edl_status now;
    if (!sh->edl.status || sh->edl.status(&now)) return 1;
    m_num(sh,now.year);m_out(sh,"-");
    if (now.month<10) m_out(sh,"0");
    m_num(sh,now.month);m_out(sh,"-");
    if (now.day<10) m_out(sh,"0");
    m_num(sh,now.day);m_out(sh," ");
    if (now.hours<10) m_out(sh,"0");
    m_num(sh,now.hours);m_out(sh,":");
    if (now.minutes<10) m_out(sh,"0");
    m_num(sh,now.minutes);m_out(sh,":");
    if (now.seconds<10) m_out(sh,"0");
    m_num(sh,now.seconds);m_out(sh,"\n");
    return 0;
}
int edl_main(const struct edl_api *edl)
{
    const struct shell_api *sh=shell_from_edl(edl);
    if (!sh || !sh->command || sh->argc<=0) return -1;
    if (m_equal(sh->command,"ls")) return app_ls(sh);
    if (m_equal(sh->command,"cat")) return app_cat(sh);
    if (m_equal(sh->command,"echo")) return app_echo(sh);
    if (m_equal(sh->command,"cp")) return app_cp(sh);
    if (m_equal(sh->command,"mv")) return app_mv(sh);
    if (m_equal(sh->command,"rm")) return app_rm(sh);
    if (m_equal(sh->command,"touch")) return app_touch(sh);
    if (m_equal(sh->command,"mkdir")) return app_mkdir(sh);
    if (m_equal(sh->command,"date")) return app_date(sh);
    if (m_equal(sh->command,"true")) return 0;
    return 127;
}
