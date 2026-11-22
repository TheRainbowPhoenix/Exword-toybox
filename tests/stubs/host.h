#ifndef TEST_HOST_H
#define TEST_HOST_H
#include "edl.h"
struct edl_arena {uint32_t arena_start,arena_end,stack_start,stack_end;};
struct edl_image {uint32_t entry;unsigned count;};
extern struct edl_arena edl_active_arena;
int edl_profile_init(void);void edl_profile_release(void);
int edl_scan(unsigned,struct edl_record*,unsigned);
int edl_metadata(const char*,struct edl_metadata*);
int edl_read(const char*,uint32_t,void*,uint32_t);
int edl_os_call(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
int edl_load(const char*,struct edl_image*);
int edl_invoke(uint32_t,const struct edl_api*,uint32_t);
void edl_gint_cleanup(void);void edl_sync_code(void);
int edl_content_id(const char id[8],char appid[7],unsigned order[4]);
#endif
