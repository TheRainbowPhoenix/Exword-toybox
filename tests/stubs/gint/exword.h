#ifndef TEST_EXWORD_H
#define TEST_EXWORD_H
int exword_sys_open(const char*,int);int exword_sys_close(int);
int exword_sys_get_filesize(int);
int exword_sys_findfirst(const char*,int*,char*,unsigned long*);
int exword_sys_findnext(int,char*,unsigned long*);
int exword_sys_findclose(int);
int exword_sys_seek(int,int,int);
int exword_sys_read(int,void*,int);
int exword_sys_write(int,const void*,int);
int exword_sys_create(const char*,int);
int exword_sys_delete(const char*);
int exword_sys_rename(const char*,const char*);
#endif
