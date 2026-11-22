#include "shell_posix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct shell_api shell;
static char data[200],screen[256];
static unsigned length,used,created;
static int resolve(const char *name,char out[EDL_PATH_MAX])
{
    int n=snprintf(out,EDL_PATH_MAX,"\\\\drv0\\%s",name);
    return n<0 || n>=EDL_PATH_MAX ? -1:0;
}
static int size(const char *path)
{
    return !strcmp(path,"\\\\drv0\\file.txt") && created?(int)length:-1;
}
static int create(const char *path,int dir)
{
    if(strcmp(path,"\\\\drv0\\file.txt")||dir)return -1;
    created=1;length=0;return 0;
}
static int write_file(const char *path,unsigned off,const void *src,unsigned n,unsigned flags)
{
    if(strcmp(path,"\\\\drv0\\file.txt") || off+n>sizeof data)return -1;
    if(flags&SHELL_WRITE_TRUNC)length=0;
    if(off>length)return -1;
    if(n)memcpy(data+off,src,n);
    if(off+n>length)length=off+n;
    return (int)n;
}
static int read_file(const char *path,uint32_t off,void *dst,uint32_t n)
{
    if(strcmp(path,"\\\\drv0\\file.txt") || off+n>length)return -1;
    memcpy(dst,data+off,n);return (int)n;
}
static int list(const char *p,unsigned first,struct shell_dirent *d,unsigned n)
{(void)p;(void)first;(void)d;(void)n;return -1;}
static int console(const void *p,unsigned n)
{
    assert(used+n<sizeof screen);
    memcpy(screen+used,p,n);used+=n;screen[used]=0;
    return (int)n;
}
static const int keys[]={'a','b',SHELL_KEY_ENTER,'x',SHELL_KEY_UP,SHELL_KEY_ESC};
static unsigned idx;
static int read_key(void) {assert(idx<sizeof keys/sizeof keys[0]);return keys[idx++];}
static int tty_size(unsigned *c,unsigned *r){*c=64;*r=24;return 0;}
int main(void)
{
    shell.edl.read=read_file;
    shell.resolve_path=resolve;shell.file_size=size;shell.file_create=create;
    shell.file_write=write_file;shell.list_dir=list;shell.console_write=console;
    shell.read_key=read_key;shell.tty_size=tty_size;shell.cwd="\\\\drv0";
    sp_bind(&shell);
    assert(sp_open("missing.txt",SP_O_RDONLY)==-1);
    int fd=sp_open("file.txt",SP_O_CREAT|SP_O_WRONLY);
    assert(fd==3);
    assert(sp_write(fd,"test",4)==4);
    assert(sp_close(fd)==0);
    struct sp_stat st;
    assert(sp_stat("file.txt",&st)==0&&st.size==4);
    fd=sp_open("file.txt",SP_O_RDONLY);
    char b[16]={0};
    assert(sp_read(fd,b,7)==4&&!strcmp(b,"test"));
    assert(sp_read(fd,b,7)==0);
    assert(sp_lseek(fd,-2,2)==2);
    assert(sp_read(fd,b,2)==2 && !memcmp(b,"st",2));
    assert(sp_close(fd)==0);
    fd=sp_open("file.txt",SP_O_APPEND|SP_O_WRONLY);
    assert(sp_write(fd,"!",1)==1&&length==5);
    assert(sp_close(fd)==0);
    fd=sp_open("file.txt",SP_O_TRUNC|SP_O_WRONLY);
    assert(fd==3&&length==0);
    assert(sp_close(fd)==0);
    assert(sp_write(1,"out",3)==3&&!strcmp(screen,"out"));
    assert(sp_isatty(0)&&sp_isatty(1)&&!sp_isatty(3));
    struct sp_winsize ws;
    assert(sp_winsize(1,&ws)==0&&ws.cols==64&&ws.rows==24);
    struct sp_termios t;
    assert(sp_tcgetattr(0,&t)==0&&(t.lflag&SP_ICANON));
    assert(sp_read(0,b,6)==3&&!memcmp(b,"ab\n",3));
    t.lflag=0;assert(sp_tcsetattr(0,&t)==0);
    assert(sp_read(0,b,1)==1&&b[0]=='x');
    assert(sp_read(0,b,3)==3&&!memcmp(b,"\x1b[A",3));
    assert(sp_read(0,b,1)==1&&b[0]==27);
    char cwd[32];
    assert(sp_getcwd(cwd,sizeof cwd)==0&&!strcmp(cwd,"\\\\drv0"));
    puts("PASS: bounded POSIX descriptors, seek, append, console and raw/canonical TTY");
}
