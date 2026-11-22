#include "shell_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
int base_edl_main(const struct edl_api *);
int text_edl_main(const struct edl_api *);
int editor_edl_main(const struct edl_api *);
static struct shell_api sh;
static char output[10000];
static unsigned outn;
static char original[1024]="hello world\nsecond line\n";
static unsigned original_n=24;
static char copied[1024];
static unsigned copied_n;
static int created;
static const int *keys;
static unsigned key_index;
static void output_reset(void) {outn=0;output[0]=0;}
static int mock_console(const void *s,unsigned n)
{
    assert(outn+n < sizeof output);
    memcpy(output+outn,s,n);outn+=n;output[outn]=0;return (int)n;
}
static int mock_resolve(const char *given,char out[EDL_PATH_MAX])
{
    snprintf(out,EDL_PATH_MAX,"\\\\drv0\\%s",given);return 0;
}
static int mock_size(const char *path)
{
    if (!strcmp(path,"\\\\drv0\\notes.txt")) return (int)original_n;
    if (!strcmp(path,"\\\\drv0\\copy.txt") && created) return (int)copied_n;
    return -1;
}
static int mock_read(const char *path,uint32_t offset,void *dest,uint32_t length)
{
    const char *source=NULL;unsigned size=0;
    if (!strcmp(path,"\\\\drv0\\notes.txt")) {source=original;size=original_n;}
    if (!strcmp(path,"\\\\drv0\\copy.txt") && created) {source=copied;size=copied_n;}
    if (!source || offset>size || length>size-offset) return -1;
    memcpy(dest,source+offset,length);return (int)length;
}
static int mock_write(const char *path,unsigned offset,const void *data,unsigned n,unsigned flags)
{
    if (strcmp(path,"\\\\drv0\\copy.txt") && strcmp(path,"\\\\drv0\\notes.txt")) return -1;
    char *destination=!strcmp(path,"\\\\drv0\\notes.txt")?original:copied;
    unsigned *length=!strcmp(path,"\\\\drv0\\notes.txt")?&original_n:&copied_n;
    if (flags&SHELL_WRITE_TRUNC) *length=0;
    if (flags&SHELL_WRITE_CREATE) created=1;
    if (offset+n>1024 || offset>*length) return -1;
    memcpy(destination+offset,data,n);
    if (offset+n>*length) *length=offset+n;
    return (int)n;
}
static int mock_list(const char *path,unsigned first,struct shell_dirent *out,unsigned cap)
{
    if (strcmp(path,"\\\\drv0\\.")) return -1;
    const char *names[]={"notes.txt","copy.txt"};
    for (unsigned i=0;i<cap && first+i<2;i++) {
        memset(&out[i],0,sizeof out[i]);
        strcpy(out[i].name,names[first+i]);
    }
    return 2;
}
static void mock_rect(int x,int y,int w,int h,uint16_t color)
{(void)x;(void)y;(void)w;(void)h;(void)color;}
static void mock_text(int x,int y,uint16_t color,const char *s)
{(void)x;(void)y;(void)color;(void)s;}
static void mock_present(void) {}
static int mock_key(void)
{
    assert(keys!=NULL && key_index<80);
    return keys[key_index++];
}
static void setup(void)
{
    memset(&sh,0,sizeof sh);
    sh.edl.magic=EDL_ABI_MAGIC;
    sh.edl.version=EDL_ABI_VERSION;
    sh.edl.size=sizeof sh;
    sh.shell_magic=SHELL_SERVICE_MAGIC;
    sh.shell_version=SHELL_SERVICE_VERSION;
    sh.edl.read=mock_read;
    sh.edl.rect=mock_rect;
    sh.edl.text=mock_text;
    sh.edl.present=mock_present;
    sh.console_write=mock_console;
    sh.resolve_path=mock_resolve;
    sh.file_size=mock_size;
    sh.file_write=mock_write;
    sh.list_dir=mock_list;
    sh.read_key=mock_key;
}
static int run(int (*f)(const struct edl_api *),const char *cmd,
               int argc,const char *const argv[])
{
    output_reset();sh.command=cmd;sh.argv=argv;sh.argc=argc;
    return f(&sh.edl);
}
int main(void)
{
    setup();
    const char *cat[]={"cat","notes.txt"};
    assert(run(base_edl_main,"cat",2,cat)==0);
    assert(!strcmp(output,"hello world\nsecond line\n"));
    const char *cp[]={"cp","notes.txt","copy.txt"};
    assert(run(base_edl_main,"cp",3,cp)==0 && created && copied_n==original_n);
    assert(!memcmp(copied,original,original_n));
    const char *grep[]={"grep","second","copy.txt"};
    assert(run(text_edl_main,"grep",3,grep)==0);
    assert(!strcmp(output,"second line\n"));
    const char *wc[]={"wc","copy.txt"};
    assert(run(text_edl_main,"wc",2,wc)==0);
    assert(strstr(output,"2 4 24 copy.txt")!=NULL);
    const char *head[]={"head","copy.txt"};
    assert(run(text_edl_main,"head",2,head)==0);
    assert(!strcmp(output,"hello world\nsecond line\n"));
    const int keyseq[]={ 'i', 'H', SHELL_KEY_ESC, ':','w','q',SHELL_KEY_ENTER };
    keys=keyseq;key_index=0;
    const char *vi[]={"vi","copy.txt"};
    assert(run(editor_edl_main,"vi",2,vi)==0);
    assert(copied[0]=='H' && copied[1]=='h' && key_index==7);
    puts("PASS: base/text multicall dispatch, file copy, grep/wc/head, vi save/return");
    return 0;
}
