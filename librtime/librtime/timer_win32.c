
/***************************************************************************
                          timer_win32.c  -  Win32 implementation of timer.h
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
 
 struct clock_s{

	LARGE_INTEGER  frequency;
 
    LARGE_INTEGER  now;
    
    int nanosec;  /* 1/1000000000 seconds */

	int microsec; /* 1/1000000 seconds */ 

    int millisec;   /* 1/1000 seconds */

    xrtp_hrtime_t (*hrtime_now)(xrtp_clock_t *clock);

    xthr_lock_t * lock;
 };

 int win32_time_now(xrtp_clock_t * clock){

	LONGLONG  lastCount = 0;
	LONGLONG  deltaCount = 0;

    lastCount = clock->now.QuadPart;
    
	QueryPerformanceCounter(&clock->now);										 
										
	deltaCount = clock->now.QuadPart - lastCount;
	
	time_log(("win32_time_now: use timer[@%d] deltaCount=%lld, timer frequency=%lld\n", (int)clock, deltaCount, clock->frequency));

	clock->nanosec += (int)(deltaCount * 1000000000 / clock->frequency.QuadPart);

	clock->microsec += (int)(deltaCount * 1000000 / clock->frequency.QuadPart);

    clock->millisec += (int)(deltaCount * 1000 / clock->frequency.QuadPart); 
    
    return clock->nanosec;
}

xrtp_clock_t* 
time_start(){

    xrtp_clock_t * clock = (xrtp_clock_t *)malloc(sizeof(struct xrtp_clock_s));

	/*return null if window doesn't support a high-resolution performance counter*/

    if(clock){

		if (!QueryPerformanceFrequency( &clock->frequency)){

			free(clock);
			return NULL;
		}

		time_log(("time_start: timer frequency = %lldHz.\n", clock->frequency));

        QueryPerformanceCounter( &clock->now );
		
		time_log(("time_start: begin counter is %lld\n", clock->now.QuadPart));

        clock->millisec = 0;
		clock->microsec = 0;
        clock->nanosec = 0;

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

int time_end(xrtp_clock_t * clock){

    xthr_done_lock(clock->lock);
    free(clock);

    return OS_OK;
}

int time_adjust(xrtp_clock_t * clock, int dmsec, int dusec, int dnsec){

    int u, n;

    time_log(("time_adjust: to [%d:%d:%d]\n", dmsec, dusec, dnsec));

	if(dmsec==0 && dusec==0 && dnsec==0)
        return OS_OK;

    xthr_lock(clock->lock);

    n = clock->nanosec + dnsec;

	clock->nanosec = n % 1000000000;

	u = clock->microsec + dusec + n/1000000000;

	clock->microsec = u % 1000000;

	clock->millisec += dmsec + u/1000000;

    xthr_unlock(clock->lock);

    time_log(("time_adjust: clock adjusted to [%d:%d:%d].\n", clock->millisec, clock->microsec, clock->nanosec));

    return OS_OK;
}

int time_rightnow(xrtp_clock_t * clock, int *msec, int *usec, int *nsec){

    xthr_lock(clock->lock);
    
    *nsec = clock->hrtime_now(clock);
    *usec = clock->microsec;
	*msec = clock->millisec;
    
    xthr_unlock(clock->lock);

    return OS_OK;
}

int time_nsec_now(xrtp_clock_t * clock){

    int t;

    xthr_lock(clock->lock);
    t =clock->hrtime_now(clock);
    xthr_unlock(clock->lock);
    
    return t;
}

/* FIXME: Consider nagetive integer value as xrtp_hrtime_t is signed */
int time_nsec_spent(xrtp_clock_t * clock, int then){

    int now;

    xthr_lock(clock->lock);
    now =clock->hrtime_now(clock);
    xthr_unlock(clock->lock);

    return now - then;
}

int time_usec_now(xrtp_clock_t * clock){
 
    int t;

    xthr_lock(clock->lock);

    clock->hrtime_now(clock);
    t = clock->microsec;

    xthr_unlock(clock->lock);
	
	return t;
}

int time_usec_spent(xrtp_clock_t * clock, int then){

	return time_usec_now(clock) - then;
}

int time_msec_now(xrtp_clock_t * clock){
 
    int m, u, n;

    xthr_lock(clock->lock);

    clock->hrtime_now(clock);
    m = clock->millisec;
    u = clock->microsec;
    n = clock->nanosec;

    xthr_unlock(clock->lock);
	
	time_log(("time_msec_now: msec = %d, usec = %d nsec = %d\n", m, u, n));

	return m;
}

int time_msec_spent(xrtp_clock_t * clock, int then){

	return time_msec_now(clock) - then;
}

int time_spent(xrtp_clock_t * clock, int ms_then, int us_then, int ns_then, int *ms_spent, int *us_spent, int *ns_spent){

    int ms_now, us_now, ns_now;

    xthr_lock(clock->lock);
    ns_now = clock->hrtime_now(clock);
    us_now = clock->microsec;
	ms_now = clock->millisec;
    xthr_unlock(clock->lock);

    *ns_spent = ns_now - ns_then;
    *us_spent = us_now - us_then;
	*ms_spent = ms_now - ms_then;

    return OS_OK;
}

int time_win32_sleep(xrtp_clock_t * clock, int nunit, int unit_per_sec){

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
	ticks_to_wait = (int)(clock->frequency.QuadPart * nunit / unit_per_sec);

	do{

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
			if (ticks_left > (int)clock->frequency.QuadPart*2/1000){
            
				Sleep(1);
			
			}else{
				
				// causes thread to give up its timeslice
				for (i=0; i<10; i++) Sleep(0);  
			}
        }

	}while (!done);            

	return OS_OK;	 
}

int time_msec_sleep (xrtp_clock_t * clock, int msec, int *remain){

	Sleep(msec);

	return OS_OK;
}

int time_usec_sleep (xrtp_clock_t * clock, int usec, int *remain){

	return time_win32_sleep(clock, usec, 1000000);
}

int time_nsec_sleep (xrtp_clock_t * clock, int nsec, int *remain){

	return time_win32_sleep(clock, nsec, 1000000000);
}

/* =============== end ================= */
