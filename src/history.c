#include "shell_history.h"
#include <string.h>

static void copy_line(char dst[SHELL_LINE_SIZE], const char *src)
{
    unsigned n=0;
    while(src[n] && n+1<SHELL_LINE_SIZE) {dst[n]=src[n];n++;}
    dst[n]=0;
}
static const char *item(const struct shell_history *h, unsigned i)
{
    return h->entries[(h->start+i)%SHELL_HISTORY_DEPTH];
}
void shell_history_init(struct shell_history *h) {memset(h,0,sizeof *h);}
unsigned shell_history_count(const struct shell_history *h) {return h->count;}
const char *shell_history_get(const struct shell_history *h, unsigned index)
{ return index<h->count ? item(h,index) : NULL; }
void shell_history_push(struct shell_history *h, const char *line)
{
    if(!line || !*line) {h->browsing=0;return;}
    unsigned i=0;
    while(line[i]==' ' || line[i]=='\t')i++;
    if(!line[i]) {h->browsing=0;return;}
    if(h->count && !strcmp(item(h,h->count-1),line)) {
        h->browsing=0;return; /* No redundant consecutive entries. */
    }
    unsigned at;
    if(h->count<SHELL_HISTORY_DEPTH) {
        at=(h->start+h->count)%SHELL_HISTORY_DEPTH;
        h->count++;
    } else {
        at=h->start;
        h->start=(h->start+1)%SHELL_HISTORY_DEPTH;
    }
    copy_line(h->entries[at],line);
    h->cursor=h->count;
    h->browsing=0;
}
const char *shell_history_up(struct shell_history *h, const char *draft)
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
    h->cursor=h->count;
    h->browsing=0;
    return h->draft;
}
