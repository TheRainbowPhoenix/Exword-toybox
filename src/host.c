/* Resident EX-word shell: standard gint D01, Edelweiss ELF loader and services. */
#include "host.h"
#include "host_fs.h"
#include "shell_names.h"
#include "shell_parse.h"
#include "shell_keys.h"
#include "shell_io.h"
#include "shell_history.h"
#include "shell_terminal.h"
#include <gint/keycodes.h>
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/cpu.h>
#include <gint/gint.h>
#include <gint/rtc.h>
#include <gint/drivers/touch.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_ROUTES 48u
#define ROUTE_NAME_BYTES 256u /* Packed names instead of 48 x 16-byte slots */
#define WHITE 0xffff
#define DARK  0x0841
#define BLUE  0x2273
#define MUTED 0x8410
static char content[EDL_PATH_MAX], cwd[EDL_PATH_MAX], image_path[EDL_PATH_MAX];
static char input[SHELL_LINE_SIZE];
static struct shell_history history;
static unsigned input_len,input_cursor;
static int keyboard_visible, in_applet; /* default: physical keyboard */
static unsigned symbol_page;

/* Every EDL is adjacent to content: keep only the 8.3 basename. */
struct module { char name[13]; };
/* Store offsets into a compact byte pool; command names are NUL-terminated. */
struct route { uint8_t name_at, mod; };
_Static_assert(SHELL_MAX_MODULES <= UINT8_MAX, "module indices must fit in a byte");
static struct module modules[SHELL_MAX_MODULES];
static struct route routes[MAX_ROUTES];
static char route_names[ROUTE_NAME_BYTES];
static unsigned module_count, route_count, route_used;
static struct shell_api api;

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
static void new_line(void) { shell_term_put('\n'); }
static void append_char(char c) { shell_term_put((unsigned char)c); }
static void print(const char *s) { while (*s) append_char(*s++); }
static void println(const char *s) { print(s); new_line(); }
static unsigned terminal_height(void) { return keyboard_visible ? 14u : 24u; }
/* Most printable keys are now physical. Touch supplies missing symbols only. */
static void draw_keyboard(void)
{
    static const char *buttons[]={"SPACE","TAB","ESC","DEL","ENTER","SYM"};
    static const int widths[]={94,60,65,60,100,85};
    drect(0,205,527,319,0x2104);
    for (unsigned row=0;row<3;row++) {
        int x0=(528-10*52+4)/2;
        for (unsigned i=0;i<10;i++) {
            int x=x0+(int)i*52,y=210+(int)row*26;
            drect(x,y,x+47,y+23,0xbdf7);
            char label[2]={(char)shell_touch_symbol(symbol_page,row,i),0};
            dtext(x+18,y+7,DARK,label);
        }
    }
    int x=12;
    for(unsigned i=0;i<6;i++) {
        drect(x,289,x+widths[i]-1,316,0xe71c);
        dtext(x+8,298,DARK,buttons[i]);
        if(i==5)dtext(x+62,298,DARK,symbol_page?"2":"1");
        x+=widths[i]+5;
    }
}
static void draw_terminal(void)
{
    dclear(DARK);
    unsigned height=terminal_height();
    for (unsigned i=0;i<height;i++) {
        const char *line=shell_term_line(i,height);
        if (*line) dtext(8,4+(int)i*12,WHITE,line);
    }
    int prompt_y=keyboard_visible?185:301;
    drect(0,prompt_y-4,527,prompt_y+15,0x3186);
    char prompt[SHELL_TERM_COLS+1]="EX> ";
    unsigned start=input_cursor>58?input_cursor-58:0;
    unsigned visible=input_len-start;
    if(visible>58)visible=58;
    memcpy(prompt+4,input+start,visible);
    prompt[4+visible]=0;
    dtext(8,prompt_y,WHITE,prompt);
    unsigned cursor_x=8+(4+input_cursor-start)*8;
    drect((int)cursor_x,prompt_y+12,(int)cursor_x+6,prompt_y+13,WHITE);
    if (shell_term_back()) dtext(430,prompt_y,WHITE,"PGDN");
    if (keyboard_visible) draw_keyboard();
    dupdate();
}
static int console_write(const void *bytes,unsigned length)
{
    if (!bytes && length) return -1;
    shell_term_feed(bytes,length);
    draw_terminal();
    return (int)length;
}
static int keyboard_hit(int x,int y)
{
    if (!keyboard_visible) return 0;
    if (y>=210 && y<288) {
        int row=(y-210)/26;
        if(row>=3 || (y-210)%26>=24)return 0;
        int x0=(528-10*52+4)/2,idx=(x-x0)/52;
        if(x<x0 || idx<0 || idx>=10 || (x-x0)%52>=48)return 0;
        return shell_touch_symbol(symbol_page,(unsigned)row,(unsigned)idx);
    }
    if(y>=289 && y<=316) {
        static const int widths[]={94,60,65,60,100,85};
        static const int keys[]={' ','\t',SHELL_KEY_ESC,
                                  SHELL_KEY_BACK,SHELL_KEY_ENTER,
                                  SHELL_KEY_SYMBOL_PAGE};
        int x0=12;
        for (int i=0;i<6;i++) {
            if(x>=x0 && x<x0+widths[i]) return keys[i];
            x0+=widths[i]+5;
        }
    }
    return 0;
}
static int read_key(void)
{
    if(keyboard_visible) { draw_keyboard();dupdate(); }
    for (;;) {
        key_event_t e=waitevent(NULL); /* EX-word scans on poll, not sleep */
        if (e.type==KEYEV_NONE) continue;
        if (e.type==KEYEV_TOUCH_DOWN) {
            int key=keyboard_hit(e.x,e.y);
            if(key==SHELL_KEY_SYMBOL_PAGE) {
                symbol_page^=1u;
                draw_keyboard(); dupdate();
                if(in_applet)return SHELL_KEY_REDRAW;
                continue;
            }
            if(key) return key;
            continue;
        }
        if (e.type!=KEYEV_DOWN && e.type!=KEYEV_HOLD) continue;
        /* Do not auto-repeat the keyboard visibility toggle. */
        int key=shell_key_translate(e.key,
                 keydown(KEY_SHIFT) || e.shift,
                 keydown(KEY_ALPHA) || e.alpha);
        if(key==SHELL_KEY_KBD_TOGGLE) {
            if(e.type==KEYEV_DOWN) {
                keyboard_visible=!keyboard_visible;
                if(in_applet) {
                    /* Never paint the shell over an editor's own framebuffer.
                       Let the applet repaint immediately using tty_size(). */
                    if(keyboard_visible) { draw_keyboard(); dupdate(); }
                    return SHELL_KEY_REDRAW;
                }
                draw_terminal();
            }
            continue;
        }
        if((key==SHELL_KEY_PAGEUP || key==SHELL_KEY_PAGEDOWN) && !in_applet) {
            shell_term_page(key==SHELL_KEY_PAGEUP?1:-1,terminal_height());
            draw_terminal();
            continue;
        }
        if(key) return key;
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
/* Allow GUI EDLs to animate without waiting indefinitely for a key.
   Uses the host's native keyboard service, never a payload-side syscall. */
static int api_wait_timeout(struct edl_event *e,unsigned ms)
{
    if(!e) return -1;
    if(!ms) return api_wait(e);
    if(ms>60000) ms=60000;
    uint32_t start=rtc_ticks(),target=(ms*128u+999u)/1000u;
    *e=(struct edl_event){0};
    for(;;) {
        key_event_t event=pollevent();
        if(event.type==KEYEV_DOWN) {
            int k=shell_key_translate(event.key,
                keydown(KEY_SHIFT)||event.shift,
                keydown(KEY_ALPHA)||event.alpha);
            e->key=k==SHELL_KEY_ENTER?EDL_OK:k==SHELL_KEY_ESC?EDL_BACK:
                k==SHELL_KEY_UP?EDL_UP:k==SHELL_KEY_DOWN?EDL_DOWN:
                k==SHELL_KEY_LEFT?EDL_LEFT:k==SHELL_KEY_RIGHT?EDL_RIGHT:0;
            if(e->key)return 0;
        }
        uint32_t now=rtc_ticks();
        uint32_t elapsed=now>=start?now-start:128u*86400u-start+now;
        if(elapsed>=target)return 0;
    }
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
static int safe_read(const char *path,uint32_t offset,void *dst,uint32_t length)
{
    return shell_io_read(edl_read,path,offset,dst,length);
}
static int safe_write(const char *path,unsigned offset,const void *bytes,
                      unsigned size,unsigned flags)
{
    if (!shell_path_valid(path,0) || protected_module_path(path))
        return -1;
    return shell_io_write(host_fs_write,path,offset,bytes,size,flags);
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
static int tty_size(unsigned *cols,unsigned *rows)
{
    if(!cols || !rows) return -1;
    *cols=SHELL_TERM_COLS;
    *rows=terminal_height();
    return 0;
}
static void init_api(void)
{
    api=(struct shell_api){
        .edl={.magic=EDL_ABI_MAGIC,.version=EDL_ABI_VERSION,
              .size=sizeof(struct shell_api),.width=DWIDTH,.height=DHEIGHT,
              .syscall_dispatcher=*(volatile uint32_t *)0x74000000,
              .image_path=image_path,.rect=api_rect,.text=api_text,
              .present=api_present,.wait=api_wait,.wait_timeout=api_wait_timeout,.scan=edl_scan,
              .metadata=edl_metadata,.read=safe_read,.os_call=edl_os_call,
              .launch=request_launch,.status=status},
        .shell_magic=SHELL_SERVICE_MAGIC,.shell_version=SHELL_SERVICE_VERSION,
        .cwd=cwd,.console_write=console_write,.read_key=read_key,
        .resolve_path=resolve,.file_size=host_fs_size,.list_dir=host_fs_list,
        .file_write=safe_write,.file_delete=safe_delete,
        .file_rename=safe_rename,.file_create=safe_create,
        .tty_size=tty_size
    };
}
static int read_manifest_bytes(void *ctx,uint32_t at,void *dst,size_t n)
{
    return edl_read((const char*)ctx,at,dst,(uint32_t)n)==(int)n?0:-1;
}
/* Fast path: locate only this add-in's directory. The old edl_scan()
   repeated a full recursive scan of every installed app once per four
   records, making Toybox startup O(number_of_addins^2). */
static int content_location(void)
{
    char id[9]={0},unused[8]={0},appid[7]={0};
    unsigned order[4];
    edl_os_call(0x15,(uint32_t)(uintptr_t)id,(uint32_t)(uintptr_t)unused,0,0);
    if(edl_content_id(id,appid,order))memcpy(appid,EDL_APPID,sizeof EDL_APPID);
    const char *preferred=id[0]=='c' ? (id[1]=='1'?"\\\\crd1\\":"\\\\crd0\\") : "\\\\drv0\\";
    static const struct {
        const char *mount;
        int casio;
    } roots[]={
        {"\\\\drv0",0},{"\\\\drv0",1},{"\\\\crd0",1},{"\\\\crd1",1}
    };
    char root[EDL_PATH_MAX],app[EDL_PATH_MAX],folder[EDL_PATH_MAX];
    int best=0;
    for(unsigned i=0;i<sizeof roots/sizeof roots[0];i++) {
        const char *parent=roots[i].mount;
        if(roots[i].casio) {
            if(host_fs_find_dir_child(parent,"CASIO",root))continue;
            parent=root;
        }
        if(host_fs_find_dir_child(parent,appid,app) ||
           host_fs_find_dir_child(app,"_CONTENT",folder))continue;
        int score=ci_starts(folder,preferred,(unsigned)strlen(preferred))?2:1;
        if(score<=best)continue;
        memcpy(content,folder,strlen(folder)+1);
        best=score;
        if(score==2)break;
    }
    return best?0:-1;
}
static int add_module(const char *filename)
{
    if (!is_edl(filename) || strlen(filename)>12 || module_count>=SHELL_MAX_MODULES) return -1;
    char path[EDL_PATH_MAX];
    if (combine(path,content,filename)) return -1;
    int size=host_fs_size(path);
    if (size<0) return -1;
    struct shell_manifest manifest;
    if (shell_manifest_read(read_manifest_bytes,path,(uint32_t)size,&manifest)) return -1;
    unsigned needed=0;
    for (unsigned i=0;i<manifest.count;i++) {
        for (unsigned j=0;j<route_count;j++)
            if (ci_equal(manifest.names[i],
                         route_names+routes[j].name_at)) return -1;
        needed+=(unsigned)strlen(manifest.names[i])+1u;
    }
    if (route_count+manifest.count>MAX_ROUTES ||
        needed>ROUTE_NAME_BYTES-route_used) return -1;
    unsigned m=module_count++;
    memcpy(modules[m].name,filename,strlen(filename)+1);
    for (unsigned i=0;i<manifest.count;i++) {
        struct route *r=&routes[route_count++];
        unsigned n=(unsigned)strlen(manifest.names[i])+1u;
        r->name_at=(uint8_t)route_used;
        r->mod=(uint8_t)m;
        memcpy(route_names+route_used,manifest.names[i],n);
        route_used+=n;
    }
    return 0;
}
static void discover(void)
{
    module_count=route_count=route_used=0;
    struct shell_dirent entries[8];
    int total=host_fs_list(content,0,entries,8);
    if (total<0) return;
    for (unsigned first=0;first<(unsigned)total;first+=8) {
        if (host_fs_list(content,first,entries,8)<0) break;
        for (unsigned i=0;i<8 && first+i<(unsigned)total;i++)
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
/* Diagnostic for firmware-specific controls not named by gint's keycodes.
   In particular, EX-word volume rockers may not have a public enum value. */
static void scan_keys(void)
{
    println("Raw keys: press EXIT to return.");
    draw_terminal();
    for (;;) {
        key_event_t ev=waitevent(NULL);
        if(ev.type==KEYEV_NONE)continue;
        if(ev.type!=KEYEV_DOWN)continue;
        if(ev.key==KEY_EXIT)break;
        char msg[32];
        snprintf(msg,sizeof msg,"physical key: 0x%02x",(unsigned)ev.key);
        println(msg);
        draw_terminal();
    }
}
static void show_help(void)
{
    println("builtins: cd pwd help clear history modules keyscan exit");
    print("applets: ");
    for (unsigned i=0;i<route_count;i++) {
        print(route_names+routes[i].name_at);
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
    if (ci_equal(argv[0],"clear")) { shell_term_init();return 0; }
    if (ci_equal(argv[0],"history")) {
        for(unsigned i=0;i<shell_history_count(&history);i++) {
            char prefix[16];
            snprintf(prefix,sizeof prefix,"%u  ",i+1);
            print(prefix);
            println(shell_history_get(&history,i));
        }
        return 0;
    }
    if (ci_equal(argv[0],"help")) { show_help(); return 0; }
    if (ci_equal(argv[0],"keyscan")) { scan_keys(); return 0; }
    if (ci_equal(argv[0],"modules")) {
        for (unsigned i=0;i<module_count;i++) {
            println(modules[i].name);
        }
        return 0;
    }
    for (unsigned i=0;i<route_count;i++) {
        if (!ci_equal(argv[0],route_names+routes[i].name_at)) continue;
        if (combine(image_path,content,modules[routes[i].mod].name)) return -1;
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
            in_applet=1;
            rc=edl_invoke(image.entry,&api.edl,api.edl.stack_end);
            in_applet=0;
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
    shell_term_init();
    shell_history_init(&history);
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
    println("Physical QWERTY; AUDIO=touch; PGUP/PGDN=scroll.");
    println("DEL=backspace  EXIT=ESC  EXE=enter.");
    println("ZOOM=space  JUMP=tab  HISTORY=recall.");
    if (!module_count) println("No valid .edl command modules found.");
    for (;;) {
        draw_terminal();
        int key=read_key();
        if (key==SHELL_KEY_ENTER) {
            input[input_len]=0;
            shell_history_push(&history,input); /* Before parser tokenizes it. */
            print("EX> "); println(input);
            input_len=input_cursor=0;
            int rc=run_command(input);
            input[0]=0;
            if (rc==9999) break;
        } else if (key==SHELL_KEY_BACK) {
            if(input_cursor) {
                memmove(input+input_cursor-1,input+input_cursor,
                        input_len-input_cursor+1);
                input_cursor--;input_len--;
            }
        } else if (key==SHELL_KEY_LEFT) {
            if(input_cursor)input_cursor--;
        } else if (key==SHELL_KEY_RIGHT) {
            if(input_cursor<input_len)input_cursor++;
        } else if (key==SHELL_KEY_ESC) {
            input_len=input_cursor=0;input[0]=0;
        } else if (key==SHELL_KEY_UP || key==SHELL_KEY_DOWN) {
            const char *line=key==SHELL_KEY_UP ?
                shell_history_up(&history,input):shell_history_down(&history);
            if(line) {
                strncpy(input,line,sizeof input-1); input[sizeof input-1]=0;
                input_len=input_cursor=(unsigned)strlen(input);
            }
        } else if ((key=='\t' || (key>=32 && key<127)) &&
                   input_len+1<sizeof input) {
            memmove(input+input_cursor+1,input+input_cursor,
                    input_len-input_cursor+1);
            input[input_cursor++]=(char)key;input_len++;
        }
    }
    edl_profile_release();touch_exword_quit();return -2;
}
