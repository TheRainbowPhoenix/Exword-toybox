#include "shell_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
int utils_edl_main(const struct edl_api *);

struct fixture {const char *path;const unsigned char *data;unsigned size;int directory;};
static const unsigned char a[]="alpha\nbeta\nbeta\ngamma\n";
static const unsigned char b[]="beta\ngamma\nzeta\n";
static const unsigned char pairs[]="ab:cd:ef\nxx:yy:zz\n";
static const unsigned char sparse[]="a::c\n";
static const unsigned char tabbed[]="a\tb\n";
static const unsigned char one[]="abc\n";
static const unsigned char longer[]="abcdef\n";
static const unsigned char nonl[]="one\ntwo";
static const unsigned char binary[]={0,'x','x',0,'H','E','L','L','O',0};
static const unsigned char sub_a[]="ab\n";
static const unsigned char sub_b[]="wxyz";
static const struct fixture fs[]={
    {"\\\\drv0\\a.txt",a,sizeof a-1,0},
    {"\\\\drv0\\b.txt",b,sizeof b-1,0},
    {"\\\\drv0\\pairs.txt",pairs,sizeof pairs-1,0},
    {"\\\\drv0\\sparse.txt",sparse,sizeof sparse-1,0},
    {"\\\\drv0\\tab.txt",tabbed,sizeof tabbed-1,0},
    {"\\\\drv0\\one.txt",one,sizeof one-1,0},
    {"\\\\drv0\\long.txt",longer,sizeof longer-1,0},
    {"\\\\drv0\\nonl.txt",nonl,sizeof nonl-1,0},
    {"\\\\drv0\\blob.dat",binary,sizeof binary,0},
    {"\\\\drv0\\DIR",NULL,0,1},
    {"\\\\drv0\\DIR\\A.TXT",sub_a,sizeof sub_a-1,0},
    {"\\\\drv0\\DIR\\SUB",NULL,0,1},
    {"\\\\drv0\\DIR\\SUB\\B.TXT",sub_b,sizeof sub_b-1,0}
};
static struct shell_api sh;
static char out[20000];
static unsigned outlen;
static const struct fixture *lookup(const char *path)
{
    for(unsigned i=0;i<sizeof fs/sizeof fs[0];i++)
        if(!strcmp(fs[i].path,path))return &fs[i];
    return NULL;
}
static int resolve(const char *src,char path[EDL_PATH_MAX])
{
    int n=snprintf(path,EDL_PATH_MAX,"\\\\drv0\\%s",src);
    return n<0 || n>=EDL_PATH_MAX?-1:0;
}
static int size(const char *path)
{
    const struct fixture *f=lookup(path);
    return f && !f->directory?(int)f->size:-1;
}
static int read_file(const char *path,uint32_t off,void *dst,uint32_t n)
{
    const struct fixture *f=lookup(path);
    if(!f || f->directory || off>f->size || n>f->size-off)return -1;
    if(n)memcpy(dst,f->data+off,n);
    return (int)n;
}
static int list(const char *path,unsigned first,struct shell_dirent *items,unsigned cap)
{
    const char *names[2]={NULL,NULL};
    int directories[2]={0,0},total=0;
    if(!strcmp(path,"\\\\drv0\\DIR")) {
        names[0]="A.TXT";names[1]="SUB";directories[1]=1;total=2;
    }else if(!strcmp(path,"\\\\drv0\\DIR\\SUB")){
        names[0]="B.TXT";total=1;
    }else return -1;
    for(unsigned i=0;i<cap && first+i<(unsigned)total;i++) {
        memset(items+i,0,sizeof items[i]);
        strcpy(items[i].name,names[first+i]);
        items[i].directory=directories[first+i];
    }
    return total;
}
static int output(const void *bytes,unsigned n)
{
    assert(outlen+n<sizeof out);
    if(n)memcpy(out+outlen,bytes,n);
    outlen+=n;out[outlen]=0;return (int)n;
}
static int run(const char *command,int argc,const char *const args[])
{
    outlen=0;out[0]=0;
    sh.command=command;sh.argc=argc;sh.argv=args;
    return utils_edl_main(&sh.edl);
}
int main(void)
{
    sh.edl.magic=EDL_ABI_MAGIC;
    sh.edl.version=EDL_ABI_VERSION;
    sh.edl.size=sizeof sh;
    sh.shell_magic=SHELL_SERVICE_MAGIC;
    sh.shell_version=SHELL_SERVICE_VERSION;
    sh.edl.read=read_file;
    sh.console_write=output;
    sh.resolve_path=resolve;
    sh.file_size=size;
    sh.list_dir=list;

    const char *rev[]={"rev","one.txt"};
    assert(run("rev",2,rev)==0 && !strcmp(out,"cba\n"));
    const char *tac[]={"tac","a.txt"};
    assert(run("tac",2,tac)==0 && !strcmp(out,"gamma\nbeta\nbeta\nalpha\n"));
    const char *tac_nonl[]={"tac","nonl.txt"};
    assert(run("tac",2,tac_nonl)==0 && !strcmp(out,"two\none\n"));
    const char *nl[]={"nl","-ba","one.txt"};
    assert(run("nl",3,nl)==0 && !strcmp(out,"1\tabc\n"));
    const char *cut[]={"cut","-d",":","-f","1,3","pairs.txt"};
    assert(run("cut",6,cut)==0 && !strcmp(out,"ab:ef\nxx:zz\n"));
    const char *cut_empty[]={"cut","-d",":","-f","2,3","sparse.txt"};
    assert(run("cut",6,cut_empty)==0 && !strcmp(out,":c\n"));
    const char *cut_chars[]={"cut","-c1-2","one.txt"};
    assert(run("cut",3,cut_chars)==0 && !strcmp(out,"ab\n"));
    const char *cut_bad[]={"cut","-c0","one.txt"};
    assert(run("cut",3,cut_bad)!=0);
    const char *tr[]={"tr","a-z","A-Z","one.txt"};
    assert(run("tr",4,tr)==0 && !strcmp(out,"ABC\n"));
    const char *tr_del[]={"tr","-d","b","one.txt"};
    assert(run("tr",4,tr_del)==0 && !strcmp(out,"ac\n"));
    const char *strings[]={"strings","blob.dat"};
    assert(run("strings",2,strings)==0 && !strcmp(out,"HELLO\n"));
    const char *strings_n[]={"strings","-n2","blob.dat"};
    assert(run("strings",3,strings_n)==0 && !strcmp(out,"xx\nHELLO\n"));
    const char *comm[]={"comm","-12","a.txt","b.txt"};
    assert(run("comm",4,comm)==0 && !strcmp(out,"beta\ngamma\n"));
    const char *paste[]={"paste","a.txt","b.txt"};
    assert(run("paste",3,paste)==0 &&
           !strcmp(out,"alpha\tbeta\nbeta\tgamma\nbeta\tzeta\ngamma\t\n"));
    const char *fold[]={"fold","-w3","long.txt"};
    assert(run("fold",3,fold)==0 && !strcmp(out,"abc\ndef\n"));
    const char *expand[]={"expand","-t4","tab.txt"};
    assert(run("expand",3,expand)==0 && !strcmp(out,"a   b\n"));
    const char *seq[]={"seq","1","2","5"};
    assert(run("seq",4,seq)==0 && !strcmp(out,"1\n3\n5\n"));
    const char *seq_neg[]={"seq","-1","1","1"};
    assert(run("seq",4,seq_neg)==0 && !strcmp(out,"-1\n0\n1\n"));
    const char *cmp_same[]={"cmp","-s","a.txt","a.txt"};
    assert(run("cmp",4,cmp_same)==0 && !*out);
    const char *cmp_diff[]={"cmp","a.txt","b.txt"};
    assert(run("cmp",3,cmp_diff)==1 && strstr(out,"byte 1"));
    const char *cksum[]={"cksum","one.txt"};
    assert(run("cksum",2,cksum)==0 && !strcmp(out,"1112837078 4 one.txt\n"));
    const char *stat[]={"stat","one.txt"};
    assert(run("stat",2,stat)==0 && strstr(out,"Type: regular file") &&
           strstr(out,"Size: 4 bytes"));
    const char *du[]={"du","DIR"};
    assert(run("du",2,du)==0 && strstr(out,"4 \\\\drv0\\DIR\\SUB") &&
           strstr(out,"7 \\\\drv0\\DIR\n"));
    const char *find[]={"find","DIR","-name","*.TXT","-type","f"};
    assert(run("find",6,find)==0 &&
           strstr(out,"\\\\drv0\\DIR\\A.TXT\n") &&
           strstr(out,"\\\\drv0\\DIR\\SUB\\B.TXT\n"));
    const char *printf_args[]={"printf","X%s:%d:%x\\n","hi","-3","15"};
    assert(run("printf",5,printf_args)==0 && !strcmp(out,"Xhi:-3:f\n"));
    const char *printf_invalid[]={"printf","%q","x"};
    assert(run("printf",3,printf_invalid)!=0);
    const char *false_cmd[]={"false"};
    assert(run("false",1,false_cmd)==1);
    puts("PASS: 18 extra BusyBox-style EDL applets and native read-only I/O");
}
