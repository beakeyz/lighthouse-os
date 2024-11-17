#ifndef __LIGHTENV_LIBC_TIME__
#define __LIGHTENV_LIBC_TIME__

#include <sys/types.h>

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct tm {
    int tm_sec; /* Seconds          [0, 60] */
    int tm_min; /* Minutes          [0, 59] */
    int tm_hour; /* Hour             [0, 23] */
    int tm_mday; /* Day of the month [1, 31] */
    int tm_mon; /* Month            [0, 11]  (January = 0) */
    int tm_year; /* Year minus 1900 */
    int tm_wday; /* Day of the week  [0, 6]   (Sunday = 0) */
    int tm_yday; /* Day of the year  [0, 365] (Jan/01 = 0) */
    int tm_isdst; /* Daylight savings flag */

    long tm_gmtoff; /* Seconds East of UTC */
    const char* tm_zone; /* Timezone abbreviation */
};

/* unistd.c */
extern int nanosleep(const struct timespec* req, struct timespec* rem);

#endif // !__LIGHTENV_LIBC_TIME__
