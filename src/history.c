#include "shell_history.h"
#include <string.h>

static void copy_line(char dst[SHELL_LINE_SIZE], const char *src)
{
    unsigned n=0;
    while(src[n] && n+1<SHELL_LINE_SIZE) {dst[n]=src[n];n++;}
    dst[n]=0;
}
static const char *item(const struct shell_history *h,unsigned i)
{
    return h->pool+h->at[i];
}
void shell_history_init(struct shell_history *h) {memset(h,0,sizeof *h);}
unsigned shell_history_count(const struct shell_history *h) {return h->count;}
const char *shell_history_get(const struct shell_history *h,unsigned index)
{ return index<h->count ? item(h,index) : NULL; }
static void drop_oldest(struct shell_history *h)
{
    if(h->count<=1) {h->count=h->used=0;return;}
    unsigned bytes=h->at[1];
    memmove(h->pool,h->pool+bytes,h->used-bytes);
    h->used-=bytes;
    h->count--;
    for(unsigned i=0;i<h->count;i++)h->at[i]=h->at[i+1]-bytes;
}
void shell_history_push(struct shell_history *h,const char *line)
{
    if(!line || !*line) {h->browsing=0;return;}
    unsigned i=0;
    while(line[i]==' ' || line[i]=='\t')i++;
    if(!line[i]) {h->browsing=0;return;}
    if(h->count && !strcmp(item(h,h->count-1),line)) {
        h->browsing=0;return;
    }
    unsigned n=(unsigned)strlen(line)+1;
    if(n>SHELL_HISTORY_BYTES) {h->browsing=0;return;}
    while(h->count && (h->count>=SHELL_HISTORY_DEPTH ||
                       h->used+n>SHELL_HISTORY_BYTES))
        drop_oldest(h);
    h->at[h->count]=(uint16_t)h->used;
    memcpy(h->pool+h->used,line,n);
    h->count++;h->used+=n;
    h->cursor=h->count;
    h->browsing=0;
}
const char *shell_history_up(struct shell_history *h,const char *draft)
{
    if(!h->count)return NULL;
    if(!h->browsing) {
        copy_line(h->draft,draft?draft:"");
        h->cursor=h->count;
        h->browsing=1;
    }
    if(h->cursor)h->cursor--;
    return item(h,h->cursor);
}
const char *shell_history_down(struct shell_history *h)
{
    if(!h->browsing)return NULL;
    if(h->cursor+1<h->count)return item(h,++h->cursor);
    h->cursor=h->count;h->browsing=0;
    return h->draft;
}
