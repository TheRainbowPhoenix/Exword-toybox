#include "shell_terminal.h"
#include <string.h>
/* A small scrollback plus a minimal ECMA-48 parser.  This is not terminfo,
   but it understands what common text tools output and the cursor motions
   needed by simple line/cell UIs. Unknown escape sequences are ignored. */
static char cells[SHELL_TERM_HISTORY][SHELL_TERM_COLS+1];
static unsigned cursor_row,cursor_col,used,back,view_height=24;
static unsigned esc_state,param[2],param_index,private_param;
static const char empty[]="";
static void reset_view(void) { back=0; }
static void clear_all(void)
{
    memset(cells,0,sizeof cells);
    used=1; cursor_row=cursor_col=back=0;
}
void shell_term_init(void)
{
    clear_all();esc_state=0;param[0]=param[1]=param_index=private_param=0;
}
static void ensure_row(unsigned row)
{
    if (row>=SHELL_TERM_HISTORY) row=SHELL_TERM_HISTORY-1;
    while (used<=row) {
        memset(cells[used],0,sizeof cells[used]);
        used++;
    }
}
static void newline(void)
{
    if (cursor_row+1==SHELL_TERM_HISTORY) {
        memmove(cells[0],cells[1],(SHELL_TERM_HISTORY-1)*sizeof cells[0]);
        memset(cells[SHELL_TERM_HISTORY-1],0,sizeof cells[0]);
        if (back<SHELL_TERM_HISTORY-1) back++;
    } else {
        cursor_row++;
        ensure_row(cursor_row);
    }
    cursor_col=0;
}
static void glyph(unsigned char ch)
{
    if (cursor_col>=SHELL_TERM_COLS) newline();
    if (ch<32 || ch>=127) ch='?'; /* Display font is not Shift-JIS/UTF-8. */
    cells[cursor_row][cursor_col++]=(char)ch;
    if (cells[cursor_row][cursor_col]) cells[cursor_row][cursor_col]=0;
    reset_view();
}
static unsigned arg(unsigned i,unsigned fallback)
{ return param[i]?param[i]:fallback; }
static void csi(unsigned char cmd)
{
    unsigned n=arg(0,1);
    if (private_param) return;
    switch(cmd) {
        case 'm': break; /* Character attributes unsupported; consume safely. */
        case 'J':
            if (param[0]==2 || param[0]==3) clear_all();
            else if (param[0]==0) {
                memset(cells[cursor_row]+cursor_col,0,
                    sizeof cells[0]-cursor_col);
                for (unsigned i=cursor_row+1;i<used;i++) cells[i][0]=0;
            }
            break;
        case 'K':
            if(param[0]==2) memset(cells[cursor_row],0,sizeof cells[0]);
            else if(param[0]==1) {
                unsigned last=cursor_col<SHELL_TERM_COLS?cursor_col:SHELL_TERM_COLS-1;
                memset(cells[cursor_row],0,last+1);
            } else if(cursor_col<SHELL_TERM_COLS)
                memset(cells[cursor_row]+cursor_col,0,sizeof cells[0]-cursor_col);
            break;
        case 'A': cursor_row=cursor_row>=n?cursor_row-n:0; break;
        case 'B': cursor_row=cursor_row+n>=SHELL_TERM_HISTORY?
                  SHELL_TERM_HISTORY-1:cursor_row+n;
                  ensure_row(cursor_row); break;
        case 'C': cursor_col=cursor_col+n>=SHELL_TERM_COLS?
                  SHELL_TERM_COLS-1:cursor_col+n; break;
        case 'D': cursor_col=cursor_col>=n?cursor_col-n:0; break;
        case 'G': cursor_col=n>SHELL_TERM_COLS?SHELL_TERM_COLS-1:n-1; break;
        case 'H': case 'f': {
            unsigned top=used>view_height?used-view_height:0;
            unsigned row=top+arg(0,1)-1;
            cursor_row=row>=SHELL_TERM_HISTORY?SHELL_TERM_HISTORY-1:row;
            cursor_col=arg(1,1)-1;
            if(cursor_col>=SHELL_TERM_COLS) cursor_col=SHELL_TERM_COLS-1;
            ensure_row(cursor_row);
            break;
        }
        default: break;
    }
}
void shell_term_put(int v)
{
    unsigned char ch=(unsigned char)v;
    if (esc_state==1) {
        if(ch=='[') { esc_state=2;param[0]=param[1]=param_index=private_param=0; }
        else if(ch==']') esc_state=3;
        else esc_state=0;
        return;
    }
    if (esc_state==2) {
        if(ch=='?') { private_param=1;return; }
        if(ch>='0' && ch<='9') {
            if(param[param_index]<999)
                param[param_index]=param[param_index]*10+(ch-'0');
            return;
        }
        if(ch==';') {if(param_index<1) param_index++;return;}
        csi(ch);esc_state=0;return;
    }
    if (esc_state==3) {
        if(ch==7) esc_state=0;
        else if(ch==27) esc_state=4;
        return;
    }
    if (esc_state==4) {esc_state=ch=='\\'?0:3;return;}
    if (ch==27) { esc_state=1;return; }
    if (ch=='\r') {cursor_col=0;return;}
    if (ch=='\n') {newline();return;}
    if (ch=='\b' || ch==127) {if(cursor_col)cursor_col--;return;}
    if (ch=='\t') { do {glyph(' ');}while(cursor_col%4);return;}
    if (ch<32) return;
    glyph(ch);
}
void shell_term_feed(const void *data,unsigned size)
{
    const unsigned char *p=data;
    for(unsigned i=0;i<size;i++) shell_term_put(p[i]);
}
void shell_term_page(int pages,unsigned height)
{
    if (!height || height>SHELL_TERM_HISTORY) height=SHELL_TERM_HISTORY;
    view_height=height;
    unsigned max_back=used>height?used-height:0;
    if(pages>0) {
        unsigned n=height/2;
        if(n==0)n=1;
        back=back+n>max_back?max_back:back+n;
    } else if (pages<0) {
        unsigned n=height/2;
        if(n==0)n=1;
        back=back>n?back-n:0;
    }
}
const char *shell_term_line(unsigned visible_row,unsigned height)
{
    if (!height || height>SHELL_TERM_HISTORY) height=SHELL_TERM_HISTORY;
    view_height=height;
    unsigned top=used>height?used-height:0;
    if(back>top) back=top;
    top-=back;
    unsigned row=top+visible_row;
    return visible_row<height && row<used ? cells[row] : empty;
}
unsigned shell_term_back(void) {return back;}
