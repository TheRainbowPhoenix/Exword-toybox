/* EX-word native tools using the bounded POSIX-like EDL descriptor shim.
   These are platform-specific additions, NOT copied BusyBox implementations. */
#include "module_util.h"
#include "shell_posix.h"
#include <string.h>
static const char digits[]="0123456789abcdef";
static int write_all(const char *s,unsigned n)
{ return sp_write(1,s,n)==(int)n ? 0:-1; }
static int app_hexdump(const struct shell_api *sh)
{
    if(sh->argc!=2) return m_usage(sh,"usage: hexdump FILE");
    int fd=sp_open(sh->argv[1],SP_O_RDONLY);
    if(fd<0) return m_usage(sh,"hexdump: cannot open file");
    enum { WIDTH=12 }; /* 12 fits 64-column EX-word terminal. */
    unsigned char bytes[WIDTH];
    uint32_t off=0;
    int got;
    while((got=sp_read(fd,bytes,sizeof bytes))>0) {
        char line[90];
        unsigned n=0;
        for(int i=7;i>=0;i--)line[n++]=digits[(off>>(i*4))&15];
        line[n++]=' ';line[n++]=' ';
        for(int i=0;i<WIDTH;i++) {
            if(i<got) {
                line[n++]=digits[bytes[i]>>4];
                line[n++]=digits[bytes[i]&15];
            }else{line[n++]=' ';line[n++]=' ';}
            line[n++]=' ';
        }
        line[n++]=' '; /* ASCII preview without trailing | border. */
        for(int i=0;i<got;i++)
            line[n++]=bytes[i]>=32&&bytes[i]<127?(char)bytes[i]:'.';
        line[n++]='\n';
        if(write_all(line,n)) {sp_close(fd);return 1;}
        off+=(uint32_t)got;
    }
    sp_close(fd);
    if(got<0)return m_usage(sh,"hexdump: read failed");
    return 0;
}
static int app_more(const struct shell_api *sh)
{
    if(sh->argc!=2)return m_usage(sh,"usage: more FILE");
    int fd=sp_open(sh->argv[1],SP_O_RDONLY);
    if(fd<0)return m_usage(sh,"more: cannot open file");
    struct sp_winsize ws={24,64};
    sp_winsize(1,&ws);
    unsigned page=ws.rows>4?ws.rows-3:20;
    const char clear[]="\x1b[2J\x1b[H";
    write_all(clear,sizeof clear-1);
    unsigned lines=0,used=0;
    char line[192];
    unsigned char buffer[256];
    int rc=0,done=0;
    while(!done && (rc=sp_read(fd,buffer,sizeof buffer))>0) {
        for(int i=0;i<rc && !done;i++) {
            unsigned char c=buffer[i];
            if(used+2>=sizeof line || c=='\n') {
                if(c=='\n' && used+1<sizeof line)line[used++]='\n';
                if(write_all(line,used)){done=-1;break;}
                used=0;lines++;
                if(lines>=page) {
                    const char prompt[]="--More-- [SPACE: page, ENTER: line, Q: quit]";
                    if(write_all(prompt,sizeof prompt-1)){done=-1;break;}
                    int k;
                    do { k=sh->read_key(); }
                    while(k!=' ' && k!=SHELL_KEY_ENTER && k!='q' &&
                          k!='Q' && k!=SHELL_KEY_ESC);
                    if(k=='q'||k=='Q'||k==SHELL_KEY_ESC){done=1;break;}
                    if(write_all(clear,sizeof clear-1)){done=-1;break;}
                    lines=k==' '?0:page-1;
                }
            }
            if(c!='\n' && !done) {
                if(c==0 || c>=127) c='.';
                line[used++]=(char)c;
            }
        }
    }
    if(!done && used && write_all(line,used))done=-1;
    sp_close(fd);
    return (rc<0 || done<0) ? m_usage(sh,"more: read/write failure") : 0;
}
int posix_tool_main(const struct shell_api *sh)
{
    if(m_equal(sh->command,"hexdump")) return app_hexdump(sh);
    if(m_equal(sh->command,"more")) return app_more(sh);
    return 127;
}
