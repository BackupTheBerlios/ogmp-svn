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

 #ifndef OS_H
 #include <timedia/os.h>
 #endif
 
 /* FIXME: Consider time warp */
 #define HRTIME_BITS   32
 #define HRTIME_SECOND_DIVISOR 1000000000    /* HRT -> SEC */
 #define LRTIME_SECOND_DIVISOR 1000          /* LRT -> SEC */
 #define LRT_HRT_DIVISOR 1000000             /* LRT -> HRT */
 
 typedef int32 rtime_t;
 typedef int32 xrtp_hrtime_t;  /* 1 nanosecond unit */
 typedef int32 xrtp_lrtime_t;  /* 1 millisecond unit */

 typedef struct xrtp_clock_s xrtp_clock_t;
 
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
extern DECLSPEC xrtp_clock_t* 
time_start();
extern DECLSPEC int 
time_end(xrtp_clock_t * clock);

/**
 * Adjust the clock forward if positive int, backward if negitive int or nochange if zero
 * Note: hrt value MUST fall in the range of the ONE unit of lrt value
 */
extern DECLSPEC int 
time_adjust(xrtp_clock_t * clock, int dmsec, int dusec, int dnsec);

extern DECLSPEC int 
time_rightnow(xrtp_clock_t * clock, int *msec, int *usec, int *nsec); 
extern DECLSPEC int 
time_spent(xrtp_clock_t * clock, int ms_then, int us_then, int ns_then, int *ms_spent, int *us_spent, int *ns_spent);

/* for nanosecond */
extern DECLSPEC int 
time_nsec_now(xrtp_clock_t * clock);
extern DECLSPEC int 
time_nsec_spent(xrtp_clock_t * clock, int ns_then);
extern DECLSPEC int 
time_nsec_checkpass(xrtp_clock_t * clock, int * then);
extern DECLSPEC int 
time_nsec_sleep(xrtp_clock_t * clock, int howlong, int * remain);

/* for microsecond */
extern DECLSPEC int 
time_usec_now(xrtp_clock_t * clock);
extern DECLSPEC int 
time_usec_spent(xrtp_clock_t * clock, int us_then);
extern DECLSPEC int 
time_usec_checkpass(xrtp_clock_t * clock, int * then);
extern DECLSPEC int 
time_usec_sleep(xrtp_clock_t * clock, int howlong, int * remain);
 
/* for millisecond */
extern DECLSPEC int 
time_msec_now(xrtp_clock_t * clock);
extern DECLSPEC int 
time_msec_spent(xrtp_clock_t * clock, int ms_then);
extern DECLSPEC int 
time_msec_checkpass(xrtp_clock_t * clock, int * then);
extern DECLSPEC int 
time_msec_sleep(xrtp_clock_t * clock, int howlong, int * remain);

#endif
