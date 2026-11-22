/*
 * Bounded POSIX-style API for EDL BusyBox applets.
 * Not an OS and not libc: no fork/exec/pipe/dup, sockets, signals, jobs, mmap,
 * permissions, symlinks, nonblocking I/O or file timestamps.
 * Path handling and destructive filename policy remain in the resident host.
 */
#include "shell_posix.h"
#include <string.h>
#include <limits.h>
#define SP_FILES 4u
#define SP_TTY_LINE 192u
struct descriptor {
    char path[EDL_PATH_MAX];
    uint32_t offset;
    unsigned flags;
    int used;
};
static const struct shell_api *env;
static struct descriptor desc[SP_FILES];
static unsigned tty_flags=SP_ICANON|SP_ECHO;
static char line[SP_TTY_LINE], pending[8];
static unsigned line_len,line_at,pending_len,pending_at;
void sp_bind(const struct shell_api *api)
{
    env=api;memset(desc,0,sizeof desc);
    tty_flags=SP_ICANON|SP_ECHO;
    line_len=line_at=pending_len=pending_at=0;
}
static struct descriptor *lookup(int fd)
{
    return fd>=3 && fd<(int)(3+SP_FILES) && desc[fd-3].used ?
           &desc[fd-3] : NULL;
}
static int stat_absolute(const char *path,struct sp_stat *st)
{
    if (!env || !path || !st) return -1;
    int n=env->file_size(path);
    if(n>=0) {st->size=(uint32_t)n;st->mode=SP_S_IFREG;return 0;}
    if(env->list_dir(path,0,NULL,0)>=0) {
        st->size=0;st->mode=SP_S_IFDIR;return 0;
    }
    return -1;
}
int sp_stat(const char *name,struct sp_stat *st)
{
    char path[EDL_PATH_MAX];
    return !env || !name || env->resolve_path(name,path) ?
           -1 : stat_absolute(path,st);
}
int sp_fstat(int fd,struct sp_stat *st)
{
    struct descriptor *d=lookup(fd);
    if(!st) return -1;
    if(fd>=0 && fd<3) {st->size=0;st->mode=0;return 0;}
    return d ? stat_absolute(d->path,st) : -1;
}
int sp_open(const char *name,unsigned flags)
{
    if(!env || !name || (flags & ~(SP_O_ACCMODE|SP_O_CREAT|SP_O_TRUNC|SP_O_APPEND)) ||
       (flags & SP_O_ACCMODE)==SP_O_ACCMODE) return -1;
    char path[EDL_PATH_MAX];
    if(env->resolve_path(name,path)) return -1;
    int size=env->file_size(path), created=0;
    if(size<0) {
        if(!(flags & SP_O_CREAT) || (flags&SP_O_ACCMODE)==SP_O_RDONLY ||
           env->file_create(path,0)<0) return -1;
        created=1; size=0;
    }
    if(flags & SP_O_TRUNC) {
        if((flags & SP_O_ACCMODE)==SP_O_RDONLY) return -1;
        if(!created && env->file_write(path,0,NULL,0,
                     SHELL_WRITE_TRUNC|SHELL_WRITE_CREATE)!=0) return -1;
        size=0;
    }
    for(unsigned i=0;i<SP_FILES;i++) if(!desc[i].used) {
        struct descriptor *d=&desc[i];
        memcpy(d->path,path,strlen(path)+1);
        d->offset=(flags&SP_O_APPEND)?(uint32_t)size:0;
        d->flags=flags;d->used=1;
        return (int)(3+i);
    }
    return -1;
}
int sp_close(int fd)
{
    struct descriptor *d=lookup(fd);
    if(!d)return -1;
    d->used=0;return 0;
}
static int key_bytes(int k)
{
    const char *seq=NULL;
    if(k==SHELL_KEY_ENTER) seq="\n";
    else if(k==SHELL_KEY_BACK) seq="\x7f";
    else if(k==SHELL_KEY_ESC) seq="\x1b";
    else if(k==SHELL_KEY_UP) seq="\x1b[A";
    else if(k==SHELL_KEY_DOWN) seq="\x1b[B";
    else if(k==SHELL_KEY_RIGHT) seq="\x1b[C";
    else if(k==SHELL_KEY_LEFT) seq="\x1b[D";
    else if(k==SHELL_KEY_PAGEUP) seq="\x1b[5~";
    else if(k==SHELL_KEY_PAGEDOWN) seq="\x1b[6~";
    if(seq) {
        pending_len=(unsigned)strlen(seq);
        memcpy(pending,seq,pending_len);
    } else if(k>0 && k<128) {
        pending_len=1;pending[0]=(char)k;
    } else return -1;
    pending_at=0;
    return 0;
}
static int tty_read(void *out,unsigned count)
{
    if(!count)return 0;
    if(!(tty_flags&SP_ICANON)) {
        for(;;) {
            if(pending_at<pending_len) break;
            if(key_bytes(env->read_key())==0) break;
        }
        unsigned n=pending_len-pending_at;
        if(n>count)n=count;
        memcpy(out,pending+pending_at,n);
        pending_at+=n;
        return (int)n;
    }
    if(line_at==line_len) {
        line_len=line_at=0;
        for(;;) {
            int k=env->read_key();
            if(k==SHELL_KEY_BACK) {
                if(line_len) {
                    line_len--;
                    if(tty_flags&SP_ECHO) env->console_write("\b \b",3);
                }
                continue;
            }
            if(k==SHELL_KEY_ENTER) k='\n';
            if(k<0 || k>=128 || (!((k>=32 && k<127) || k=='\t' || k=='\n')))
                continue;
            if(line_len+1>=SP_TTY_LINE && k!='\n') continue;
            line[line_len++]=(char)k;
            if(tty_flags&SP_ECHO) env->console_write(&line[line_len-1],1);
            if(k=='\n' || line_len==SP_TTY_LINE) break;
        }
    }
    unsigned n=line_len-line_at;
    if(n>count)n=count;
    memcpy(out,line+line_at,n);line_at+=n;
    return (int)n;
}
int sp_read(int fd,void *dst,unsigned count)
{
    if(!env || (!dst&&count))return -1;
    if(!count)return 0;
    if(fd==0)return tty_read(dst,count);
    struct descriptor *d=lookup(fd);
    if(!d || (d->flags&SP_O_ACCMODE)==SP_O_WRONLY) return -1;
    int size=env->file_size(d->path);
    if(size<0) return -1;
    if(d->offset>=(uint32_t)size)return 0;
    unsigned n=(unsigned)size-d->offset;
    if(n>count)n=count;
    int rc=env->edl.read(d->path,d->offset,dst,n);
    if(rc>=0)d->offset+=(unsigned)rc;
    return rc;
}
int sp_write(int fd,const void *src,unsigned count)
{
    if(!env || (!src&&count))return -1;
    if(!count)return 0;
    if(fd==1 || fd==2)return env->console_write(src,count);
    struct descriptor *d=lookup(fd);
    if(!d || (d->flags&SP_O_ACCMODE)==SP_O_RDONLY)return -1;
    if(d->flags&SP_O_APPEND) {
        int size=env->file_size(d->path);
        if(size<0)return -1;
        d->offset=(uint32_t)size;
    }
    int rc=env->file_write(d->path,d->offset,src,count,0);
    if(rc>0)d->offset+=(unsigned)rc;
    return rc;
}
int sp_lseek(int fd,int32_t delta,int whence)
{
    struct descriptor *d=lookup(fd);
    if(!d || whence<0 || whence>2)return -1;
    int size=env->file_size(d->path);
    if(size<0)return -1;
    int64_t base=whence==0?0:whence==1?(int64_t)d->offset:size;
    int64_t off=base+delta;
    if(off<0 || off>INT_MAX)return -1;
    d->offset=(uint32_t)off;
    return (int)off;
}
int sp_isatty(int fd) {return fd>=0 && fd<=2;}
int sp_tcgetattr(int fd,struct sp_termios *t)
{
    if(!sp_isatty(fd)||!t)return -1;
    t->lflag=tty_flags;return 0;
}
int sp_tcsetattr(int fd,const struct sp_termios *t)
{
    if(!sp_isatty(fd)||!t)return -1;
    tty_flags=t->lflag & (SP_ICANON|SP_ECHO);
    pending_len=pending_at=line_len=line_at=0;
    return 0;
}
int sp_winsize(int fd,struct sp_winsize *w)
{
    if(!sp_isatty(fd)||!w)return -1;
    w->cols=64;w->rows=24;
    if(env && env->tty_size) env->tty_size(&w->cols,&w->rows);
    return 0;
}
int sp_getcwd(char *out,unsigned cap)
{
    if(!env || !out || !cap || !env->cwd)return -1;
    unsigned n=(unsigned)strlen(env->cwd);
    if(n>=cap)return -1;
    memcpy(out,env->cwd,n+1);return 0;
}
