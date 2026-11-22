#include "shell_parse.h"
#include "shell_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static void test_parse(const char *input, int expected, const char *a, const char *b,
                       const char *c)
{
    char line[256]; const char *argv[SHELL_MAX_ARGS]={0};
    strcpy(line,input);
    int rc=shell_parse(line,argv);
    if (rc!=expected) {fprintf(stderr,"parse '%s': got %d expected %d\n",input,rc,expected);abort();}
    if (expected>0) assert(!strcmp(argv[0],a));
    if (expected>1) assert(!strcmp(argv[1],b));
    if (expected>2) assert(!strcmp(argv[2],c));
}
static void be16(unsigned char *p,unsigned n) {p[0]=n>>8;p[1]=n;}
static void be32(unsigned char *p,unsigned n) {p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static unsigned char elf[1024];
static int mock_read(void *ignored,uint32_t at,void *out,size_t n)
{
    (void)ignored;
    if (at>sizeof elf || n>sizeof elf-at) return -1;
    memcpy(out,elf+at,n);return 0;
}
static void fixture(void)
{
    memset(elf,0,sizeof elf);
    memcpy(elf,"\177ELF\1\2\1",7);
    be16(elf+16,2);be16(elf+18,42);be16(elf+46,40);
    be32(elf+32,64);be16(elf+48,3);be16(elf+50,1);
    /* .shstrtab */
    unsigned char *shstr=elf+64+40;
    be32(shstr+4,3);be32(shstr+16,250);be32(shstr+20,21);
    memcpy(elf+250,"\0.shstrtab\0.edl.cmd\0",21);
    /* .edl.cmd */
    unsigned char *cmd=elf+64+80;
    be32(cmd,11);be32(cmd+4,1);be32(cmd+16,300);
    be32(cmd+20,sizeof(struct shell_manifest));
    be32(elf+300,SHELL_MANIFEST_MAGIC);
    be32(elf+304,SHELL_MANIFEST_VERSION);
    be32(elf+308,3);
    memcpy(elf+312,"ls",3);memcpy(elf+328,"cat",4);memcpy(elf+344,"echo",5);
}
int main(void)
{
    test_parse("ls",1,"ls",NULL,NULL);
    test_parse("  cp 'a file' \"b file\"  ",3,"cp","a file","b file");
    test_parse("echo hello\\ world",2,"echo","hello world",NULL);
    test_parse("printf '%s\\n' hello",3,"printf","%s\\n","hello");
    test_parse("printf \"%s\\n\" hello",3,"printf","%s\\n","hello");
    test_parse("echo \\some\\path",2,"echo","\\some\\path",NULL);
    test_parse("echo 'a\\ b'",2,"echo","a\\ b",NULL);
    test_parse("echo \"a\\\"b\"",2,"echo","a\"b",NULL);
    test_parse("echo \"\" xyz",3,"echo","","xyz");
    test_parse("echo 'unterminated",-1,NULL,NULL,NULL);
    test_parse("",0,NULL,NULL,NULL);
    fixture();
    struct shell_manifest m;
    assert(shell_manifest_read(mock_read,NULL,sizeof elf,&m)==0);
    assert(m.count==3 && !strcmp(m.names[0],"ls") && !strcmp(m.names[1],"cat"));
    be32(elf+308,SHELL_MAX_COMMANDS+1);
    assert(shell_manifest_read(mock_read,NULL,sizeof elf,&m)<0);
    fixture();memcpy(elf+328,"ls",3);
    assert(shell_manifest_read(mock_read,NULL,sizeof elf,&m)<0);
    fixture();be32(elf+64+80+16,1020);
    assert(shell_manifest_read(mock_read,NULL,sizeof elf,&m)<0);
    fixture();elf[18]=0;elf[19]=3;
    assert(shell_manifest_read(mock_read,NULL,sizeof elf,&m)<0);
    puts("PASS: command parser and ELF manifest safety tests");
    return 0;
}
