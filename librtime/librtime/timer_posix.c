/***************************************************************************
                          timer_posix.c  -  POSIX implementation of timer.h
                             -------------------
    begin                : Fri Oct 3 2003
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

 #include "xthread.h"
 #include "timer.h"

 #include <signal.h>
 #include <sys/time.h>
 #include <time.h>
 #include <stdio.h>
 #include <stdlib.h>
 
 /* #define TIMER_LOG */
 
 #ifdef TIMER_LOG
   const int timer_log = 1;
 #else
   const int timer_log = 0;
 #endif
 #define time_log(fmtargs)  do{if(timer_log) printf fmtargs;}while(0)
 
 struct clock_s{

    struct timeval now;
    
    int nsec;  /* 1 nanosec */

    int usec;  /* 1 microsec */

    int msec;   /* 1 millisec */

    int (*hrtime_now)(xrtp_clock_t *clock);

    xthr_lock_t * lock;
 };

int posix_time_now(xrtp_clock_t * clock){
    
    struct timeval then = clock->now;
    
    gettimeofday(&clock->now, NULL);
    
    clock->msec += (clock->now.tv_sec - then.tv_sec) * 1000;
    clock->msec += (clock->now.tv_usec - then.tv_usec) / 1000;
    
    clock->usec += (clock->now.tv_sec - then.tv_sec) * 1000000;
    clock->usec += clock->now.tv_usec - then.tv_usec;

    clock->nsec += (clock->now.tv_sec - then.tv_sec) * HRTIME_SECOND_DIVISOR;
    clock->nsec += (clock->now.tv_usec - then.tv_usec) * 1000;

    time_log(("posix_time_now: msec = %d, nsec = %u\n",clock->msec , clock->nsec));

    return clock->nsec;
 }

 xrtp_clock_t * time_start(){

    xrtp_clock_t * clock = (xrtp_clock_t *)malloc(sizeof(struct xrtp_clock_s));
    if(clock){

        gettimeofday(&clock->now, NULL);

        clock->msec = 0;
        clock->usec = 0;
        clock->nsec = 0;

        clock->hrtime_now = posix_time_now;
        
        clock->lock = xthr_new_lock();
        if(!clock->lock){

          free(clock);
          return NULL;
        }
    }

    time_log(("time_begin: new clock[@%u] created.\n", (int)(clock)));
    return clock;
 }

int time_end(xrtp_clock_t * clock){

   xthr_done_lock(clock->lock);
   free(clock);

   return OS_OK;
}

int time_adjust(xrtp_clock_t * clock, int dmsec, int dusec, int dnsec){
  
   int u, n;

   time_log(("time_adjust: to [%d:%d:%d]\n", dmsec, dusec, dnsec));

   if(dmsec==0 && dusec==0 && dnsec==0) return OS_OK;

   xthr_lock(clock->lock);

   n = clock->nsec + dnsec;

   clock->nsec = n % 1000000000;

   u = clock->usec + dusec + n/1000000000;

   clock->usec = u % 1000000;

   clock->msec += dmsec + u/1000000;

   xthr_unlock(clock->lock);

   time_log(("time_adjust: clock adjusted to [%d:%d:%d].\n", clock->msec, clock->usec, clock->nsec));

   return OS_OK;
}

int time_spent(xrtp_clock_t * clock, rtime_t ms_then, rtime_t us_then, rtime_t ns_then, rtime_t *ms_spent, rtime_t *us_spent, rtime_t *ns_spent){

    rtime_t ms_now, us_now, ns_now;

    xthr_lock(clock->lock);
    ns_now = clock->hrtime_now(clock);
    us_now = clock->usec;
    ms_now = clock->msec;
    xthr_unlock(clock->lock);

    *ms_spent = ms_now - ms_then;
    *us_spent = us_now - us_then;
    *ns_spent = ns_now - ns_then;

    return OS_OK;
}

int time_rightnow(xrtp_clock_t * clock, int *msec, int *usec, int *nsec){

    xthr_lock(clock->lock);

    *nsec = clock->hrtime_now(clock);
    *usec = clock->usec;
    *msec = clock->msec;

    xthr_unlock(clock->lock);

    return OS_OK;
}

/* For nanosec */
int time_nsec_now(xrtp_clock_t * clock) {

    int t;

    xthr_lock(clock->lock);
    t = clock->hrtime_now(clock);
    xthr_unlock(clock->lock);

    return t;
}

int time_nsec_spent(xrtp_clock_t * clock, int then){

    return time_nsec_now(clock) - then;
}

int time_nsec_checkpass(xrtp_clock_t * clock, int *then){

    int t = *then;

    *then = time_nsec_now(clock);

    return *then - t;
}

int time_nsec_sleep(xrtp_clock_t * clock, int nsec, int * remain) {

    int r;

    if (nsec <=0) return OS_OK;

    struct timespec tv, rem;
    tv.tv_sec = nsec / 1000000000;
    tv.tv_nsec = nsec % 1000000000;

    r = nanosleep(&tv, &rem);

    if(remain)
        *remain = rem.tv_sec * 1000000000 + rem.tv_nsec;

    if(r == -1) return OS_EINTR;

    return OS_OK;
}

/* For microsec */
int time_usec_now(xrtp_clock_t * clock) {

    int t;

    xthr_lock(clock->lock);
    clock->hrtime_now(clock);
    t = clock->usec;
    xthr_unlock(clock->lock);

    return t;
}

int time_usec_spent(xrtp_clock_t * clock, int then){

    return time_usec_now(clock) - then;
}

int time_usec_checkpass(xrtp_clock_t * clock, int *then){

    int t = *then;
    
    *then = time_usec_now(clock);

    return *then - t;
}

int time_usec_sleep(xrtp_clock_t * clock, int usec, int * remain) {

    int r;

    if (usec <=0) return OS_OK;

    struct timespec tv, rem;
    tv.tv_sec = usec / 1000000;
    tv.tv_nsec = (usec % 1000000) * 1000;

    r = nanosleep(&tv, &rem);
    
    if(remain)
        *remain = rem.tv_sec * 1000000 + rem.tv_nsec * 1000;

    if(r == -1) return OS_EINTR;

    return OS_OK;
}

/* For millisec */
int time_msec_now(xrtp_clock_t * clock) {

    int t;

    xthr_lock(clock->lock);
    clock->hrtime_now(clock);
    t = clock->msec;
    xthr_unlock(clock->lock);

    return t;
}

int time_msec_spent(xrtp_clock_t * clock, int then){

    return time_msec_now(clock) - then;
}

int time_msec_checkpass(xrtp_clock_t * clock, int *then){

    int t = *then;

    *then = time_msec_now(clock);

    return *then - t;
}

int time_msec_sleep(xrtp_clock_t * clock, int msec, int * remain) {

    int r;

    if (msec <=0) return OS_OK;

    struct timespec tv, rem;
    tv.tv_sec = msec / 1000;
    tv.tv_nsec = (msec % 1000) * 1000000;

    r = nanosleep(&tv, &rem);

    if(remain)
        *remain = rem.tv_sec * 1000 + rem.tv_nsec * 1000000;

    if(r == -1) return OS_EINTR;

    return OS_OK;
}

/* old api, don't use any more */
 xrtp_hrtime_t hrtime_now(xrtp_clock_t * clock){

    xrtp_hrtime_t t;

    xthr_lock(clock->lock);
    t =clock->hrtime_now(clock);
    xthr_unlock(clock->lock);
    
    return t;
 }

 /* FIXME: Consider nagetive integer value as xrtp_hrtime_t is signed */
 xrtp_hrtime_t hrtime_passed(xrtp_clock_t * clock, xrtp_hrtime_t then){

    xrtp_hrtime_t now;

    xthr_lock(clock->lock);
    now =clock->hrtime_now(clock);
    xthr_unlock(clock->lock);

    return now - then;
 }

 xrtp_hrtime_t hrtime_checkpass(xrtp_clock_t * clock, xrtp_hrtime_t *then){

    xrtp_hrtime_t t = *then;
    
    xthr_lock(clock->lock);
    *then = clock->hrtime_now(clock);
    xthr_unlock(clock->lock);

    return *then - t;
 }

 int time_passed(xrtp_clock_t * clock, xrtp_lrtime_t lrts_then, xrtp_hrtime_t hrts_then, xrtp_lrtime_t *lrt_passed, xrtp_hrtime_t *hrt_passed){

    xrtp_hrtime_t hrts_now;
    xrtp_lrtime_t lrts_now;

    xthr_lock(clock->lock);
    hrts_now = clock->hrtime_now(clock);
    lrts_now = clock->msec;
    xthr_unlock(clock->lock);

    *lrt_passed = lrts_now - lrts_then;
    *hrt_passed = hrts_now - hrts_then;

    return OS_OK;
 }

 xrtp_clock_t * time_begin(xrtp_lrtime_t lrt, xrtp_hrtime_t hrt){

    xrtp_clock_t * clock = (xrtp_clock_t *)malloc(sizeof(struct xrtp_clock_s));
    if(clock){

        gettimeofday(&clock->now, NULL);

        clock->msec = lrt;
        clock->nsec = hrt;

        clock->hrtime_now = posix_time_now;

        clock->lock = xthr_new_lock();
        if(!clock->lock){

          free(clock);
          return NULL;
        }
    }

    time_log(("time_begin: new clock[@%u] created.\n", (int)(clock)));
    return clock;
 }

int time_now(xrtp_clock_t * clock, xrtp_lrtime_t *lrt, xrtp_hrtime_t *hrt){

    xthr_lock(clock->lock);

    *hrt = clock->hrtime_now(clock);
    *lrt = clock->msec;

    xthr_unlock(clock->lock);

    return OS_OK;
}

 xrtp_lrtime_t lrtime_now(xrtp_clock_t * clock){

    xrtp_lrtime_t t;

    xthr_lock(clock->lock);
    clock->hrtime_now(clock);
    t = clock->msec;
    xthr_unlock(clock->lock);

    return t;
 }
                              
 xrtp_lrtime_t lrtime_passed(xrtp_clock_t * clock, xrtp_lrtime_t then){

    return lrtime_now(clock) - then;
 }

 xrtp_lrtime_t lrtime_checkpass(xrtp_clock_t * clock, xrtp_lrtime_t *then){

    xrtp_lrtime_t t = *then;
    *then = lrtime_now(clock);
    
    return *then - t;
 }
 
 int hrtime_sleep(xrtp_clock_t * clock, xrtp_hrtime_t howlong, xrtp_hrtime_t *remain){

    int r;

    if (howlong <=0) return OS_OK;

    struct timespec tv, rem;
    tv.tv_sec = howlong / HRTIME_SECOND_DIVISOR;
    tv.tv_nsec = howlong % HRTIME_SECOND_DIVISOR;
    
    r = nanosleep(&tv, &rem);
    if(remain)
        *remain = rem.tv_sec * HRTIME_SECOND_DIVISOR + rem.tv_nsec;

    if(r == -1) return OS_EINTR;

    return OS_OK;
 }

 int lrtime_sleep(xrtp_clock_t * clock, xrtp_lrtime_t howlong, xrtp_lrtime_t *remain){

    int r;

    if (howlong <=0) return OS_OK;

    struct timespec tv, rem;
    tv.tv_sec = howlong / LRTIME_SECOND_DIVISOR;
    tv.tv_nsec = (howlong % LRTIME_SECOND_DIVISOR) * LRT_HRT_DIVISOR;
    time_log(("lrtime_sleep: sleep %ums =  %u sec %u nsec\n", howlong, (uint)tv.tv_sec, (uint)tv.tv_nsec));
    r = nanosleep(&tv, &rem);
    if(remain)
        *remain = rem.tv_sec * LRTIME_SECOND_DIVISOR + rem.tv_nsec / LRT_HRT_DIVISOR;

    if(r == -1) return OS_EINTR;

    return OS_OK;
 }
