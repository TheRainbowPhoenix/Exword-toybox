#ifndef TEST_EDL_H
#define TEST_EDL_H
#include <stdint.h>
#include <stddef.h>
#define EDL_PATH_MAX 256
#define EDL_ABI_MAGIC 0x45444c31u
#define EDL_ABI_VERSION 1
enum { EDL_UP=1, EDL_DOWN, EDL_LEFT, EDL_RIGHT, EDL_OK, EDL_BACK, EDL_REFRESH, EDL_TOUCH, EDL_HOME };
struct edl_event { int key,x,y; };
struct edl_status { uint16_t year; uint8_t month,day,hours,minutes,seconds; int8_t battery; };
struct edl_record {char path[EDL_PATH_MAX]; char title[32];};
struct edl_metadata {uint32_t magic,version;char title[32],description[64];uint16_t icon[1024];};
struct edl_api {
 uint32_t magic,version,size,width,height;
 uintptr_t syscall_dispatcher;
 const char *image_path;
 void (*rect)(int,int,int,int,uint16_t);
 void (*text)(int,int,uint16_t,const char *);
 void (*blit)(int,int,int,int,const uint16_t *);
 void (*present)(void);
 int (*wait)(struct edl_event *);
 int (*scan)(unsigned,struct edl_record *,unsigned);
 int (*metadata)(const char *,struct edl_metadata *);
 int (*read)(const char *,uint32_t,void *,uint32_t);
 int (*os_call)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
 int (*launch)(const char *);
 uint32_t arena_start,arena_end,stack_start,stack_end;
 int (*status)(struct edl_status *);
 int (*wait_timeout)(struct edl_event *,unsigned);
};
#endif
