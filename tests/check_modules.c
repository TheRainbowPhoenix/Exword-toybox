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
static int test_keyboard_visible;
static unsigned full_editor_frames, compact_editor_frames;
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
{
    (void)color;
    if(x==0 && y==0 && w==528) {
        if(h==320)full_editor_frames++;
        if(h==205)compact_editor_frames++;
    }
}
static void mock_text(int x,int y,uint16_t color,const char *s)
{(void)x;(void)y;(void)color;(void)s;}
static void mock_present(void) {}
static int mock_key(void)
{
    assert(keys!=NULL && key_index<80);
    int key=keys[key_index++];
    if(key==SHELL_KEY_REDRAW) test_keyboard_visible=!test_keyboard_visible;
    return key;
}
static int mock_tty_size(unsigned *cols,unsigned *rows)
{
    *cols=64;
    *rows=test_keyboard_visible?14:24;
    return 0;
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
    sh.tty_size=mock_tty_size;
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
    const char *dump[]={"hexdump","notes.txt"};
    assert(run(text_edl_main,"hexdump",2,dump)==0);
    assert(strstr(output,"68 65 6c 6c 6f")!=NULL);
    const char *hexline=output;
    for(int row=0;row<2;row++) {
        const char *lf=strchr(hexline,'\n');
        assert(lf && lf-hexline<=64 && lf>hexline && lf[-1]!='|');
        hexline=lf+1;
    }
    assert(*hexline==0);
    const char *more[]={"more","notes.txt"};
    assert(run(text_edl_main,"more",2,more)==0);
    assert(strstr(output,"hello world")!=NULL);

    const char *cp[]={"cp","notes.txt","copy.txt"};
    assert(run(base_edl_main,"cp",3,cp)==0 && created && copied_n==original_n);
    assert(!memcmp(copied,original,original_n));
    const char *grep[]={"grep","second","copy.txt"};
    assert(run(text_edl_main,"grep",3,grep)==0);
    assert(!strcmp(output,"second line\n"));
    const char *wc[]={"wc","copy.txt"};
    assert(run(text_edl_main,"wc",2,wc)==0);
    assert(strstr(output,"2 4 24 copy.txt")!=NULL);
    const char *wc_l[]={"wc","-l","copy.txt"};
    assert(run(text_edl_main,"wc",3,wc_l)==0);
    assert(!strcmp(output,"2 copy.txt\n"));
    const char *wc_w[]={"wc","-wc","copy.txt"};
    assert(run(text_edl_main,"wc",3,wc_w)==0);
    assert(!strcmp(output,"4 24 copy.txt\n"));
    const char *wc_two[]={"wc","-l","copy.txt","notes.txt"};
    assert(run(text_edl_main,"wc",4,wc_two)==0);
    assert(strstr(output,"2 copy.txt\n") && strstr(output,"2 notes.txt\n"));
    assert(strstr(output,"4 total\n"));
    const char *wc_bad[]={"wc","-q","copy.txt"};
    assert(run(text_edl_main,"wc",3,wc_bad)!=0);
    const char *head[]={"head","copy.txt"};
    assert(run(text_edl_main,"head",2,head)==0);
    assert(!strcmp(output,"hello world\nsecond line\n"));
    const char *head_n[]={"head","-n","1","copy.txt"};
    assert(run(text_edl_main,"head",4,head_n)==0);
    assert(!strcmp(output,"hello world\n"));
    const char *head_attached[]={"head","-n1","copy.txt"};
    assert(run(text_edl_main,"head",3,head_attached)==0);
    assert(!strcmp(output,"hello world\n"));
    const char *head_bytes[]={"head","-c","5","copy.txt"};
    assert(run(text_edl_main,"head",4,head_bytes)==0);
    assert(!strcmp(output,"hello"));
    const char *head_zero[]={"head","-n0","copy.txt"};
    assert(run(text_edl_main,"head",3,head_zero)==0);
    assert(output[0]==0);
    const char *tail_n[]={"tail","-n","1","copy.txt"};
    assert(run(text_edl_main,"tail",4,tail_n)==0);
    assert(!strcmp(output,"second line\n"));
    const char *tail_attached[]={"tail","-n1","copy.txt"};
    assert(run(text_edl_main,"tail",3,tail_attached)==0);
    assert(!strcmp(output,"second line\n"));
    const char *tail_bytes[]={"tail","-c","5","copy.txt"};
    assert(run(text_edl_main,"tail",4,tail_bytes)==0);
    assert(!strcmp(output,"line\n"));
    const char *tail_zero[]={"tail","-n0","copy.txt"};
    assert(run(text_edl_main,"tail",3,tail_zero)==0 && !output[0]);
    const int keyseq[]={ 'i', 'H', SHELL_KEY_REDRAW, SHELL_KEY_REDRAW,
                          SHELL_KEY_ESC, ':','w','q',SHELL_KEY_ENTER };
    keys=keyseq;key_index=0;
    const char *vi[]={"vi","copy.txt"};
    assert(run(editor_edl_main,"vi",2,vi)==0);
    assert(copied[0]=='H' && copied[1]=='h' && key_index==9);
    assert(full_editor_frames>=2 && compact_editor_frames>=1);
    assert(!test_keyboard_visible);
    puts("PASS: wc/head/tail option parsing, compact hexdump, editor toggle redraw");
    return 0;
}
