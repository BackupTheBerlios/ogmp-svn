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
 #include <windows.h>
 #include <winbase.h>

 #include <stdio.h>
 #include <stdlib.h>
 
 //#define TIMER_LOG 
 #ifdef TIMER_LOG
   const int timer_log = 1;
 #else
   const int timer_log = 0;
 #endif
 #define time_log(fmtargs)  do{if(timer_log) printf fmtargs;}while(0)
 
 struct xrtp_clock_s{
	LARGE_INTEGER  frequency;
 
    LARGE_INTEGER  now;
    
    xrtp_hrtime_t nsec;  /* 1 nanosec */

    xrtp_lrtime_t tms;   /* 10 millisec */

    xrtp_hrtime_t (*hrtime_now)(xrtp_clock_t *clock);

    xthr_lock_t * lock;
 };

 xrtp_hrtime_t win32_time_now(xrtp_clock_t * clock){

	LONGLONG  lastCount = 0;

    lastCount = clock->now.QuadPart;
    
	QueryPerformanceCounter(& clock->now );										 
											
	clock->nsec += (int)(clock->now.QuadPart - lastCount) / (int)(clock->frequency.QuadPart) * HRTIME_SECOND;

    clock->tms += (int)((clock->now.QuadPart - lastCount)/clock->frequency.QuadPart) * 100; 
    
	time_log(("win32_time_now: tms = %d, nsec = %u\n",clock->tms , clock->nsec));
    return clock->nsec;
 }

 xrtp_clock_t * time_begin(xrtp_lrtime_t lrt, xrtp_hrtime_t hrt){

     xrtp_clock_t * clock = (xrtp_clock_t *)malloc(sizeof(struct xrtp_clock_s));
	 LARGE_INTEGER  frequency;
	 /*return null if window doesn't support a high-resolution performance counter*/
	 if (!QueryPerformanceFrequency( &frequency)){
		 return NULL;
	 }
   
	
    if(clock){
        clock->frequency = frequency;
        QueryPerformanceCounter( &clock->now ); 
		printf("begin quadpart is --->%lld\n", clock->now.QuadPart);
        clock->tms = lrt;
        clock->nsec = hrt;

        clock->hrtime_now = win32_time_now;
        
        clock->lock = xthr_new_lock();
        if(!clock->lock){

          free(clock);
          return NULL;
        }
    }
    time_log(("time_begin: new clock[@%u] created.\n", (int)(clock)));
    return clock;
 }

 int time_adjust(xrtp_clock_t * clock, xrtp_lrtime_t lrt, xrtp_hrtime_t hrt){

    if(hrt < 1-LRT_HRT_DIVISOR || hrt > LRT_HRT_DIVISOR-1)
        return OS_INVALID;
        
    if(!hrt && !lrt)
        return OS_OK;

    xthr_lock(clock->lock);
    clock->tms += lrt;
    clock->nsec += hrt;
    xthr_unlock(clock->lock);

    return OS_OK;
 }

 int time_end(xrtp_clock_t * clock){

    xthr_done_lock(clock->lock);
    free(clock);

    return OS_OK;
 }

 int time_now(xrtp_clock_t * clock, xrtp_lrtime_t *lrt, xrtp_hrtime_t *hrt){

    xthr_lock(clock->lock);
    
    *hrt = clock->hrtime_now(clock);
    *lrt = clock->tms;
    
    xthr_unlock(clock->lock);

    return OS_OK;
 }
 
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

 int time_passed(xrtp_clock_t * clock, xrtp_lrtime_t lrts_then, xrtp_hrtime_t hrts_then, xrtp_lrtime_t *lrt_passed, xrtp_hrtime_t *hrt_passed){

    xrtp_hrtime_t hrts_now;
    xrtp_lrtime_t lrts_now;

    xthr_lock(clock->lock);
    hrts_now = clock->hrtime_now(clock);
    lrts_now = clock->tms;
    xthr_unlock(clock->lock);

    *lrt_passed = lrts_now - lrts_then;
    *hrt_passed = hrts_now - hrts_then;

    return OS_OK;
 }

 xrtp_lrtime_t lrtime_now(xrtp_clock_t * clock){

    xrtp_lrtime_t t;

    xthr_lock(clock->lock);
    clock->hrtime_now(clock);
    t = clock->tms;
    xthr_unlock(clock->lock);

    return t;
 }                            
 
 xrtp_lrtime_t lrtime_passed(xrtp_clock_t * clock, xrtp_lrtime_t then){

    return lrtime_now(clock) - then;
 }

 int hrtime_sleep(xrtp_clock_t * clock, xrtp_hrtime_t howlong, xrtp_hrtime_t *remain){

   LARGE_INTEGER m_prev_end_of_frame;  
   //int max_fps = 60;
   int ticks_passed = 0;
   int ticks_to_wait = 0;
   int ticks_left = 0;
   int done = 0;

   int i = 0;
   
   LARGE_INTEGER t;

   m_prev_end_of_frame.QuadPart = 0;
   done = 0;
   QueryPerformanceCounter(&t);
   m_prev_end_of_frame = t;
      
   /**
   **must cast at last, otherwise tick_to_wait could have negtive value
   **/
    ticks_to_wait = (int)(clock->frequency.QuadPart * howlong / HRTIME_SECOND);
      do
      {
        QueryPerformanceCounter(&t);
           
        ticks_passed = (int)((__int64)t.QuadPart - (__int64)m_prev_end_of_frame.QuadPart);
        ticks_left = ticks_to_wait - ticks_passed;

		if (t.QuadPart < m_prev_end_of_frame.QuadPart)    // time wrap
                    done = 1;
        if (ticks_passed >= ticks_to_wait)
                    done = 1;
                
        if (!done)
        {
         // if > 0.002s left, do Sleep(1), which will actually sleep some 
         //   steady amount, probably 1-2 ms,
         //   and do so in a nice way (cpu meter drops; laptop battery spared).
         // otherwise, do a few Sleep(0)'s, which just give up the timeslice,
         //   but don't really save cpu or battery, but do pass a tiny
         //   amount of time.
         if (ticks_left > (int)clock->frequency.QuadPart*2/1000)
            Sleep(1);
         else                        
            for (i=0; i<10; i++) 
               Sleep(0);  // causes thread to give up its timeslice
        }
      }
	  
      while (!done);            

	return OS_OK;
	 
 }

 int lrtime_sleep(xrtp_clock_t * clock, xrtp_lrtime_t howlong, xrtp_lrtime_t *remain){

   LARGE_INTEGER m_prev_end_of_frame;  
   //int max_fps = 60;
   int ticks_passed = 0;
   int ticks_to_wait = 0;
   int ticks_left = 0;
   int done = 0;

   int i = 0;
   
   LARGE_INTEGER t;

   m_prev_end_of_frame.QuadPart = 0;
   done = 0;
   QueryPerformanceCounter(&t);
   m_prev_end_of_frame = t;
   /**
   **must cast at last, otherwise tick_to_wait could have negtive value
   **/   
   ticks_to_wait = (int)(clock->frequency.QuadPart * howlong / LRTIME_SECOND_DIVISOR);

      do
      {
        QueryPerformanceCounter(&t);
           
        ticks_passed = (int)((__int64)t.QuadPart - (__int64)m_prev_end_of_frame.QuadPart);
        ticks_left = ticks_to_wait - ticks_passed;

		if (t.QuadPart < m_prev_end_of_frame.QuadPart)// time wrap
                    done = 1;
		if (ticks_passed >= ticks_to_wait)
                    done = 1;
      
        if (!done)
        {
         // if > 0.002s left, do Sleep(1), which will actually sleep some 
         //   steady amount, probably 1-2 ms,
         //   and do so in a nice way (cpu meter drops; laptop battery spared).
         // otherwise, do a few Sleep(0)'s, which just give up the timeslice,
         //   but don't really save cpu or battery, but do pass a tiny
         //   amount of time.
         if (ticks_left > (int)clock->frequency.QuadPart*2/1000)
            Sleep(1);
         else                        
            for (i=0; i<10; i++) 
               Sleep(0);  // causes thread to give up its timeslice
        }
      }
	  
      while (!done);      
      
	return OS_OK;
 }
