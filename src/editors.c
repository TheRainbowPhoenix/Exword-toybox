/* Minimal, real editable text buffer with vi-style and nano-style key bindings.
   This is intentionally a first-stage EX-word editor, not BusyBox's vi.c or
   GNU nano. A full BusyBox vi port still requires tty/termios compatibility. */
#include "module_util.h"
#include "shell_format.h"
#define MAX_TEXT 32768u
#define SCREEN_LINES_SMALL 13u
#define SCREEN_LINES_LARGE 22u
#define COLS 63u
#define INK 0x0841
#define WHITE 0xffff
#define BAR 0x2273
__attribute__((used,section(".edl.cmd"),aligned(4)))
const struct shell_manifest shell_commands={
    .magic=SHELL_MANIFEST_MAGIC,.version=SHELL_MANIFEST_VERSION,.count=2,
    .names={"vi","nano"}
};
static char data[MAX_TEXT+1];
static unsigned len,cursor,top_line;
static int dirty,nano,insert_mode,command_mode;
static char colon[24],message[64];
static unsigned colon_len;
static const struct shell_api *sh;
static char path[EDL_PATH_MAX];

static void set_message(const char *s)
{
    unsigned n=0;
    while (s[n] && n<sizeof message-1) {message[n]=s[n];n++;}
    message[n]=0;
}
static unsigned line_begin(unsigned at)
{ while (at && data[at-1]!='\n') at--;return at; }
static unsigned line_end(unsigned at)
{ while (at<len && data[at]!='\n') at++;return at; }
static unsigned lines_before(unsigned at)
{
    unsigned n=0;
    for (unsigned i=0;i<at;i++) if (data[i]=='\n') n++;
    return n;
}
static unsigned line_start_for(unsigned row)
{
    unsigned pos=0;
    while (row-- && pos<len) {
        while (pos<len && data[pos]!='\n') pos++;
        if (pos<len) pos++;
    }
    return pos;
}
static void move_vertical(int direction)
{
    unsigned begin=line_begin(cursor),col=cursor-begin;
    if (direction<0) {
        if (!begin) return;
        unsigned prev=line_begin(begin-1),end=line_end(prev);
        cursor=prev+(col<end-prev?col:end-prev);
    } else {
        unsigned end=line_end(cursor);
        if (end==len) return;
        unsigned next=end+1,nextend=line_end(next);
        cursor=next+(col<nextend-next?col:nextend-next);
    }
}
static void draw_editor(void)
{
    unsigned cols=64, rows=14;
    /* tty_size reflects the resident touch keyboard visibility.
       The editor owns the entire 320px screen when it is hidden. */
    if(sh->tty_size) sh->tty_size(&cols,&rows);
    unsigned visible=rows>=24?SCREEN_LINES_LARGE:SCREEN_LINES_SMALL;
    int status_y=visible==SCREEN_LINES_LARGE?289:181;
    unsigned line=lines_before(cursor);
    if (line<top_line) top_line=line;
    if (line>=top_line+visible) top_line=line-visible+1;
    sh->edl.rect(0,0,528,visible==SCREEN_LINES_LARGE?320:205,WHITE);
    sh->edl.rect(0,0,528,18,BAR);
    sh->edl.text(6,3,WHITE,nano?"nano-lite  ESC: Exit":"vi-lite  i: insert   :wq: save+quit");
    unsigned at=line_start_for(top_line),cur_line=top_line;
    for (unsigned row=0;row<visible;row++,cur_line++) {
        char text[COLS+1];unsigned n=0;
        unsigned start=at;
        while (at<len && data[at]!='\n') {
            if (n<COLS) text[n++]=data[at]=='\t'?' ':data[at];
            at++;
        }
        text[n]=0;
        sh->edl.text(9,22+(int)row*12,INK,text);
        if (cur_line==line) {
            unsigned col=cursor-start;
            if (col>COLS-1) col=COLS-1;
            int x=8+(int)col*8,y=21+(int)row*12;
            sh->edl.rect(x,y,8,1,0xf800);
            sh->edl.rect(x,y+10,8,1,0xf800);
        }
        if (at<len && data[at]=='\n') at++;
    }
    sh->edl.rect(0,status_y,528,23,0xe71c);
    if (command_mode) {
        char prompt[30]={':' }; /* overwritten below for consistency */
        prompt[0]=':';
        for (unsigned i=0;i<colon_len && i<sizeof prompt-2;i++) prompt[i+1]=colon[i];
        sh->edl.text(8,status_y+5,INK,prompt);
    } else if (message[0]) sh->edl.text(8,status_y+5,INK,message);
    else sh->edl.text(8,status_y+5,INK,nano?"ESC: save / discard / cancel":
                      insert_mode?"-- INSERT --  ESC: normal":"-- NORMAL --");
    sh->edl.present();
}
static void insert_char(char c)
{
    if (len>=MAX_TEXT) {set_message("File buffer full (32 KiB)");return;}
    for (unsigned i=len;i>cursor;i--) data[i]=data[i-1];
    data[cursor++]=c;data[++len]=0;
    dirty=1;
}
static void delete_at(unsigned at)
{
    if (at>=len) return;
    for (unsigned i=at;i<len;i++) data[i]=data[i+1];
    len--;if (cursor>at) cursor--;
    dirty=1;
}
static int save(void)
{
    if (sh->file_write(path,0,data,len,SHELL_WRITE_CREATE|SHELL_WRITE_TRUNC)!=(int)len) {
        set_message("Save FAILED. Check storage.");return -1;
    }
    dirty=0;set_message("Saved");return 0;
}
static int finish_colon(void)
{
    colon[colon_len]=0;
    command_mode=0;colon_len=0;
    if (m_equal(colon,"q!")) return 1;
    if (m_equal(colon,"q")) {
        if (dirty) {set_message("Modified: :wq or :q! required");return 0;}
        return 1;
    }
    if (m_equal(colon,"w")) {save();return 0;}
    if (m_equal(colon,"wq") || m_equal(colon,"x")) return save()==0;
    set_message("Unknown : command");return 0;
}
static int editor_loop(void)
{
    for (;;) {
        draw_editor();
        int key=sh->read_key();
        if (key==SHELL_KEY_REDRAW) continue;
        if (nano && key==SHELL_KEY_ESC) {
            set_message("Exit? S=save D=discard C=cancel");
            draw_editor();
            int choice=sh->read_key();
            if (choice==SHELL_KEY_REDRAW) continue;
            if (choice=='s'||choice=='S') {if (save()==0) return 0;}
            else if (choice=='d'||choice=='D') return 0;
            else set_message("");
            continue;
        }
        if (command_mode) {
            if (key==SHELL_KEY_ENTER) { if (finish_colon()) return 0; }
            else if (key==SHELL_KEY_ESC) {command_mode=0;colon_len=0;}
            else if (key==SHELL_KEY_BACK) {if (colon_len) colon_len--;}
            else if (key>=32 && key<127 && colon_len+1<sizeof colon)
                colon[colon_len++]=(char)key;
            continue;
        }
        if (key==SHELL_KEY_LEFT) {if (cursor>line_begin(cursor)) cursor--;continue;}
        if (key==SHELL_KEY_RIGHT) {if (cursor<line_end(cursor)) cursor++;continue;}
        if (key==SHELL_KEY_UP) {move_vertical(-1);continue;}
        if (key==SHELL_KEY_DOWN) {move_vertical(1);continue;}
        if (!nano && !insert_mode) {
            if (key==SHELL_KEY_ESC) {set_message("");continue;}
            if (key=='i') {insert_mode=1;set_message("");continue;}
            if (key=='a') {if (cursor<line_end(cursor)) cursor++;insert_mode=1;continue;}
            if (key=='o') {cursor=line_end(cursor);insert_char('\n');insert_mode=1;continue;}
            if (key=='h') {if(cursor>line_begin(cursor)) cursor--;continue;}
            if (key=='l') {if(cursor<line_end(cursor)) cursor++;continue;}
            if (key=='k') {move_vertical(-1);continue;}
            if (key=='j') {move_vertical(1);continue;}
            if (key=='0') {cursor=line_begin(cursor);continue;}
            if (key=='$') {cursor=line_end(cursor);continue;}
            if (key=='x') {delete_at(cursor);continue;}
            if (key==':') {command_mode=1;colon_len=0;set_message("");continue;}
            continue;
        }
        if (key==SHELL_KEY_ESC) {insert_mode=0;set_message("");continue;}
        if (key==SHELL_KEY_BACK) {if(cursor) delete_at(cursor-1);continue;}
        if (key==SHELL_KEY_ENTER) {insert_char('\n');continue;}
        if (key>=32 && key<127) insert_char((char)key);
    }
}
int edl_main(const struct edl_api *edl)
{
    sh=shell_from_edl(edl);
    if (!sh || sh->argc!=2) {if(sh) m_line(sh,"usage: vi FILE / nano FILE");return 2;}
    if (m_path(sh,sh->argv[1],path)) return 2;
    nano=m_equal(sh->command,"nano");
    len=cursor=top_line=0;dirty=command_mode=colon_len=0;
    insert_mode=nano;message[0]=0;data[0]=0;
    int length=sh->file_size(path);
    if (length>=(int)MAX_TEXT) {m_line(sh,"editor: maximum file size 32 KiB");return 1;}
    if (length>0) {
        if (sh->edl.read(path,0,data,(unsigned)length)!=length) return 1;
        len=(unsigned)length;data[len]=0;
        for (unsigned i=0;i<len;i++) if (!data[i]) {
            m_line(sh,"editor: binary data not supported");return 1;
        }
    }
    return editor_loop();
}
