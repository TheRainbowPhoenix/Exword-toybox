#ifndef TOYBOX_SHELL_POSIX_H
#define TOYBOX_SHELL_POSIX_H
#include "shell_api.h"
#include <stdint.h>
/* Small POSIX-like shim for porting BusyBox applets to an EDL with no OS
   processes, pipes or libc. It deliberately does not interpose libc symbols.
   File descriptors 0/1/2 are TTY, 3..6 are files. */
enum {
    SP_O_RDONLY=0, SP_O_WRONLY=1, SP_O_RDWR=2,
    SP_O_ACCMODE=3, SP_O_CREAT=0x0100, SP_O_TRUNC=0x0200,
    SP_O_APPEND=0x0400
};
enum { SP_ICANON=1, SP_ECHO=2 };
enum { SP_S_IFREG=0100000, SP_S_IFDIR=0040000 };
struct sp_stat { uint32_t size,mode; };
struct sp_termios { unsigned lflag; };
struct sp_winsize { unsigned rows,cols; };
void sp_bind(const struct shell_api *api);
int sp_open(const char *name,unsigned flags);
int sp_close(int fd);
int sp_read(int fd,void *buf,unsigned count);
int sp_write(int fd,const void *buf,unsigned count);
int sp_lseek(int fd,int32_t offset,int whence); /* SEEK_SET/CUR/END = 0/1/2 */
int sp_stat(const char *name,struct sp_stat *st);
int sp_fstat(int fd,struct sp_stat *st);
int sp_isatty(int fd);
int sp_tcgetattr(int fd,struct sp_termios *t);
int sp_tcsetattr(int fd,const struct sp_termios *t);
int sp_winsize(int fd,struct sp_winsize *w);
int sp_getcwd(char *out,unsigned cap);
int sp_unlink(const char *name);
int sp_rename(const char *from,const char *to);
int sp_mkdir(const char *name);
int sp_rmdir(const char *name);
struct sp_dirent { char name[128]; int directory; };
struct sp_DIR;
struct sp_DIR *sp_opendir(const char *name);
const struct sp_dirent *sp_readdir(struct sp_DIR *dir);
int sp_closedir(struct sp_DIR *dir);
#endif
