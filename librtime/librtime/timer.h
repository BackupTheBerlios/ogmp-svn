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

 xrtp_clock_t * time_begin(xrtp_lrtime_t lrt, xrtp_hrtime_t hrt);

 int time_end(xrtp_clock_t * clock);

 /**
  * Adjust the clock forward if positive int, backward if negitive int or nochange if zero
  * Note: hrt value MUST fall in the range of the ONE unit of lrt value
  */
 int time_adjust(xrtp_clock_t * clock, xrtp_lrtime_t lrt, xrtp_hrtime_t hrt);

 int time_now(xrtp_clock_t * clock, xrtp_lrtime_t *lrt, xrtp_hrtime_t *hrt);
 
 xrtp_hrtime_t hrtime_now(xrtp_clock_t * clock);
 xrtp_hrtime_t hrtime_passed(xrtp_clock_t * clock, xrtp_hrtime_t hrts_then);
 xrtp_hrtime_t hrtime_checkpass(xrtp_clock_t * clock, xrtp_hrtime_t *then);
 int hrtime_sleep(xrtp_clock_t * clock, xrtp_hrtime_t howlong, xrtp_hrtime_t *remain);

 xrtp_lrtime_t lrtime_now(xrtp_clock_t * clock);
 xrtp_lrtime_t lrtime_passed(xrtp_clock_t * clock, xrtp_lrtime_t then); 
 xrtp_lrtime_t lrtime_checkpass(xrtp_clock_t * clock, xrtp_lrtime_t *then);
 int lrtime_sleep(xrtp_clock_t * clock, xrtp_lrtime_t howlong, xrtp_lrtime_t *remain);

 xrtp_hrtime_t time_usec_now(xrtp_clock_t * clock);
 xrtp_hrtime_t time_usec_passed(xrtp_clock_t * clock, xrtp_hrtime_t hrts_then);
 xrtp_hrtime_t time_usec_checkpass(xrtp_clock_t * clock, xrtp_hrtime_t * then);
 int time_usec_sleep(xrtp_clock_t * clock, xrtp_hrtime_t howlong, xrtp_hrtime_t * remain);
 
 int time_passed(xrtp_clock_t * clock, xrtp_lrtime_t lrts_then, xrtp_hrtime_t hrts_then, xrtp_lrtime_t *lrt_passed, xrtp_hrtime_t *hrt_passed);

#endif
