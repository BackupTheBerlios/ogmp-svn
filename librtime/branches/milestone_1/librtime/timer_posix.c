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

 #include <timedia/xthread.h>
 #include <timedia/xmalloc.h>
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
 
 struct xclock_s
 {
    struct timeval now;
    
    int nsec;  /* 1 nanosec */

    int usec;  /* 1 microsec */

    int msec;   /* 1 millisec */

    uint32 ntp_usec;

    int (*hrtime_now)(xclock_t *clock);

    xthr_lock_t * lock;
 };

int posix_time_now(xclock_t * clock)
{
    struct timeval then = clock->now;
    
    gettimeofday(&clock->now, NULL);
    
    clock->msec += (clock->now.tv_sec - then.tv_sec) * 1000;
    clock->msec += (clock->now.tv_usec - then.tv_usec) / 1000;
    
    clock->usec += (clock->now.tv_sec - then.tv_sec) * 1000000;
    clock->usec += clock->now.tv_usec - then.tv_usec;

    clock->nsec += (clock->now.tv_sec - then.tv_sec) * 1000000000;
    clock->nsec += (clock->now.tv_usec - then.tv_usec) * 1000;

    clock->ntp_usec = clock->now.tv_usec;

    time_log(("posix_time_now: msec = %d, nsec = %u\n",clock->msec , clock->nsec));

    return clock->nsec;
 }

 xclock_t * time_start(){

    xclock_t * clock = (xclock_t *)xmalloc(sizeof(struct xrtp_clock_s));
    if(clock){

        gettimeofday(&clock->now, NULL);

        clock->msec = 0;
        clock->usec = 0;
        clock->nsec = 0;

        clock->hrtime_now = posix_time_now;
        
        clock->lock = xthr_new_lock();
        if(!clock->lock){

          xfree(clock);
          return NULL;
        }
    }

    time_log(("time_begin: new clock[@%u] created.\n", (int)(clock)));
    return clock;
 }

int time_end(xclock_t * clock){

   xthr_done_lock(clock->lock);
   xfree(clock);

   return OS_OK;
}

int time_adjust(xclock_t * clock, int dmsec, int dusec, int dnsec){
  
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

int time_spent(xclock_t * clock, rtime_t ms_then, rtime_t us_then, rtime_t ns_then, rtime_t *ms_spent, rtime_t *us_spent, rtime_t *ns_spent){

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

int time_rightnow(xclock_t * clock, int *msec, int *usec, int *nsec){

    xthr_lock(clock->lock);

    *nsec = clock->hrtime_now(clock);
    *usec = clock->usec;
    *msec = clock->msec;

    xthr_unlock(clock->lock);

    return OS_OK;
}

/* For nanosec */
int time_nsec_now(xclock_t * clock) {

    int t;

    xthr_lock(clock->lock);
    t = clock->hrtime_now(clock);
    xthr_unlock(clock->lock);

    return t;
}

int time_nsec_spent(xclock_t * clock, int then){

    return time_nsec_now(clock) - then;
}

int time_nsec_checkpass(xclock_t * clock, int *then){

    int t = *then;

    *then = time_nsec_now(clock);

    return *then - t;
}

int time_nsec_sleep(xclock_t * clock, int nsec, int * remain) {

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
int time_usec_now(xclock_t * clock) {

    int t;

    xthr_lock(clock->lock);
    clock->hrtime_now(clock);
    t = clock->usec;
    xthr_unlock(clock->lock);

    return t;
}

int time_usec_spent(xclock_t * clock, int then){

    return time_usec_now(clock) - then;
}

int time_usec_checkpass(xclock_t * clock, int *then){

    int t = *then;
    
    *then = time_usec_now(clock);

    return *then - t;
}

int time_usec_sleep(xclock_t * clock, int usec, int * remain) {

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
int time_msec_now(xclock_t * clock) {

    int t;

    xthr_lock(clock->lock);
    clock->hrtime_now(clock);
    t = clock->msec;
    xthr_unlock(clock->lock);

    return t;
}

int time_msec_spent(xclock_t * clock, int then){

    return time_msec_now(clock) - then;
}

int time_msec_checkpass(xclock_t * clock, int *then){

    int t = *then;

    *then = time_msec_now(clock);

    return *then - t;
}

int time_msec_sleep(xclock_t * clock, int msec, int * remain) {

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

int time_ntp(xrtp_clock_t * clock, uint32 *hintp, uint32 *lontp)
{
    xthr_lock(clock->lock);

    clock->hrtime_now(clock);
    *lontp = clock->ntp_usec;
    *hintp = time(NULL) + EPOCH_OFFSET;

    xthr_unlock(clock->lock);

    return OS_OK;
}

/* old api, don't use any more */
 rtime_t hrtime_now(xclock_t * clock){

    rtime_t t;

    xthr_lock(clock->lock);
    t =clock->hrtime_now(clock);
    xthr_unlock(clock->lock);
    
    return t;
 }

 /* FIXME: Consider nagetive integer value as rtime_t is signed */
 rtime_t hrtime_passed(xclock_t * clock, rtime_t then){

    rtime_t now;

    xthr_lock(clock->lock);
    now =clock->hrtime_now(clock);
    xthr_unlock(clock->lock);

    return now - then;
 }

 rtime_t hrtime_checkpass(xclock_t * clock, rtime_t *then){

    rtime_t t = *then;
    
    xthr_lock(clock->lock);
    *then = clock->hrtime_now(clock);
    xthr_unlock(clock->lock);

    return *then - t;
 }

 int time_passed(xclock_t * clock, rtime_t lrts_then, rtime_t hrts_then, rtime_t *lrt_passed, rtime_t *hrt_passed){

    rtime_t hrts_now;
    rtime_t lrts_now;

    xthr_lock(clock->lock);
    hrts_now = clock->hrtime_now(clock);
    lrts_now = clock->msec;
    xthr_unlock(clock->lock);

    *lrt_passed = lrts_now - lrts_then;
    *hrt_passed = hrts_now - hrts_then;

    return OS_OK;
 }

 xclock_t * time_begin(rtime_t lrt, rtime_t hrt){

    xclock_t * clock = (xclock_t *)xmalloc(sizeof(struct xrtp_clock_s));
    if(clock){

        gettimeofday(&clock->now, NULL);

        clock->msec = lrt;
        clock->nsec = hrt;

        clock->hrtime_now = posix_time_now;

        clock->lock = xthr_new_lock();
        if(!clock->lock){

          xfree(clock);
          return NULL;
        }
    }

    time_log(("time_begin: new clock[@%u] created.\n", (int)(clock)));
    return clock;
 }

int time_now(xclock_t * clock, rtime_t *lrt, rtime_t *hrt){

    xthr_lock(clock->lock);

    *hrt = clock->hrtime_now(clock);
    *lrt = clock->msec;

    xthr_unlock(clock->lock);

    return OS_OK;
}

 rtime_t lrtime_now(xclock_t * clock){

    rtime_t t;

    xthr_lock(clock->lock);
    clock->hrtime_now(clock);
    t = clock->msec;
    xthr_unlock(clock->lock);

    return t;
 }
                              
 rtime_t lrtime_passed(xclock_t * clock, rtime_t then){

    return lrtime_now(clock) - then;
 }

 rtime_t lrtime_checkpass(xclock_t * clock, rtime_t *then){

    rtime_t t = *then;
    *then = lrtime_now(clock);
    
    return *then - t;
 }
 
 int hrtime_sleep(xclock_t * clock, rtime_t howlong, rtime_t *remain){

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

 int lrtime_sleep(xclock_t * clock, rtime_t howlong, rtime_t *remain){

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
