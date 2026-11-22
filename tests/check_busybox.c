#include "shell_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int bb_text_main(const struct shell_api *);

static struct shell_api shell;
static const char source[] = "zeta\nalpha\nalpha\nbeta\n";
static char result[512];
static unsigned result_size;
static char screen[2048];
static unsigned screen_size;
static int resolve(const char *name, char out[EDL_PATH_MAX])
{
    int rc=snprintf(out, EDL_PATH_MAX, "\\\\drv0\\%s", name);
    return rc<0 || rc>=EDL_PATH_MAX ? -1 : 0;
}
static int size(const char *path)
{
    if (!strcmp(path,"\\\\drv0\\sorted.txt")) return sizeof source-1;
    if (!strcmp(path,"\\\\drv0\\result.txt")) return (int)result_size;
    return -1;
}
static int read_file(const char *path, uint32_t off, void *dst, uint32_t n)
{
    const char *p=NULL;unsigned length=0;
    if (!strcmp(path,"\\\\drv0\\sorted.txt")) {
        p=source;length=sizeof source-1;
    } else if (!strcmp(path,"\\\\drv0\\result.txt")) {
        p=result;length=result_size;
    }
    if (!p || off>length || n>length-off) return -1;
    memcpy(dst,p+off,n);
    return (int)n;
}
static int write_file(const char *path, unsigned off, const void *bytes,
                      unsigned n, unsigned flags)
{
    if (strcmp(path,"\\\\drv0\\result.txt") || off+n>sizeof result) return -1;
    if (flags&SHELL_WRITE_TRUNC) result_size=0;
    if (off>result_size) return -1;
    if (n) memcpy(result+off,bytes,n);
    if (off+n>result_size) result_size=off+n;
    return (int)n;
}
static int output(const void *bytes, unsigned n)
{
    assert(screen_size+n<sizeof screen);
    memcpy(screen+screen_size,bytes,n);
    screen_size+=n;screen[screen_size]=0;
    return (int)n;
}
static int run(const char *name,int argc,const char *const *argv)
{
    screen_size=0;screen[0]=0;
    shell.command=name;shell.argc=argc;shell.argv=argv;
    return bb_text_main(&shell);
}
int main(void)
{
    memset(&shell,0,sizeof shell);
    shell.console_write=output;
    shell.resolve_path=resolve;
    shell.file_size=size;
    shell.file_write=write_file;
    shell.edl.read=read_file;

    const char *base[]={"basename","dir/file.txt",".txt"};
    assert(run("basename",3,base)==0 && !strcmp(screen,"file\n"));
    const char *dir[]={"dirname","dir/file.txt"};
    assert(run("dirname",2,dir)==0 && !strcmp(screen,"dir\n"));
    const char *uniq[]={"uniq","sorted.txt"};
    assert(run("uniq",2,uniq)==0 && !strcmp(screen,"zeta\nalpha\nbeta\n"));
    const char *count[]={"uniq","-c","sorted.txt"};
    assert(run("uniq",3,count)==0 && !strcmp(screen,"1 zeta\n2 alpha\n1 beta\n"));
    const char *sorted[]={"sort","-u","sorted.txt"};
    assert(run("sort",3,sorted)==0 && !strcmp(screen,"alpha\nbeta\nzeta\n"));
    const char *outfile[]={"sort","-ru","-o","result.txt","sorted.txt"};
    assert(run("sort",5,outfile)==0);
    assert(result_size==16 && !memcmp(result,"zeta\nbeta\nalpha\n",16));
    const char *tail[]={"tail","-n","2","sorted.txt"};
    assert(run("tail",4,tail)==0 && !strcmp(screen,"alpha\nbeta\n"));
    const char *bytes[]={"tail","-c","5","sorted.txt"};
    assert(run("tail",4,bytes)==0 && !strcmp(screen,"beta\n"));
    const char *same[]={"uniq","sorted.txt","sorted.txt"};
    assert(run("uniq",3,same)!=0);
    puts("PASS: BusyBox-derived basename/dirname/uniq/sort/tail applets");
}
