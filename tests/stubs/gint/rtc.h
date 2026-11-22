#ifndef TEST_RTC_H
#define TEST_RTC_H
#include <stdint.h>
typedef struct {unsigned year,month,month_day,hours,minutes,seconds;} rtc_time_t;
void rtc_get_time(rtc_time_t*);
uint32_t rtc_ticks(void);
#endif
