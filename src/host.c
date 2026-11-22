/* Resident EX-word shell: standard gint D01, Edelweiss ELF loader and services. */
#include "host.h"
#include "host_fs.h"
#include "shell_names.h"
#include "shell_parse.h"
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/cpu.h>
#include <gint/gint.h>
#include <gint/rtc.h>
#include <gint/drivers/touch.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define TERM_COLS 64
#define TERM_ROWS 14
#define MAX_ROUTES 48u /* shared command pool, not 24 names per module */
#define WHITE 0xffff
#define DARK  0x0841
#define BLUE  0x2273
#define MUTED 0x8410
static char content[EDL_PATH_MAX], cwd[EDL_PATH_MAX], image_path[EDL_PATH_MAX];
static char input[SHELL_LINE_SIZE], previous[SHELL_LINE_SIZE];
static unsigned input_len, shift_mode, symbol_mode;
static char scroll[TERM_ROWS][TERM_COLS+1];
static unsigned scroll_row, scroll_col;
struct module { char path[EDL_PATH_MAX]; };
/* Allocate names once in a shared registry, not in per-module fixed arrays. */
struct route { char name[SHELL_NAME_SIZE]; uint8_t mod; };
_Static_assert(SHELL_MAX_MODULES <= UINT8_MAX, "module indices must fit in a byte");
static struct module modules[SHELL_MAX_MODULES];
static struct route routes[MAX_ROUTES];
static unsigned module_count, route_count;
static struct shell_api api;
static char command_line[SHELL_LINE_SIZE];

static int ci_equal(const char *a, const char *b)
{
    while (*a && *b) {
        char x=*a++, y=*b++;
        if (x>='A' && x<='Z') x+=32;
        if (y>='A' && y<='Z') y+=32;
        if (x!=y) return 0;
    }
    return !*a && !*b;
}
static int ci_starts(const char *a,const char *prefix,unsigned n)
{
    for (unsigned i=0;i<n;i++) {
        char x=a[i],y=prefix[i];
        if (!x || !y) return 0;
        if (x>='A' && x<='Z') x+=32;
        if (y>='A' && y<='Z') y+=32;
        if (x!=y) return 0;
    }
    return 1;
}
static int is_edl(const char *s)
{
    size_t n=strlen(s);
    return n>4 && ci_equal(s+n-4, ".edl");
}
static int combine(char dst[EDL_PATH_MAX], const char *parent, const char *leaf)
{
    size_t n=strlen(parent), m=strlen(leaf);
    if (n+m+2>EDL_PATH_MAX || strchr(leaf,'\\') || strchr(leaf,'/')) return -1;
    memcpy(dst,parent,n); dst[n++]='\\'; memcpy(dst+n,leaf,m+1);
    return 0;
}
/* Lexical normalization; never escape above the native device root. */
static int resolve(const char *given, char out[EDL_PATH_MAX])
{
    if (!given || !*given) return -1;
    char joined[EDL_PATH_MAX], work[EDL_PATH_MAX];
    size_t n=0;
    int absolute=(given[0]=='\\' || given[0]=='/');
    if (absolute) {
        if (given[0]=='\\' && given[1]=='\\') given+=2;
        else if (given[0]=='/' && given[1]=='/') given+=2;
        else given++;
        /* / means the drive mount root; explicit /drv0/... also accepted. */
        if (!(given[0]=='d' && !strncmp(given,"drv0",4)) &&
            !(given[0]=='c' && (!strncmp(given,"crd0",4) || !strncmp(given,"crd1",4)))) {
            memcpy(joined,"\\\\drv0\\",7); n=7;
        } else { joined[0]='\\'; joined[1]='\\'; n=2; }
    } else {
        n=strlen(cwd);
        if (n>=sizeof joined-1) return -1;
        memcpy(joined,cwd,n);
        joined[n++]='\\';
    }
    while (*given && n+1<sizeof joined) {
        char c=*given++;
        joined[n++]=c=='/'?'\\':c;
    }
    if (*given || n>=sizeof joined) return -1;
    joined[n]=0;
    /* Keep the first component (drv0/crd0/crd1) as the non-popable root. */
    if (joined[0]!='\\' || joined[1]!='\\') return -1;
    work[0]='\\';work[1]='\\';n=2;
    const char *p=joined+2;
    unsigned segments=0;
    while (*p) {
        while (*p=='\\') p++;
        if (!*p) break;
        const char *start=p;
        while (*p && *p!='\\') p++;
        size_t len=(size_t)(p-start);
        if (len==1 && start[0]=='.') continue;
        if (len==2 && start[0]=='.' && start[1]=='.') {
            if (segments>1) {
                while (n>2 && work[n-1]!='\\') n--;
                if (n>2) n--;
                segments--;
            }
            continue;
        }
        if (n+len+2>=sizeof work) return -1;
        if (n!=2) work[n++]='\\';
        memcpy(work+n,start,len); n+=len;
        segments++;
    }
    work[n]=0;
    if (!segments) return -1;
    const char *end=work+2;
    while (*end && *end!='\\') end++;
    size_t mount=(size_t)(end-(work+2));
    if (!(mount==4 && (!strncmp(work+2,"drv0",4) ||
                       !strncmp(work+2,"crd0",4) || !strncmp(work+2,"crd1",4)))) return -1;
    memcpy(out,work,n+1);
    return 0;
}
static void new_line(void)
{
    if (scroll_row+1<TERM_ROWS) scroll_row++;
    else { memmove(scroll,scroll+1,(TERM_ROWS-1)*(TERM_COLS+1)); scroll_row=TERM_ROWS-1; }
    memset(scroll[scroll_row],0,TERM_COLS+1);
    scroll_col=0;
}
static void append_char(char c)
{
    if (c=='\r') { scroll_col=0; return; }
    if (c=='\n') { new_line(); return; }
    if (c=='\t') { do { append_char(' '); } while (scroll_col%4); return; }
    if ((unsigned char)c<32 || (unsigned char)c>126) c='.';
    if (scroll_col==TERM_COLS) new_line();
    scroll[scroll_row][scroll_col++]=c;
    scroll[scroll_row][scroll_col]=0;
}
static void print(const char *s) { while (*s) append_char(*s++); }
static void println(const char *s) { print(s); new_line(); }
static void draw_keyboard(void)
{
    static const char *letters[3]={"qwertyuiop","asdfghjkl","zxcvbnm"};
    static const char *symbols[3]={"1234567890","-_/\\:;.,?@","!#$%&*()+="};
    drect(0,205,527,319,0x2104);
    for (unsigned row=0;row<3;row++) {
        const char *keys=symbol_mode?symbols[row]:letters[row];
        size_t count=strlen(keys);
        int width=48, gap=4;
        int x0=(528-(int)count*(width+gap)+gap)/2;
        int y=210+(int)row*26;
        for (size_t i=0;i<count;i++) {
            int x=x0+(int)i*(width+gap);
            drect(x,y,x+width-1,y+23,0xbdf7);
            char label[2]={keys[i],0};
            if (!symbol_mode && shift_mode && label[0]>='a' && label[0]<='z') label[0]-=32;
            dtext(x+19,y+7,DARK,label);
        }
    }
    static const char *label[]={"SHIFT","123","SPACE","ESC","DEL","ENTER"};
    static const int widths[]={77,68,130,66,70,82};
    int x=8;
    for (int i=0;i<6;i++) {
        uint16_t fill = (i==0 && shift_mode) || (i==1 && symbol_mode) ? 0x9dff : 0xe71c;
        drect(x,289,x+widths[i]-1,316,fill);
        dtext(x+5,298,DARK,i==1 && symbol_mode?"ABC":label[i]);
        x+=widths[i]+3;
    }
}
static void draw_terminal(void)
{
    dclear(DARK);
    drect(0,0,527,16,BLUE);
    char status[70];
    snprintf(status,sizeof status,"TOYBOX  %s",cwd[0]?cwd:"(starting)");
    status[64]=0;
    dtext(6,3,WHITE,status);
    for (unsigned i=0;i<TERM_ROWS;i++)
        if (scroll[i][0]) dtext(8,20+(int)i*12,WHITE,scroll[i]);
    drect(0,190,527,204,0x3186);
    char prompt[TERM_COLS+1]="EX> ";
    unsigned visible=input_len>58?58:input_len;
    const char *tail=input+input_len-visible;
    memcpy(prompt+4,tail,visible); prompt[4+visible]=0;
    dtext(8,193,WHITE,prompt);
    draw_keyboard();
    dupdate();
}
static int console_write(const void *bytes,unsigned length)
{
    if (!bytes && length) return -1;
    const char *p=bytes;
    for (unsigned i=0;i<length;i++) append_char(p[i]);
    draw_terminal();
    return (int)length;
}
static int keyboard_hit(int x,int y)
{
    if (y>=210 && y<284) {
        int row=(y-210)/26;
        if (row>=3 || (y-210)%26>=24) return 0;
        const char *keys=symbol_mode?
            (const char*[]){"1234567890","-_/\\:;.,?@","!#$%&*()+="}[row]:
            (const char*[]){"qwertyuiop","asdfghjkl","zxcvbnm"}[row];
        int count=(int)strlen(keys), x0=(528-count*52+4)/2;
        int index=(x-x0)/52;
        if (x<x0 || index<0 || index>=count || (x-x0)%52>=48) return 0;
        char c=keys[index];
        return shift_mode && !symbol_mode && c>='a' && c<='z' ? c-32 : c;
    }
    if (y>=289 && y<=316) {
        static const int widths[]={77,68,130,66,70,82};
        int left=8;
        for (int i=0;i<6;i++) {
            if (x>=left && x<left+widths[i]) {
                if (i==0) { shift_mode=!shift_mode; return -1; }
                if (i==1) { symbol_mode=!symbol_mode; return -1; }
                if (i==2) return ' ';
                if (i==3) return SHELL_KEY_ESC;
                if (i==4) return SHELL_KEY_BACK;
                return SHELL_KEY_ENTER;
            }
            left+=widths[i]+3;
        }
    }
    return 0;
}
static int read_key(void)
{
    draw_keyboard(); dupdate();
    for (;;) {
        key_event_t e=pollevent();
        if (e.type==KEYEV_NONE) { sleep(); continue; }
        if (e.type==KEYEV_TOUCH_DOWN) {
            int key=keyboard_hit(e.x,e.y);
            if (key==-1) { draw_keyboard(); dupdate(); continue; }
            if (key) return key;
            continue;
        }
        if (e.type!=KEYEV_DOWN && e.type!=KEYEV_HOLD) continue;
        if (e.key==KEY_EXE) return SHELL_KEY_ENTER;
        if (e.key==KEY_EXIT) return SHELL_KEY_BACK;
        if (e.key==KEY_ACON || e.key==KEY_MENU) return SHELL_KEY_ESC;
        if (e.key==KEY_UP) return SHELL_KEY_UP;
        if (e.key==KEY_DOWN) return SHELL_KEY_DOWN;
        if (e.key==KEY_LEFT) return SHELL_KEY_LEFT;
        if (e.key==KEY_RIGHT) return SHELL_KEY_RIGHT;
        int digit=keycode_digit(e.key);
        if (digit>=0 && digit<=9) return '0'+digit;
    }
}
/* Optional legacy callbacks, for modules using the Edelweiss ABI prefix. */
static void api_rect(int x,int y,int w,int h,uint16_t color)
{
    if (w>0 && h>0 && x>=0 && y>=0 && x<=DWIDTH-w && y<=DHEIGHT-h)
        drect(x,y,x+w-1,y+h-1,color);
}
static void api_text(int x,int y,uint16_t color,const char *s) { dtext(x,y,color,s); }
static void api_present(void) { dupdate(); }
static int api_wait(struct edl_event *e)
{
    int k=read_key();
    *e=(struct edl_event){0};
    e->key=k==SHELL_KEY_ENTER?EDL_OK:k==SHELL_KEY_ESC?EDL_BACK:
           k==SHELL_KEY_UP?EDL_UP:k==SHELL_KEY_DOWN?EDL_DOWN:
           k==SHELL_KEY_LEFT?EDL_LEFT:k==SHELL_KEY_RIGHT?EDL_RIGHT:0;
    return 0;
}
static int request_launch(const char *path) { (void)path; return -1; }
static int status(struct edl_status *out)
{
    rtc_time_t t;
    if (!out) return -1;
    rtc_get_time(&t);
    *out=(struct edl_status){.year=t.year,.month=t.month,.day=t.month_day,
        .hours=t.hours,.minutes=t.minutes,.seconds=t.seconds,.battery=-1};
    return 0;
}
/* Never let a command overwrite its own resident or loadable code files.
   This guards the only installed copy of our shell against accidental rm/vi. */
static int protected_module_path(const char *path)
{
    if (!path || !content[0]) return 0;
    unsigned n=(unsigned)strlen(content);
    if (!ci_starts(path,content,n) || path[n]!='\\') return 0;
    const char *leaf=path+n+1;
    if (strchr(leaf,'\\')) return 0;
    size_t k=strlen(leaf);
    return k>=4 && (ci_equal(leaf+k-4,".edl") || ci_equal(leaf+k-4,".d01"));
}
static int safe_write(const char *path,unsigned offset,const void *bytes,unsigned size,unsigned flags)
{
    return !shell_path_valid(path,0) || protected_module_path(path) ?
           -1 : host_fs_write(path,offset,bytes,size,flags);
}
static int safe_delete(const char *path)
{
    return !shell_path_valid(path,host_fs_dir_exists(path)) ||
           protected_module_path(path) ? -1 : host_fs_delete(path);
}
static int safe_rename(const char *from,const char *to)
{
    int directory=host_fs_dir_exists(from);
    return !shell_path_valid(from,directory) || !shell_path_valid(to,directory) ||
           protected_module_path(from) || protected_module_path(to) ?
           -1 : host_fs_rename(from,to);
}
static int safe_create(const char *path,int directory)
{
    return !shell_path_valid(path,directory) || protected_module_path(path) ?
           -1 : host_fs_create(path,directory);
}
static void init_api(void)
{
    api=(struct shell_api){
        .edl={.magic=EDL_ABI_MAGIC,.version=EDL_ABI_VERSION,
              .size=sizeof(struct shell_api),.width=DWIDTH,.height=DHEIGHT,
              .syscall_dispatcher=*(volatile uint32_t *)0x74000000,
              .image_path=image_path,.rect=api_rect,.text=api_text,
              .present=api_present,.wait=api_wait,.scan=edl_scan,
              .metadata=edl_metadata,.read=edl_read,.os_call=edl_os_call,
              .launch=request_launch,.status=status},
        .shell_magic=SHELL_SERVICE_MAGIC,.shell_version=SHELL_SERVICE_VERSION,
        .cwd=cwd,.console_write=console_write,.read_key=read_key,
        .resolve_path=resolve,.file_size=host_fs_size,.list_dir=host_fs_list,
        .file_write=safe_write,.file_delete=safe_delete,
        .file_rename=safe_rename,.file_create=safe_create
    };
}
static int read_manifest_bytes(void *ctx,uint32_t at,void *dst,size_t n)
{
    return edl_read((const char*)ctx,at,dst,(uint32_t)n)==(int)n?0:-1;
}
static int contains_app(const char *path,const char *appid)
{
    const char *s=path;
    while ((s=strchr(s,'\\'))!=NULL) {
        s++;
        if (!*s) break;
        char name[8]; unsigned n=0;
        while (s[n] && s[n]!='\\' && n<7) { name[n]=s[n]; n++; }
        name[n]=0;
        if (ci_equal(name,appid) && s[n]=='\\' &&
            ci_starts(s+n+1,"_CONTENT\\",9)) return 1;
    }
    return 0;
}
static int content_location(void)
{
    char id[9]={0},unused[8]={0},appid[7]={0}; unsigned order[4];
    edl_os_call(0x15,(uint32_t)(uintptr_t)id,(uint32_t)(uintptr_t)unused,0,0);
    if (edl_content_id(id,appid,order)) memcpy(appid,EDL_APPID,sizeof EDL_APPID);
    const char *preferred=id[0]=='c' ? (id[1]=='1'?"\\\\crd1\\":"\\\\crd0\\") : "\\\\drv0\\";
    int best=0;
    struct edl_record rows[8];
    int total=edl_scan(0,rows,8);
    if (total<=0) return -1;
    for (unsigned first=0;first<(unsigned)total;first+=8) {
        int rc=edl_scan(first,rows,8);
        if (rc<0) return -1;
        for (unsigned i=0;i<8 && first+i<(unsigned)total;i++) {
            const char *path=rows[i].path;
            if (!contains_app(path,appid)) continue;
            const char *last=strrchr(path,'\\');
            if (!last || (size_t)(last-path)>=sizeof content) continue;
            int score=ci_starts(path,preferred,(unsigned)strlen(preferred))?2:1;
            if (score<=best) continue;
            memcpy(content,path,(size_t)(last-path));
            content[last-path]=0;
            best=score;
        }
    }
    return best?0:-1;
}
static int add_module(const char *filename)
{
    if (!is_edl(filename) || module_count>=SHELL_MAX_MODULES) return -1;
    char path[EDL_PATH_MAX];
    if (combine(path,content,filename)) return -1;
    int size=host_fs_size(path);
    if (size<0) return -1;
    struct shell_manifest manifest;
    if (shell_manifest_read(read_manifest_bytes,path,(uint32_t)size,&manifest)) return -1;
    for (unsigned i=0;i<manifest.count;i++)
        for (unsigned j=0;j<route_count;j++)
            if (ci_equal(manifest.names[i],routes[j].name)) return -1;
    if (route_count+manifest.count>MAX_ROUTES) return -1;
    unsigned m=module_count++;
    memcpy(modules[m].path,path,strlen(path)+1);
    for (unsigned i=0;i<manifest.count;i++) {
        struct route *r=&routes[route_count++];
        memcpy(r->name,manifest.names[i],SHELL_NAME_SIZE);
        r->mod=(uint8_t)m;
    }
    return 0;
}
static void discover(void)
{
    module_count=route_count=0;
    struct shell_dirent entries[16];
    int total=host_fs_list(content,0,entries,16);
    if (total<0) return;
    for (unsigned first=0;first<(unsigned)total;first+=16) {
        if (host_fs_list(content,first,entries,16)<0) break;
        for (unsigned i=0;i<16 && first+i<(unsigned)total;i++)
            if (!entries[i].directory) add_module(entries[i].name);
    }
}
static int change_dir(const char *s)
{
    char full[EDL_PATH_MAX];
    if (resolve(s,full) || !host_fs_dir_exists(full)) return -1;
    memcpy(cwd,full,strlen(full)+1);
    return 0;
}
static void show_help(void)
{
    println("builtins: cd pwd help clear history modules exit");
    print("applets: ");
    for (unsigned i=0;i<route_count;i++) {
        print(routes[i].name);
        print((i+1)%6==0?"\n":" ");
    }
    new_line();
}
static int run_command(char *line)
{
    const char *argv[SHELL_MAX_ARGS]={0};
    int argc=shell_parse(line,argv);
    if (argc<0) { println("syntax: unmatched quote or too many arguments"); return -1; }
    if (!argc) return 0;
    if (ci_equal(argv[0],"exit")) return 9999;
    if (ci_equal(argv[0],"cd")) {
        if (argc>2 || change_dir(argc>1?argv[1]:content)) println("cd: directory not found");
        return 0;
    }
    if (ci_equal(argv[0],"pwd")) { println(cwd); return 0; }
    if (ci_equal(argv[0],"clear")) { memset(scroll,0,sizeof scroll);scroll_row=scroll_col=0;return 0; }
    if (ci_equal(argv[0],"history")) { println(previous); return 0; }
    if (ci_equal(argv[0],"help")) { show_help(); return 0; }
    if (ci_equal(argv[0],"modules")) {
        for (unsigned i=0;i<module_count;i++) {
            const char *leaf=strrchr(modules[i].path,'\\');
            println(leaf?leaf+1:modules[i].path);
        }
        return 0;
    }
    for (unsigned i=0;i<route_count;i++) {
        if (!ci_equal(argv[0],routes[i].name)) continue;
        const char *path=modules[routes[i].mod].path;
        memcpy(image_path,path,strlen(path)+1);
        api.command=argv[0]; api.argc=argc; api.argv=argv;
        api.edl.arena_start=edl_active_arena.arena_start;
        api.edl.arena_end=edl_active_arena.arena_end;
        api.edl.stack_start=edl_active_arena.stack_start;
        api.edl.stack_end=edl_active_arena.stack_end;
        /* Payload arena may contain dirty data left by the previous applet. */
        for (uint32_t p=api.edl.arena_start;p<api.edl.stack_end;p+=32)
            __asm__ volatile("ocbp @%0" :: "r"(p) : "memory");
        __asm__ volatile("synco" ::: "memory");
        struct edl_image image;
        int rc=edl_load(image_path,&image);
        if (!rc) {
            edl_sync_code();
            *(volatile uint32_t *)(uintptr_t)api.edl.stack_start=0x45444c53;
            clearevents();
            rc=edl_invoke(image.entry,&api.edl,api.edl.stack_end);
            edl_gint_cleanup();
            if (*(volatile uint32_t *)(uintptr_t)api.edl.stack_start!=0x45444c53) {
                println("FATAL: payload stack canary destroyed");
                return -12;
            }
            clearevents();
        }
        if (rc) { char msg[50];snprintf(msg,sizeof msg,"applet exit status: %d",rc);println(msg); }
        return rc;
    }
    println("command not found; type help");
    return -1;
}
int main(void)
{
    dclear(DARK); dtext(20,22,WHITE,"Toybox: initializing Edelweiss..."); dupdate();
    int rc=edl_profile_init();
    if (rc) {
        char msg[60]; snprintf(msg,sizeof msg,"Unsupported firmware / memory (%d)",rc);
        dtext(20,48,WHITE,msg); dupdate();
        while (read_key()!=SHELL_KEY_ESC) {}
        touch_exword_quit(); return -2;
    }
    if (content_location()) {
        dtext(20,48,WHITE,"No EDL package content found"); dupdate();
        while (read_key()!=SHELL_KEY_ESC) {}
        edl_profile_release();touch_exword_quit();return -2;
    }
    memcpy(cwd,content,strlen(content)+1);
    init_api(); discover();
    println("EX-word Toybox shell (experimental)");
    println("Touch keyboard below. Type help for commands.");
    if (!module_count) println("No valid .edl command modules found.");
    for (;;) {
        draw_terminal();
        int key=read_key();
        if (key==SHELL_KEY_ENTER) {
            input[input_len]=0;
            if (input_len) memcpy(previous,input,input_len+1);
            print("EX> "); println(input);
            memcpy(command_line,input,input_len+1);
            input_len=0;input[0]=0;
            if (run_command(command_line)==9999) break;
        } else if (key==SHELL_KEY_BACK) {
            if (input_len) input[--input_len]=0;
        } else if (key==SHELL_KEY_ESC) {
            input_len=0;input[0]=0;
        } else if (key==SHELL_KEY_UP) {
            strncpy(input,previous,sizeof input-1); input[sizeof input-1]=0;
            input_len=(unsigned)strlen(input);
        } else if (key>=32 && key<127 && input_len+1<sizeof input) {
            input[input_len++]=(char)key;input[input_len]=0;
        }
    }
    edl_profile_release();touch_exword_quit();return -2;
}
