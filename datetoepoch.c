#include <stdio.h>
#include <time.h>
#include "datetoepoch.h"

// Convert dump1090/net_io.c date and time string back to seconds with rounding
//
// Fields 7 & 8 are the message reception time and date
// p += sprintf(p, "%04d/%02d/%02d,", (stTime_receive.tm_year+1900),(stTime_receive.tm_mon+1), stTime_receive.tm_mday);
// p += sprintf(p, "%02d:%02d:%02d.%03u,", stTime_receive.tm_hour, stTime_receive.tm_min, stTime_receive.tm_sec, (unsigned) (mm->sysTimestampMsg % 1000));

time_t
Date2Epoch(const char *date_s, const char *time_s)
{
	struct tm tm;
	int ms;
	time_t t;

	sscanf(date_s, "%d/%d/%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
	sscanf(time_s, "%d:%d:%d.%d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &ms);
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	tm.tm_isdst = -1;
	t = mktime(&tm);
	if (ms >= 500)
		++t;

	return t;
}

