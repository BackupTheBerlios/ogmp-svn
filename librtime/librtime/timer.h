/***************************************************************************
                          timer.h  -  Time line for rtp
                             -------------------
    begin                : Wed Mar 5 2003
    copyright            : (C) 2003 by Heming Ling
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TIMER_H
 
 #define TIMER_H

 #include <timedia/os.h>
 #include <time.h>

 /* seconds b/w 1970(epoch) and 1900(ntp) */
 #define EPOCH_OFFSET 2208988800
 
 /* FIXME: Consider time warp */
 #define HRTIME_BITS   32
 #define HRTIME_SECOND_DIVISOR 1000000000    /* HRT -> SEC */
 #define LRTIME_SECOND_DIVISOR 1000          /* LRT -> SEC */
 #define LRT_HRT_DIVISOR 1000000             /* LRT -> HRT */
 
 typedef int32 rtime_t;
 typedef int32 xrtp_hrtime_t;  /* 1 nanosecond unit */
 typedef int32 xrtp_lrtime_t;  /* 1 millisecond unit */

 #define xrtp_clock_s xclock_s
 #define xrtp_clock_t xclock_t
 typedef struct xclock_s xclock_t;
 
 typedef struct clock_timeout_s clock_timeout_t;

 #define HRTIME_INFINITY 0x7fffFfff
 #define HRTIME_MAXNSEC  1000000000
 
 #define HRTIME_NANO     1
 #define HRTIME_MICRO    1000
 #define HRTIME_MILLI    1000000
 #define HRTIME_SECOND   1000000000

 #define HRTIME_USEC(x) ((x)/1000)
 
 #define TIME_NEWER(x,y) (((x) - (y)) >> (HRTIME_BITS - 1))

/* new api */
/* start timer at (0,0,0), time_begin will be replace by this */
extern DECLSPEC
xclock_t* 
time_start();

extern DECLSPEC
int 
time_end(xclock_t * clock);

/**
 * Adjust the clock forward if positive int, backward if negitive int or nochange if zero
 * Note: hrt value MUST fall in the range of the ONE unit of lrt value
 */
extern DECLSPEC int 
time_adjust(xclock_t * clock, rtime_t dmsec, rtime_t dusec, rtime_t dnsec);

extern DECLSPEC 
int 
time_rightnow(xclock_t * clock, rtime_t *msec, rtime_t *usec, rtime_t *nsec); 

extern DECLSPEC 
int 
time_spent(xrtp_clock_t * clock, rtime_t ms_then, rtime_t us_then, rtime_t ns_then, rtime_t *ms_spent, rtime_t *us_spent, rtime_t *ns_spent);

/* for nanosecond */
extern DECLSPEC
rtime_t 
time_nsec_now(xclock_t * clock);

extern DECLSPEC
rtime_t 
time_nsec_spent(xclock_t * clock, rtime_t ns_then);

extern DECLSPEC
rtime_t
time_nsec_checkpass(xclock_t * clock, rtime_t *then);

extern DECLSPEC
int 
time_nsec_sleep(xclock_t * clock, rtime_t howlong, rtime_t *remain);

/* for microsecond */
extern DECLSPEC
rtime_t
time_usec_now(xclock_t * clock);

extern DECLSPEC
rtime_t
time_usec_spent(xclock_t * clock, rtime_t us_then);

extern DECLSPEC
rtime_t
time_usec_checkpass(xclock_t * clock, rtime_t *then);

extern DECLSPEC
int 
time_usec_sleep(xclock_t * clock, rtime_t howlong, rtime_t *remain);
 
/* for millisecond */
extern DECLSPEC
rtime_t
time_msec_now(xclock_t * clock);

extern DECLSPEC
rtime_t
time_msec_spent(xclock_t * clock, rtime_t ms_then);

extern DECLSPEC
rtime_t
time_msec_checkpass(xclock_t * clock, rtime_t *then);

extern DECLSPEC
int 
time_msec_sleep(xclock_t * clock, int howlong, rtime_t *remain);

extern DECLSPEC
int 
time_ntp(xrtp_clock_t * clock, uint32 *hintp, uint32 *lontp);

#endif
