/***************************************************************************
                          t_time.c  -  Timer tester
                             -------------------
    begin                : Sun Nov 23 2003
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

#include "spu_text.h"
 
#include "timer.h"
#include "xthread.h"
 
#include <string.h>
#include <stdlib.h>

typedef struct subtitle_show_s{

    xrtp_lrtime_t seek;
    int adjusted;
    
    xrtp_lrtime_t start;  /* millisecond unit */
    xrtp_lrtime_t end;    /* millisecond unit */
    
    char texts[1024];
    int text_len;
    
    int finish;
    
    xthr_lock_t * lock;
    xthr_cond_t * ready_to_show;
    xrtp_clock_t * clock;
    
} subtitle_show_t;

subtitle_show_t show;

int recv_loop(void * param){

    xrtp_lrtime_t end;
    xrtp_lrtime_t delt;

    xrtp_lrtime_t lrnow;

    printf("thread display: Started ...\n");

    while(1){

       xthr_lock(show.lock);
       if(show.finish) break;
       xthr_cond_wait(show.ready_to_show, show.lock);
       
       show.texts[show.text_len] = '\0';
       printf("\n%s\n\n", show.texts);
       end = show.end;
       
       lrnow = time_msec_now(show.clock);
       delt = end - lrnow;       
       if(delt > 0){

          xthr_unlock(show.lock);
          time_msec_sleep(show.clock, delt, NULL);
          //printf("recv_loop: selpt %dms\n", lrtime_passed(show.clock, lrnow));

       }else{

          xthr_unlock(show.lock);
       }
    }

    xthr_unlock(show.lock);

    return OS_OK;
}
 
 int send_loop(void * param){

    subtitle_t * subt;
    
    int slen = 0;

    xrtp_lrtime_t delt;

    xrtp_lrtime_t lrnow;

    int line;
	
	int exit = 0;

	
    int s1,s2,s3,s4,e1,e2,e3,e4;
    int r;

	demux_sputext_t * spu = NULL;

    char * title = "subtitle.srt";

    FILE * f = NULL;
	
	f = fopen(title, "r");

    
   if( f == NULL )
   {
	   printf( "The file 'subtitle.srt' was not opened\n" );
       return OS_OK;
   }
   else
      printf( "The file 'subtitle.srt' was opened\n" );


   spu = demux_sputext_new(f);
    
   printf("thread run: Subtitle specialised\n");

    
   while(1){

       subt = demux_sputext_next_subtitle(spu);
       if(!subt)
          exit = 1;
       
       xthr_lock(show.lock);
       
       if(exit){

          show.finish = 1;
          xthr_unlock(show.lock);
          xthr_cond_signal(show.ready_to_show);
          break;
       }
       
       if(show.seek > subt->start && !show.adjusted){

          xthr_unlock(show.lock);
          continue;
          
       }
       
       if(!show.adjusted){

          time_adjust(show.clock, show.seek, 0, 0);
          show.adjusted = 1;
       }

       
       s1 = subt->start / 3600000;
       r = subt->start % 3600000;
       s2 = r / 60000;
       r = r % 60000;
       s3 = r / 1000;
       r = r % 1000;
       s4 = r;
       
       e1 = subt->end / 3600000;
       r = subt->end % 3600000;
       e2 = r / 60000;
       r = r % 60000;
       e3 = r / 1000;
       r = r % 1000;
       e4 = r;
       printf("[%d:%d:%d,%d --> %d:%d:%d,%d]\n", s1,s2,s3,s4,e1,e2,e3,e4);

       slen = 0;
       
       show.start = subt->start;
       show.end = subt->end;

       for (line = 0; line < subt->lines; line++) {

          if(slen+strlen(subt->text[line])+1 >= 1024)
             break;

          strcpy(show.texts+slen, subt->text[line]);
          slen += strlen(subt->text[line]);
       }
       show.text_len = slen;
       
       
       printf("send_loop: subtitle len = %d, subtitle:%s\n", show.text_len, show.texts);
  
       
       lrnow = time_msec_now(show.clock);
       delt = show.start - lrnow;  /* to millisec */

       if(delt > 0){

          xthr_unlock(show.lock);
          printf("thread run: now = %dms, next sub shown in %dms, during %dms\n", lrnow, delt, show.end-show.start);
          time_msec_sleep(show.clock, delt, NULL);

       }else{

          xthr_unlock(show.lock);
       }

       /*
       ret = sender.subt->go(sender.subt, str, slen, start, end - start);
       */
       xthr_cond_signal(show.ready_to_show);
       
    }
    printf("Before return \n");

    return OS_OK;
 }
 
 int main(int argc, char** argv){

    int ret;

	xrtp_thread_t * th_send = NULL;
    xrtp_thread_t * th_recv = NULL;
    
    show.clock = time_start();
    show.lock = xthr_new_lock();
    show.ready_to_show = xthr_new_cond(XTHREAD_NONEFLAGS);
    show.finish = 0;
    
    show.seek = 1000000;
    show.adjusted = 0;   
	
    th_send = xthr_new(send_loop, NULL, XTHREAD_NONEFLAGS);
    th_recv = xthr_new(recv_loop, NULL, XTHREAD_NONEFLAGS);   
	
    printf("\nTime Tester:{ This is a Timer tester }\n\n");


    printf("Time Test: wait\n");
    xthr_wait(th_send, &ret);
    xthr_wait(th_recv, &ret);

    printf("after 2 waits\n");

    time_end(show.clock);

    return 0;
 }
