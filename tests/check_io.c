#include "shell_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char disk[1024];
static const char *unsafe_path;
static const void *unsafe_bytes;
static unsigned reads,writes,created;
static int native_read(const char *path,uint32_t off,void *dst,uint32_t n)
{
    assert(strcmp(path,unsafe_path)==0 && path!=unsafe_path);
    assert(dst!=unsafe_bytes && off+n<=sizeof disk);
    memcpy(dst,disk+off,n);reads++;return (int)n;
}
static int native_write(const char *path,unsigned off,const void *src,
                        unsigned n,unsigned flags)
{
    assert(strcmp(path,unsafe_path)==0 && path!=unsafe_path);
    assert(src!=unsafe_bytes && off+n<=sizeof disk);
    if(flags&2u)created++;
    else assert(!(flags&1u));
    if(n)memcpy(disk+off,src,n);
    writes++;return (int)n;
}
int main(void)
{
    char source[700],dest[700],path[]="\\\\drv0\\COPY.TXT";
    for(unsigned i=0;i<sizeof source;i++) source[i]=(char)('A'+i%26);
    unsafe_path=path;unsafe_bytes=source;
    assert(shell_io_write(native_write,path,0,source,sizeof source,3u)==700);
    assert(writes==3&&created==1);
    unsafe_bytes=dest;
    assert(shell_io_read(native_read,path,0,dest,sizeof dest)==700);
    assert(reads==3&&!memcmp(dest,source,sizeof source));
    assert(shell_io_write(native_write,path,0,source,0,2u)==0);
    assert(shell_io_read(native_read,path,0,NULL,3)==-1);
    assert(shell_io_read(native_read,path,UINT32_MAX,NULL,2)==-1);
    puts("PASS: native world staging avoids direct payload pointers and chunks I/O");
}
