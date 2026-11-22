#ifndef TOYBOX_SHELL_POSIX_COMPAT_H
#define TOYBOX_SHELL_POSIX_COMPAT_H
/* Opt-in source translation shim for simple BusyBox coreutils.
   Include after basic headers. Only basic file/TTY syscalls work; this
   does NOT implement libc's FILE*, errno, malloc or process semantics.
   POSIX flags used here are host-defined; do not pass system flags. */
#include "shell_posix.h"
#define O_RDONLY SP_O_RDONLY
#define O_WRONLY SP_O_WRONLY
#define O_RDWR   SP_O_RDWR
#define O_CREAT  SP_O_CREAT
#define O_TRUNC  SP_O_TRUNC
#define O_APPEND SP_O_APPEND
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define open(path,flags,...) sp_open((path),(flags))
#define read(fd,buf,n) sp_read((fd),(buf),(n))
#define write(fd,buf,n) sp_write((fd),(buf),(n))
#define close(fd) sp_close(fd)
#define lseek(fd,off,how) sp_lseek((fd),(off),(how))
#define stat sp_stat
#define fstat sp_fstat
#define st_size size
#define st_mode mode
#define isatty sp_isatty
#define tcgetattr sp_tcgetattr
#define tcsetattr(fd,when,p) sp_tcsetattr((fd),(p))
#define termios sp_termios
#define c_lflag lflag
#define ICANON SP_ICANON
#define ECHO SP_ECHO
#define opendir sp_opendir
#define readdir sp_readdir
#define closedir sp_closedir
#define DIR sp_DIR
#define dirent sp_dirent
#define d_name name
#define unlink sp_unlink
#define rename sp_rename
#define mkdir(name,mode) sp_mkdir(name)
#define rmdir sp_rmdir
#define getcwd(dst,cap) (sp_getcwd((dst),(cap))==0?(dst):NULL)
#ifndef S_ISREG
#define S_ISREG(mode) (((mode)&0170000)==SP_S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode)&0170000)==SP_S_IFDIR)
#endif
#endif
