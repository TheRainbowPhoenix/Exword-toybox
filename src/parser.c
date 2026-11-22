#include "shell_parse.h"
#include <stddef.h>

/* Small in-place shell tokenizer. ASCII single/double quotes and separators,
   with deliberate preservation of EX-word native path backslashes and
   printf/tr escape sequences. Not a full POSIX shell language. */
int shell_parse(char *line, const char *argv[SHELL_MAX_ARGS])
{
    if (!line || !argv) return -1;
    unsigned argc=0;
    char *src=line,*dst=line;
    while(*src) {
        while(*src==' ' || *src=='\t') src++;
        if(!*src)break;
        if(argc>=SHELL_MAX_ARGS)return -1;
        argv[argc++]=dst;
        char quote=0;
        while(*src && (quote || (*src!=' ' && *src!='\t'))) {
            char ch=*src++;
            if(ch=='\\' && *src && quote!='\'') {
                int escape= quote=='"'
                    ? (*src=='\\' || *src=='"' || *src=='$' || *src==0x60)
                    : (*src==' ' || *src=='\t' || *src=='\\' ||
                       *src=='"' || *src=='\'');
                if(escape) { *dst++=*src++; continue; }
            }
            if(quote) {
                if(ch==quote)quote=0;
                else *dst++=ch;
            } else if(ch=='\'' || ch=='"') quote=ch;
            else *dst++=ch;
        }
        if(quote)return -1;
        /* src may equal dst, so consume the delimiter before inserting NUL. */
        if(*src) {src++;*dst++='\0';}
        else *dst++='\0';
    }
    return (int)argc;
}
