/* EDL-only file-oriented BusyBox-style tools. Original implementations:
   no stdin/pipes, no GNU libbb dependencies, bounded line length 255 bytes.
   The separate BusyBox-derived ports remain in busybox_text.c. */
#include "utils_util.h"
#include <string.h>

/* Buffer writes to avoid repainting the whole screen for each character. */
struct u_buffer { const struct shell_api *sh; char bytes[128]; unsigned n; };
static int flush(struct u_buffer *w)
{
    if(!w->n)return 0;
    int rc=u_out(w->sh,w->bytes,w->n);
    w->n=0;return rc;
}
static int put(struct u_buffer *w,int c)
{
    if(w->n==sizeof w->bytes && flush(w))return -1;
    w->bytes[w->n++]=(char)c;
    return 0;
}
static int open_single(const struct shell_api *sh,struct u_reader *r,
                       const char *usage)
{
    if(sh->argc!=2)return m_usage(sh,usage);
    if(u_open(r,sh,sh->argv[1]))return m_usage(sh,"cannot open file");
    return 0;
}
static int app_rev(const struct shell_api *sh)
{
    struct u_reader r;
    int rc=open_single(sh,&r,"usage: rev FILE");
    if(rc)return rc;
    char line[U_LINE];
    while((rc=u_line(&r,line))==1) {
        unsigned n=(unsigned)strlen(line);
        int lf=n && line[n-1]=='\n';
        unsigned m=n-(unsigned)lf;
        char out[U_LINE];
        for(unsigned i=0;i<m;i++)out[i]=line[m-1-i];
        if(lf)out[m++]='\n';
        if(u_out(sh,out,m))return 1;
    }
    return rc<0?m_usage(sh,"rev: input line exceeds 255 bytes"):0;
}
static int range_out(const struct shell_api *sh,const struct u_reader *r,
                     uint32_t from,uint32_t to)
{
    unsigned char buf[128];
    while(from<to) {
        unsigned n=to-from;
        if(n>sizeof buf)n=sizeof buf;
        if(sh->edl.read(r->path,from,buf,n)!=(int)n ||
           u_out(sh,buf,n))return -1;
        from+=n;
    }
    return 0;
}
static int app_tac(const struct shell_api *sh)
{
    struct u_reader r;
    int rc=open_single(sh,&r,"usage: tac FILE");
    if(rc)return rc;
    if(!r.size)return 0;
    unsigned char end_byte;
    if(sh->edl.read(r.path,r.size-1,&end_byte,1)!=1)return 1;
    int has_lf=end_byte=='\n';
    uint32_t scan=r.size-(unsigned)has_lf, end=r.size;
    unsigned char buf[128];
    int first=1;
    while(scan) {
        unsigned n=scan;
        if(n>sizeof buf)n=sizeof buf;
        uint32_t begin=scan-n;
        if(sh->edl.read(r.path,begin,buf,n)!=(int)n)return 1;
        for(unsigned j=n;j;) {
            if(buf[--j]!='\n')continue;
            uint32_t cut=begin+j+1;
            if(range_out(sh,&r,cut,end))return 1;
            if(first && !has_lf && u_char(sh,'\n'))return 1;
            first=0;
            end=cut;
        }
        scan=begin;
    }
    return range_out(sh,&r,0,end)?1:0;
}
static int app_nl(const struct shell_api *sh)
{
    int number_blank=0,i=1;
    if(i<sh->argc && (m_equal(sh->argv[i],"-ba")||
                        m_equal(sh->argv[i],"-bt"))) {
        number_blank=m_equal(sh->argv[i],"-ba");i++;
    }
    if(sh->argc-i!=1)return m_usage(sh,"usage: nl [-ba|-bt] FILE");
    struct u_reader r;
    if(u_open(&r,sh,sh->argv[i]))return m_usage(sh,"nl: cannot open file");
    char line[U_LINE];unsigned number=0;
    int rc;
    while((rc=u_line(&r,line))==1) {
        if(number_blank || (line[0]!='\n' && line[0])) {
            m_num(sh,++number);
            if(u_char(sh,'\t'))return 1;
        }
        if(u_out(sh,line,(unsigned)strlen(line)))return 1;
    }
    return rc<0?m_usage(sh,"nl: input line exceeds 255 bytes"):0;
}
static int selection(const char *p,unsigned char picks[256])
{
    memset(picks,0,256);
    if(!p || !*p)return -1;
    for(;;) {
        unsigned a=0,b=0;
        if(*p<'0'||*p>'9')return -1;
        while(*p>='0'&&*p<='9') {
            unsigned digit=(unsigned)(*p++-'0');
            if(a>(255u-digit)/10u)return -1;
            a=a*10u+digit;
        }
        if(!a)return -1;
        b=a;
        if(*p=='-') {
            p++;
            if(*p>='0'&&*p<='9') {
                b=0;
                while(*p>='0'&&*p<='9') {
                    unsigned digit=(unsigned)(*p++-'0');
                    if(b>(255u-digit)/10u)return -1;
                    b=b*10u+digit;
                }
                if(b<a)return -1;
            }else b=255;
        }
        for(unsigned k=a;k<=b;k++)picks[k]=1;
        if(!*p)return 0;
        if(*p++!=',' || !*p)return -1;
    }
}
static int app_cut(const struct shell_api *sh)
{
    int mode=0,i=1;
    char delim='\t';
    const char *selected=NULL;
    for(;i<sh->argc && sh->argv[i][0]=='-' && sh->argv[i][1];i++) {
        const char *p=sh->argv[i];
        if(m_equal(p,"--")) {i++;break;}
        char opt=p[1];
        if(opt=='c'||opt=='b'||opt=='f') {
            mode=opt=='f'?2:1;
            selected=p+2;
            if(!*selected) {
                if(++i>=sh->argc)return m_usage(sh,"cut: missing list");
                selected=sh->argv[i];
            }
        }else if(opt=='d') {
            const char *d=p+2;
            if(!*d) {
                if(++i>=sh->argc)return m_usage(sh,"cut: missing delimiter");
                d=sh->argv[i];
            }
            if(!d[0]||d[1])return m_usage(sh,"cut: delimiter must be one byte");
            delim=d[0];
        }else return m_usage(sh,"usage: cut -c LIST|-f LIST [-d CHAR] FILE");
    }
    if(!mode || !selected || sh->argc-i!=1)
        return m_usage(sh,"usage: cut -c LIST|-f LIST [-d CHAR] FILE");
    unsigned char picks[256];
    if(selection(selected,picks))return m_usage(sh,"cut: invalid list (1..255)");
    struct u_reader r;
    if(u_open(&r,sh,sh->argv[i]))return m_usage(sh,"cut: cannot open file");
    char line[U_LINE],out[U_LINE];int rc;
    while((rc=u_line(&r,line))==1) {
        unsigned len=(unsigned)strlen(line),n=0;
        int lf=len && line[len-1]=='\n';
        if(lf)len--;
        if(mode==1) {
            for(unsigned j=0;j<len;j++)if(picks[j+1])out[n++]=line[j];
        }else {
            unsigned field=1,begin=0;int has_delim=0;
            for(unsigned j=0;j<len;j++)if(line[j]==delim)has_delim=1;
            if(!has_delim) {
                memcpy(out,line,len);n=len;
            }else for(unsigned j=0;j<=len;j++) {
                if(j==len || line[j]==delim) {
                    if(field<256 && picks[field]) {
                        if(n)out[n++]=delim;
                        if(j>begin) {memcpy(out+n,line+begin,j-begin);n+=j-begin;}
                    }
                    begin=j+1;field++;
                }
            }
        }
        if(lf)out[n++]='\n';
        if(u_out(sh,out,n))return 1;
    }
    return rc<0?m_usage(sh,"cut: input line exceeds 255 bytes"):0;
}
static int decode_set_char(const char **pp)
{
    const char *p=*pp;
    if(!*p)return -1;
    int c=(unsigned char)*p++;
    if(c=='\\' && *p) {
        c=(unsigned char)*p++;
        if(c=='n')c='\n';
        else if(c=='r')c='\r';
        else if(c=='t')c='\t';
        else if(c=='0')c=0;
    }
    *pp=p;
    return c;
}
static int make_set(const char *p,unsigned char out[128],unsigned *count)
{
    unsigned n=0;
    while(*p) {
        int first=decode_set_char(&p);
        if(first<0)return -1;
        if(*p=='-' && p[1]) {
            p++;
            int last=decode_set_char(&p);
            if(last<first)return -1;
            for(int c=first;c<=last;c++) {
                if(n>=128)return -1;
                out[n++]=(unsigned char)c;
            }
        }else {
            if(n>=128)return -1;
            out[n++]=(unsigned char)first;
        }
    }
    *count=n;return n?0:-1;
}
static int app_tr(const struct shell_api *sh)
{
    int i=1,delete_mode=0,squeeze=0;
    if(i<sh->argc && sh->argv[i][0]=='-' && sh->argv[i][1]) {
        if(m_equal(sh->argv[i],"-d"))delete_mode=1;
        else if(m_equal(sh->argv[i],"-s"))squeeze=1;
        else return m_usage(sh,"usage: tr [-d|-s] SET1 [SET2] FILE");
        i++;
    }
    int nargs=sh->argc-i;
    if(nargs!=(delete_mode||squeeze?2:3))
        return m_usage(sh,"usage: tr [-d|-s] SET1 [SET2] FILE");
    unsigned char from[128],to[128],mapping[256],affected[256]={0};
    unsigned nf=0,nt=0;
    if(make_set(sh->argv[i],from,&nf))return m_usage(sh,"tr: invalid SET1");
    for(unsigned j=0;j<256;j++)mapping[j]=(unsigned char)j;
    if(!delete_mode && !squeeze) {
        if(make_set(sh->argv[i+1],to,&nt))return m_usage(sh,"tr: invalid SET2");
        for(unsigned j=0;j<nf;j++)mapping[from[j]]=to[j<nt?j:nt-1];
    }
    for(unsigned j=0;j<nf;j++)affected[from[j]]=1;
    struct u_reader r;
    if(u_open(&r,sh,sh->argv[sh->argc-1]))
        return m_usage(sh,"tr: cannot open file");
    struct u_buffer w={.sh=sh};
    int c,last=-1;
    while((c=u_get(&r))>=0) {
        if(delete_mode && affected[c])continue;
        if(squeeze && affected[c] && c==last)continue;
        int transformed=mapping[c];
        if(put(&w,transformed))return 1;
        last=c;
    }
    return c==-2 || flush(&w)?m_usage(sh,"tr: read/write failed"):0;
}
static int app_strings(const struct shell_api *sh)
{
    int i=1;unsigned minimum=4;
    if(i<sh->argc && (m_equal(sh->argv[i],"-n") ||
                       !strncmp(sh->argv[i],"-n",2))) {
        const char *arg=sh->argv[i]+2;
        if(!*arg) {
            if(++i>=sh->argc)return m_usage(sh,"strings: missing minimum");
            arg=sh->argv[i];
        }
        if(u_uint(arg,&minimum)||minimum<1||minimum>64)
            return m_usage(sh,"strings: minimum must be 1..64");
        i++;
    }
    if(sh->argc-i!=1)return m_usage(sh,"usage: strings [-n N] FILE");
    struct u_reader r;
    if(u_open(&r,sh,sh->argv[i]))return m_usage(sh,"strings: cannot open file");
    struct u_buffer w={.sh=sh};
    char pre[64];unsigned n=0;int active=0,c;
    while((c=u_get(&r))>=0) {
        if(c>=32 && c<=126) {
            if(!active) {
                pre[n++]=(char)c;
                if(n>=minimum) {
                    for(unsigned j=0;j<n;j++)if(put(&w,pre[j]))return 1;
                    active=1;
                }
            }else if(put(&w,c))return 1;
        }else {
            if(active && put(&w,'\n'))return 1;
            active=0;n=0;
        }
    }
    if(active && put(&w,'\n'))return 1;
    return c==-2 || flush(&w)?m_usage(sh,"strings: read/write failed"):0;
}
static int cmp_lines(const char *a,const char *b)
{
    return strcmp(a,b);
}
static int app_comm(const struct shell_api *sh)
{
    int i=1;unsigned skip=0;
    for(;i<sh->argc && sh->argv[i][0]=='-' && sh->argv[i][1];i++) {
        const char *p=sh->argv[i]+1;
        while(*p) {
            if(*p<'1' || *p>'3')return m_usage(sh,"usage: comm [-123] FILE1 FILE2");
            skip|=1u<<(*p++-'1');
        }
    }
    if(sh->argc-i!=2)return m_usage(sh,"usage: comm [-123] FILE1 FILE2");
    struct u_reader a,b;
    if(u_open(&a,sh,sh->argv[i]) || u_open(&b,sh,sh->argv[i+1]))
        return m_usage(sh,"comm: cannot open file");
    char left[U_LINE],right[U_LINE];
    int x=u_line(&a,left),y=u_line(&b,right);
    while(x==1 || y==1) {
        if(x<0||y<0)return m_usage(sh,"comm: invalid/long input");
        int column;
        if(y==0 || (x==1 && cmp_lines(left,right)<0))column=0;
        else if(x==0 || cmp_lines(left,right)>0)column=1;
        else column=2;
        if(!(skip&(1u<<column))) {
            for(int n=0;n<column;n++)if(!(skip&(1u<<n)) && u_char(sh,'\t'))return 1;
            const char *line=column==1?right:left;
            if(u_out(sh,line,(unsigned)strlen(line)))return 1;
            if(line[0] && line[strlen(line)-1]!='\n' && u_char(sh,'\n'))return 1;
        }
        if(column!=1)x=u_line(&a,left);
        if(column!=0)y=u_line(&b,right);
    }
    return x<0||y<0?m_usage(sh,"comm: invalid/long input"):0;
}
static int app_paste(const struct shell_api *sh)
{
    char delim='\t';int i=1;
    if(i<sh->argc && sh->argv[i][0]=='-' &&
       sh->argv[i][1]=='d') {
        const char *d=sh->argv[i]+2;
        if(!*d) {
            if(++i>=sh->argc)return m_usage(sh,"paste: delimiter missing");
            d=sh->argv[i];
        }
        if(!d[0]||d[1])return m_usage(sh,"paste: delimiter must be one byte");
        delim=d[0];i++;
    }
    unsigned count=(unsigned)(sh->argc-i);
    if(count<2 || count>4)return m_usage(sh,"usage: paste [-d CHAR] FILE1 FILE2 [FILE3 FILE4]");
    struct u_reader files[4];
    for(unsigned j=0;j<count;j++)
        if(u_open(&files[j],sh,sh->argv[i+(int)j]))
            return m_usage(sh,"paste: cannot open file");
    char lines[4][U_LINE];
    int exists[4];
    for(;;) {
        int any=0;
        for(unsigned j=0;j<count;j++) {
            exists[j]=u_line(&files[j],lines[j]);
            if(exists[j]<0)return m_usage(sh,"paste: invalid/long input");
            if(exists[j])any=1;
        }
        if(!any)return 0; /* Do not print an empty final delimiter row. */
        for(unsigned j=0;j<count;j++) {
            if(j && u_char(sh,delim))return 1;
            if(exists[j]) {
                unsigned n=(unsigned)strlen(lines[j]);
                if(n && lines[j][n-1]=='\n')n--;
                if(u_out(sh,lines[j],n))return 1;
            }
        }
        if(u_char(sh,'\n'))return 1;
    }
}
static int app_fold(const struct shell_api *sh)
{
    int i=1;unsigned width=64;
    if(i<sh->argc && !strncmp(sh->argv[i],"-w",2)) {
        const char *p=sh->argv[i]+2;
        if(!*p) {
            if(++i>=sh->argc)return m_usage(sh,"fold: width missing");
            p=sh->argv[i];
        }
        if(u_uint(p,&width)||!width||width>64)
            return m_usage(sh,"fold: width must be 1..64");
        i++;
    }
    if(sh->argc-i!=1)return m_usage(sh,"usage: fold [-w WIDTH] FILE");
    struct u_reader r;
    if(u_open(&r,sh,sh->argv[i]))return m_usage(sh,"fold: cannot open file");
    struct u_buffer w={.sh=sh};
    unsigned col=0;int c;
    while((c=u_get(&r))>=0) {
        if(c=='\n') {if(put(&w,c))return 1;col=0;continue;}
        if(col>=width) {
            if(put(&w,'\n'))return 1;
            col=0;
        }
        if(put(&w,c))return 1;
        col++;
    }
    return c==-2||flush(&w)?m_usage(sh,"fold: read/write failed"):0;
}
static int app_expand(const struct shell_api *sh)
{
    int i=1;unsigned tab=8;
    if(i<sh->argc && !strncmp(sh->argv[i],"-t",2)) {
        const char *p=sh->argv[i]+2;
        if(!*p) {
            if(++i>=sh->argc)return m_usage(sh,"expand: tab size missing");
            p=sh->argv[i];
        }
        if(u_uint(p,&tab)||!tab||tab>16)
            return m_usage(sh,"expand: tab size must be 1..16");
        i++;
    }
    if(sh->argc-i!=1)return m_usage(sh,"usage: expand [-t N] FILE");
    struct u_reader r;
    if(u_open(&r,sh,sh->argv[i]))return m_usage(sh,"expand: cannot open file");
    struct u_buffer w={.sh=sh};
    unsigned col=0;int c;
    while((c=u_get(&r))>=0) {
        if(c=='\t') {
            unsigned n=tab-(col%tab);
            while(n--) {if(put(&w,' '))return 1;col++;}
        }else {
            if(put(&w,c))return 1;
            if(c=='\n'||c=='\r')col=0;
            else col++;
        }
    }
    return c==-2||flush(&w)?m_usage(sh,"expand: read/write failed"):0;
}
int utils_text_main(const struct shell_api *sh)
{
    if(m_equal(sh->command,"rev"))return app_rev(sh);
    if(m_equal(sh->command,"tac"))return app_tac(sh);
    if(m_equal(sh->command,"nl"))return app_nl(sh);
    if(m_equal(sh->command,"cut"))return app_cut(sh);
    if(m_equal(sh->command,"tr"))return app_tr(sh);
    if(m_equal(sh->command,"strings"))return app_strings(sh);
    if(m_equal(sh->command,"comm"))return app_comm(sh);
    if(m_equal(sh->command,"paste"))return app_paste(sh);
    if(m_equal(sh->command,"fold"))return app_fold(sh);
    if(m_equal(sh->command,"expand"))return app_expand(sh);
    return 127;
}
