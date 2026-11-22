/*
 * EX-word adaptations of BusyBox 0.60.5 basename, dirname, uniq, sort and tail.
 * Original applet sources: https://github.com/mirror/busybox/tree/0_60_5/coreutils
 *
 * Copyright (C) 1999-2001 Lineo, Inc., Erik Andersen, John Beppu,
 * Matt Kraai, and the BusyBox contributors.
 *
 * Adapted for the Edelweiss one-command-at-a-time EDL ABI, 2026.
 * Changes: native-path separators, bounded buffers, streaming reads,
 * no stdio/libbb/getopt/POSIX file descriptors, output through shell_api.
 * Some algorithms have been substantially rewritten for bounded memory.
 *
 * Licensed under GNU GPL version 2; see COPYING.BUSYBOX.
 */
#include "module_util.h"
#include <stdint.h>

/* Platform constraints: no stdin/pipes/tty yet; use explicit input files. */
#define BB_LINE_MAX 256u
#define BB_SORT_LINES 128u

/* Native EX-word paths also use backslashes. */
static int bb_sep(char c) { return c == '/' || c == '\\'; }
static int bb_same(const char *a, const char *b)
{
    while (*a && *b) { if (*a++ != *b++) return 0; }
    return *a == *b;
}
static unsigned bb_len(const char *s)
{
    unsigned n = 0;
    while (s[n]) n++;
    return n;
}
static int bb_basename(const struct shell_api *sh)
{
    int arg = 1;
    if (arg < sh->argc && m_equal(sh->argv[arg], "--")) arg++;
    if (sh->argc - arg < 1 || sh->argc - arg > 2)
        return m_usage(sh, "usage: basename PATH [SUFFIX]");
    const char *path = sh->argv[arg];
    unsigned length = bb_len(path);
    while (length && bb_sep(path[length-1])) length--;
    if (!length) { m_line(sh, *path ? (path[0]=='\\' ? "\\" : "/") : "."); return 0; }
    unsigned start = length;
    while (start && !bb_sep(path[start-1])) start--;
    unsigned n = length - start;
    const char *suffix = sh->argc-arg == 2 ? sh->argv[arg+1] : NULL;
    unsigned suffix_n = suffix ? bb_len(suffix) : 0;
    if (suffix_n && n > suffix_n) {
        unsigned i=0;
        while (i<suffix_n && path[start+n-suffix_n+i]==suffix[i]) i++;
        if (i==suffix_n) n-=suffix_n;
    }
    if (sh->console_write(path+start,n)!=(int)n) return 1;
    m_out(sh,"\n");
    return 0;
}
static int bb_dirname(const struct shell_api *sh)
{
    int arg = 1;
    if (arg < sh->argc && m_equal(sh->argv[arg], "--")) arg++;
    if (sh->argc - arg != 1) return m_usage(sh,"usage: dirname PATH");
    const char *path = sh->argv[arg];
    unsigned n = bb_len(path);
    while (n && bb_sep(path[n-1])) n--;
    if (!n) { m_line(sh, *path ? (path[0]=='\\' ? "\\" : "/") : "."); return 0; }
    while (n && !bb_sep(path[n-1])) n--;
    if (!n) { m_line(sh,"."); return 0; }
    while (n>1 && bb_sep(path[n-1])) n--;
    if (sh->console_write(path,n)!=(int)n) return 1;
    m_out(sh,"\n");
    return 0;
}
/* Adaptation of BusyBox's get_line_from_file() usage to bounded EDL reads. */
struct bb_reader {
    const struct shell_api *sh;
    char path[EDL_PATH_MAX];
    uint32_t length, offset;
    unsigned buffered, at;
    unsigned char buffer[256];
};
static int bb_open(struct bb_reader *r, const struct shell_api *sh, const char *path)
{
    r->sh=sh; r->offset=0; r->at=r->buffered=0;
    if (m_path(sh,path,r->path)) return -1;
    int n=sh->file_size(r->path);
    if (n<0) return -1;
    r->length=(uint32_t)n;
    return 0;
}
static int bb_next(struct bb_reader *r, char line[BB_LINE_MAX])
{
    unsigned used=0;
    for (;;) {
        if (r->at==r->buffered) {
            if (r->offset >= r->length) break;
            unsigned n=r->length-r->offset;
            if (n>sizeof r->buffer) n=sizeof r->buffer;
            if (r->sh->edl.read(r->path,r->offset,r->buffer,n)!=(int)n) return -1;
            r->offset+=n; r->at=0; r->buffered=n;
        }
        unsigned char ch=r->buffer[r->at++];
        /* Do not silently split or truncate a line (or treat a binary NUL as EOF). */
        if (!ch || used+1>=BB_LINE_MAX) return -1;
        line[used++]=(char)ch;
        if (ch=='\n') break;
    }
    line[used]=0;
    return used ? 1 : 0;
}
struct bb_writer {
    const struct shell_api *sh;
    char path[EDL_PATH_MAX];
    uint32_t offset;
    int file, started;
};
static int bb_writer_open(struct bb_writer *w, const struct shell_api *sh,
                          const char *destination)
{
    w->sh=sh; w->offset=0; w->file=destination!=NULL; w->started=0;
    return destination && m_path(sh,destination,w->path) ? -1 : 0;
}
static int bb_emit(struct bb_writer *w, const char *data, unsigned n)
{
    int rc;
    if (w->file) {
        rc=w->sh->file_write(w->path,w->offset,data,n,
                             w->started ? 0 : SHELL_WRITE_CREATE|SHELL_WRITE_TRUNC);
    } else rc=w->sh->console_write(data,n);
    if (rc!=(int)n) return -1;
    w->started=1;
    w->offset+=n;
    return 0;
}
static int bb_close(struct bb_writer *w)
{
    return w->file && !w->started ? bb_emit(w,"",0) : 0;
}
static int bb_same_file(const struct bb_reader *r,const struct bb_writer *w)
{
    return w->file && m_equal(r->path,w->path);
}
/* Tiny decimal formatter (libbb's printf is not available in a plain EDL). */
static int bb_count(struct bb_writer *w, unsigned count)
{
    char tmp[12], digits[12]; unsigned n=0,i=0;
    do { tmp[n++]=(char)('0'+count%10); count/=10; } while(count);
    while(n) digits[i++]=tmp[--n];
    digits[i++]=' ';
    return bb_emit(w,digits,i);
}
static int bb_uniq_run(struct bb_writer *w, const char *line,
                       unsigned count,int show_count,int duplicates,int singles)
{
    if (!count || (count>1&&!duplicates) || (count==1&&!singles)) return 0;
    if (show_count && bb_count(w,count)) return -1;
    return bb_emit(w,line,bb_len(line));
}
static int bb_uniq(const struct shell_api *sh)
{
    unsigned show_count=0,duplicate=1,single=1;
    int i=1;
    for (;i<sh->argc && sh->argv[i][0]=='-' && sh->argv[i][1];i++) {
        for (const char *p=sh->argv[i]+1;*p;p++) {
            if (*p=='c') show_count=1;
            else if (*p=='d') {duplicate=1;single=0;}
            else if (*p=='u') {duplicate=0;single=1;}
            else return m_usage(sh,"uniq: unknown option");
        }
    }
    if (sh->argc-i<1 || sh->argc-i>2) return m_usage(sh,"usage: uniq [-cdu] FILE [OUTPUT]");
    struct bb_reader r;
    struct bb_writer w;
    if (bb_open(&r,sh,sh->argv[i]) ||
        bb_writer_open(&w,sh,sh->argc-i==2?sh->argv[i+1]:NULL))
        return m_usage(sh,"uniq: cannot open file");
    if (bb_same_file(&r,&w)) return m_usage(sh,"uniq: input and output must differ");
    char prev[BB_LINE_MAX]={0},cur[BB_LINE_MAX];
    unsigned count=0;
    int rc;
    while ((rc=bb_next(&r,cur))==1) {
        if (count && !bb_same(cur,prev)) {
            if (bb_uniq_run(&w,prev,count,show_count,duplicate,single)) return 1;
            count=0;
        }
        if (!count) { unsigned n=bb_len(cur); memcpy(prev,cur,n+1); }
        count++;
    }
    if (rc<0) return m_usage(sh,"uniq: read failed or line exceeds 255 bytes");
    if (bb_uniq_run(&w,prev,count,show_count,duplicate,single) || bb_close(&w))
        return m_usage(sh,"uniq: write failed");
    return 0;
}
/* BusyBox 0.60.5 sort's compare_ascii/compare_numeric and qsort structure
 * adapted to fixed storage (no realloc, qsort or libc streams). */
static char bb_lines[BB_SORT_LINES][BB_LINE_MAX];
static uint8_t bb_order[BB_SORT_LINES];
static int bb_compare_ascii(const char *a,const char *b)
{
    unsigned i=0;
    for (;;) {
        char x=a[i],y=b[i];
        if (x=='\n' || x=='\r') x=0;
        if (y=='\n' || y=='\r') y=0;
        if (x!=y) return (unsigned char)x < (unsigned char)y ? -1:1;
        if (!x) return 0;
        i++;
    }
}
static long bb_atoi(const char *p)
{
    long n=0; int neg=0;
    while (*p==' '||*p=='\t') p++;
    if (*p=='-' || *p=='+') { neg=*p=='-';p++; }
    while (*p>='0' && *p<='9') {
        if (n<100000000) n=n*10+(*p-'0');
        p++;
    }
    return neg ? -n : n;
}
static int bb_compare(const char *a,const char *b,int numeric)
{
    if (numeric) {
        long x=bb_atoi(a),y=bb_atoi(b);
        if (x!=y) return x<y?-1:1;
    }
    return bb_compare_ascii(a,b);
}
static int bb_sort(const struct shell_api *sh)
{
    int reverse=0,numeric=0,unique=0;
    const char *source=NULL,*dest=NULL;
    for (int i=1;i<sh->argc;i++) {
        const char *p=sh->argv[i];
        if (m_equal(p,"-o")) {
            if (++i>=sh->argc || dest) return m_usage(sh,"sort: missing output");
            dest=sh->argv[i];
        } else if (p[0]=='-' && p[1]) {
            for (p++;*p;p++) {
                if (*p=='n') numeric=1;
                else if (*p=='r') reverse=1;
                else if (*p=='u') unique=1;
                else return m_usage(sh,"sort: unknown option");
            }
        } else if (!source) source=p;
        else return m_usage(sh,"sort: one input file supported");
    }
    if (!source) return m_usage(sh,"usage: sort [-nru] [-o OUTPUT] FILE");
    struct bb_reader r;
    struct bb_writer w;
    if (bb_open(&r,sh,source) || bb_writer_open(&w,sh,dest))
        return m_usage(sh,"sort: cannot open file");
    int rc; unsigned total=0;
    while (total<BB_SORT_LINES && (rc=bb_next(&r,bb_lines[total]))==1) {
        bb_order[total]=(uint8_t)total;
        total++;
    }
    if (total==BB_SORT_LINES) {
        char probe[BB_LINE_MAX];
        if (bb_next(&r,probe)!=0)
            return m_usage(sh,"sort: limit is 128 lines of 255 bytes");
    } else if (rc<0) return m_usage(sh,"sort: bad input or line too long");
    for (unsigned i=1;i<total;i++) {
        unsigned j=i;
        while (j>0 && bb_compare(bb_lines[bb_order[j-1]],
                                 bb_lines[bb_order[j]],numeric)>0) {
            uint8_t t=bb_order[j-1];bb_order[j-1]=bb_order[j];bb_order[j]=t;
            j--;
        }
    }
    for (unsigned i=0;i<total;i++) {
        unsigned at=reverse?total-1-i:i;
        const char *line=bb_lines[bb_order[at]];
        if (unique && i && !bb_compare(line,
             bb_lines[bb_order[reverse?at+1:at-1]],numeric)) continue;
        if (bb_emit(&w,line,bb_len(line)))
            return m_usage(sh,"sort: write failed");
    }
    return bb_close(&w) ? m_usage(sh,"sort: write failed") : 0;
}
static int bb_number(const char *p,unsigned *out)
{
    unsigned n=0;
    if (!p || !*p) return -1;
    for (;*p;p++) {
        if (*p<'0'||*p>'9') return -1;
        if (n > 1000000u) return -1;
        n=n*10+(*p-'0');
    }
    *out=n;return 0;
}
/* Reverse-scan variant of BusyBox tail's last-N-lines logic. */
static int bb_tail(const struct shell_api *sh)
{
    int i=1, bytes=0;
    unsigned count=10;
    if (i<sh->argc && (m_equal(sh->argv[i],"-n")||m_equal(sh->argv[i],"-c"))) {
        bytes=m_equal(sh->argv[i],"-c");
        if (i+1>=sh->argc || bb_number(sh->argv[i+1],&count))
            return m_usage(sh,"tail: bad count");
        i+=2;
    }
    if (sh->argc-i!=1) return m_usage(sh,"usage: tail [-n N|-c N] FILE");
    struct bb_reader r;
    if (bb_open(&r,sh,sh->argv[i])) return m_usage(sh,"tail: cannot open file");
    uint32_t start=0, size=r.length;
    if (!count || !size) return 0;
    if (bytes) start=count>=size?0:size-count;
    else {
        unsigned lines=0;
        uint32_t end=size;
        /* Ignore the trailing LF only for line counting, not output. */
        unsigned char last;
        if (sh->edl.read(r.path,size-1,&last,1)!=1) return 1;
        if (last=='\n') end--;
        while (end) {
            unsigned n=end>sizeof r.buffer?sizeof r.buffer:end;
            uint32_t begin=end-n;
            if (sh->edl.read(r.path,begin,r.buffer,n)!=(int)n) return 1;
            for (unsigned j=n;j;) {
                if (r.buffer[--j]=='\n' && ++lines>=count) {
                    start=begin+j+1;
                    end=0;break;
                }
            }
            if (end) end=begin;
        }
    }
    while (start<size) {
        unsigned n=size-start;
        if (n>sizeof r.buffer) n=sizeof r.buffer;
        if (sh->edl.read(r.path,start,r.buffer,n)!=(int)n ||
            sh->console_write(r.buffer,n)!=(int)n) return 1;
        start+=n;
    }
    return 0;
}
int bb_text_main(const struct shell_api *sh)
{
    if (m_equal(sh->command,"basename")) return bb_basename(sh);
    if (m_equal(sh->command,"dirname")) return bb_dirname(sh);
    if (m_equal(sh->command,"uniq")) return bb_uniq(sh);
    if (m_equal(sh->command,"sort")) return bb_sort(sh);
    if (m_equal(sh->command,"tail")) return bb_tail(sh);
    return 127;
}
