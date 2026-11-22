#include "shell_io.h"
#include <edl.h>
#include <string.h>
/* D01-owned resident memory, never EDL stack or arena storage. */
static unsigned char stage[256];
static char safe_path[EDL_PATH_MAX];
static int copy_path(const char *path)
{
    if(!path)return -1;
    size_t n=strlen(path);
    if(n>=sizeof safe_path)return -1;
    memcpy(safe_path,path,n+1);return 0;
}
int shell_io_read(shell_native_read native,const char *path,uint32_t offset,
                  void *dst,uint32_t length)
{
    if(!native || copy_path(path) || (length && !dst) ||
       offset>UINT32_MAX-length) return -1;
    uint32_t done=0;
    while(done<length) {
        uint32_t n=length-done;
        if(n>sizeof stage)n=sizeof stage;
        int rc=native(safe_path,offset+done,stage,n);
        if(rc!=(int)n) return -1;
        memcpy((unsigned char *)dst+done,stage,n);
        done+=n;
    }
    return (int)done;
}
int shell_io_write(shell_native_write native,const char *path,unsigned offset,
                   const void *src,unsigned length,unsigned flags)
{
    if(!native || copy_path(path) || (length&&!src) ||
       offset>UINT32_MAX-length) return -1;
    if(!length)return native(safe_path,offset,stage,0,flags);
    unsigned done=0;
    while(done<length) {
        unsigned n=length-done;
        if(n>sizeof stage)n=sizeof stage;
        memcpy(stage,(const unsigned char *)src+done,n);
        int rc=native(safe_path,offset+done,stage,n,
             done ? flags & ~3u : flags); /* CREATE/TRUNC only on first chunk. */
        if(rc!=(int)n)return -1;
        done+=n;
    }
    return (int)done;
}
