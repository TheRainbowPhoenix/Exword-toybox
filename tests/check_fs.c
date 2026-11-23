#include "host_fs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct fake { char path[128]; char data[256]; int size, dir, alive; };
static struct fake files[12];
static unsigned count;
static char query[128];
static int scanning, opened=-1, position;
static unsigned create_calls, delete_calls, write_calls;

static int find(const char *path)
{
    for (unsigned i=0;i<count;i++)
        if (files[i].alive && !strcmp(files[i].path,path)) return (int)i;
    return -1;
}
static int match_parent(const char *path,const char *dir)
{
    size_t n=strlen(dir);
    return !strncmp(path,dir,n) && path[n]=='\\' &&
           path[n+1] && !strchr(path+n+1,'\\');
}
static int next_child(char *name, unsigned long *kind)
{
    for (unsigned i=(unsigned)scanning;i<count;i++) {
        if (files[i].alive && match_parent(files[i].path,query)) {
            strcpy(name,strrchr(files[i].path,'\\')+1);
            *kind=files[i].dir?5:1;
            scanning=(int)i+1;
            return 0;
        }
    }
    return -16;
}
int exword_sys_findfirst(const char *pattern,int *fd,char *name,unsigned long *kind)
{
    size_t n=strlen(pattern);
    assert(n>=2 && pattern[n-2]=='\\' && pattern[n-1]=='*');
    assert(n-2<sizeof query);
    memcpy(query,pattern,n-2);query[n-2]=0;
    scanning=0;*fd=7;
    return next_child(name,kind);
}
int exword_sys_findnext(int fd,char *name,unsigned long *kind)
{assert(fd==7);return next_child(name,kind);}
int exword_sys_findclose(int fd) {assert(fd==7);return 0;}
int exword_sys_create(const char *path,int type)
{
    create_calls++;
    if (type!=1 && type!=5) return -1;
    if (find(path)>=0 || count>=12) return -1;
    struct fake *f=&files[count++];
    memset(f,0,sizeof *f);strcpy(f->path,path);
    f->alive=1;f->dir=type==5;
    return 0;
}
int exword_sys_open(const char *path,int mode)
{
    assert(mode==1||mode==3);
    int i=find(path);
    if (i<0 || files[i].dir) return -1;
    opened=i;position=0;
    return i+3;
}
int exword_sys_close(int fd)
{assert(fd==opened+3);opened=-1;return 0;}
int exword_sys_get_filesize(int fd)
{assert(fd==opened+3);return files[opened].size;}
int exword_sys_seek(int fd,int off,int whence)
{assert(fd==opened+3 && whence==0);position=off;return off;}
int exword_sys_read(int fd,void *dest,int n)
{
    assert(fd==opened+3);
    if (position+n>files[opened].size) return -1;
    memcpy(dest,files[opened].data+position,n);position+=n;
    return n;
}
int exword_sys_write(int fd,const void *data,int n)
{
    assert(fd==opened+3);
    write_calls++;
    if (position+n>256) return -1;
    memcpy(files[opened].data+position,data,n);
    position+=n;
    if (position>files[opened].size) files[opened].size=position;
    return n;
}
int exword_sys_delete(const char *path)
{
    int i=find(path);
    if (i<0) return -1;
    if (files[i].dir)
        for (unsigned j=0;j<count;j++)
            if (files[j].alive && match_parent(files[j].path,path)) return -1;
    delete_calls++;
    files[i].alive=0;
    return 0;
}
int exword_sys_rename(const char *a,const char *b)
{
    int i=find(a);
    if (i<0 || find(b)>=0) return -1;
    strcpy(files[i].path,b);
    return 0;
}
int main(void)
{
    const char *folder="\\\\drv0\\FOLDER";
    const char *file="\\\\drv0\\FOLDER\\abc.txt";
    assert(host_fs_create(folder,1)==0);
    assert(files[0].dir);
    assert(host_fs_dir_exists(folder)==1);
    char actual[EDL_PATH_MAX]={0};
    assert(host_fs_find_dir_child("\\\\drv0","folder",actual)==0);
    assert(!strcmp(actual,folder));
    assert(host_fs_find_dir_child("\\\\drv0","missing",actual)<0);
    assert(host_fs_create(file,0)==0);
    assert(host_fs_write(file,0,"abc",3,SHELL_WRITE_TRUNC|SHELL_WRITE_CREATE)==3);
    assert(host_fs_write(file,3,"def",3,0)==3);
    assert(host_fs_size(file)==6);
    assert(!memcmp(files[find(file)].data,"abcdef",6));
    struct shell_dirent rows[4];
    assert(host_fs_list(folder,0,rows,4)==1);
    assert(!strcmp(rows[0].name,"abc.txt") && !rows[0].directory);
    unsigned writes=write_calls,creates=create_calls;
    assert(host_fs_write("\\\\drv0\\FOLDER\\longfilename.txt",0,"x",1,SHELL_WRITE_CREATE)<0);
    assert(host_fs_create("\\\\drv0\\FOLDER\\longfilename.txt",0)<0);
    assert(host_fs_create("\\\\drv0\\1234567890123",1)<0);
    assert(writes==write_calls && creates==create_calls);
    assert(host_fs_rename(file,"\\\\drv0\\FOLDER\\new.txt")==0);
    assert(host_fs_size("\\\\drv0\\FOLDER\\new.txt")==6);
    assert(host_fs_delete(folder)<0); /* Native directory is not empty. */
    assert(host_fs_delete("\\\\drv0\\FOLDER\\new.txt")==0);
    assert(host_fs_delete(folder)==0);
    assert(delete_calls>=3);
    puts("PASS: native EX-word create/read-write/rename/delete and 8.3 boundaries");
}
