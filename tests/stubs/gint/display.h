#ifndef TEST_DISPLAY_H
#define TEST_DISPLAY_H
#include <stdint.h>
#define DWIDTH 528
#define DHEIGHT 320
void dclear(uint16_t);
void drect(int,int,int,int,uint16_t);
void dtext(int,int,uint16_t,const char*);
void dupdate(void);
#endif
