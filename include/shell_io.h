#ifndef TOYBOX_SHELL_IO_H
#define TOYBOX_SHELL_IO_H
#include <stdint.h>
/* Native world cannot safely be given buffers in the EDL executable arena.
   Bridge file I/O through resident BSS and copy in gint world on return.
   Callers are serialized (the EDL loader executes a single applet). */
typedef int (*shell_native_read)(const char *,uint32_t,void *,uint32_t);
typedef int (*shell_native_write)(const char *,unsigned,const void *,unsigned,unsigned);
int shell_io_read(shell_native_read native,const char *path,uint32_t offset,
                  void *dst,uint32_t length);
int shell_io_write(shell_native_write native,const char *path,unsigned offset,
                   const void *src,unsigned length,unsigned flags);
#endif
