/***************************************************************************
                          schedule.c  -  description
                             -------------------
    begin                : Tue Jun 24 2003
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

 /*
  * This is a small scale packet schedule module can only sched limited session
  * The maximum according to the constance MAX_SCHED_SESSION
  */

#include "internal.h"

//#define PSCHED_SIMPLE_LOG
 
#ifdef PSCHED_SIMPLE_LOG
 #define simple_sched_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define simple_sched_log(fmtargs)
#endif

#define MAX_SCHED_SESSION 64

typedef struct simple_sched_s simple_sched_t;
typedef struct ssch_unit_s ssch_unit_t;
typedef struct ssch_rtp_request_s ssch_rtp_request_t;
typedef struct ssch_rtcp_request_s ssch_rtcp_request_t;
 
struct ssch_unit_s{

    xrtp_session_t * session;
    xrtp_hrtime_t period;

    /*
    int max_budget;
    int budget;
    
    int n_rtp_request;
    */
    
    xrtp_hrtime_t rtp_range;  /* The legal ts range */
    xrtp_lrtime_t rtcp_range;

    int delay;   /* the distance b/w unit ts and now */
    
    //int rtp_arrived;
    xrtp_hrtime_t rtp_hrt_arrival;
    
    int rtcp_doing;

    int rtcp_arrived;
    xrtp_lrtime_t rtcp_lrt_arrival;

    ssch_rtp_request_t * rtp_request;
    ssch_rtcp_request_t * rtcp_request;

    xthr_lock_t * lock;
};

 struct ssch_rtp_request_s{

    ssch_rtp_request_t * prev;    /* prev request */
    ssch_rtp_request_t * next;    /* next request */

    xrtp_hrtime_t hrts;   /* timestamp */
    
    ssch_unit_t * ssch_unit;
 };

 struct ssch_rtcp_request_s{

    xrtp_lrtime_t next_lrts;

    ssch_unit_t * ssch_unit;

    int expired;
 };

 struct simple_sched_s {

    struct session_sched_s $iface;

    xrtp_clock_t * clock;
    
    ssch_unit_t * units[MAX_SCHED_SESSION];

    int n_session;

    /* store the index of the current scheduling session in units[] */
    struct ssch_rtp_request_s rtp_queue[MAX_SCHED_SESSION];
    ssch_rtp_request_t * rtp_queue_head;
    
    struct ssch_rtcp_request_s rtcp_queue[MAX_SCHED_SESSION];
    int first_rtp_free_slot;
    int first_rtcp_free_slot;

    xthr_lock_t * lock;
    xthr_lock_t * rtcp_lock;

    xthr_cond_t * wait_rtp_request;
    xthr_cond_t * wait_rtcp_request;
    
    xrtp_thread_t * rtp_run;
    xrtp_thread_t * rtcp_run;

    portman_t * rtp_portman;
    portman_t * rtcp_portman;

    int stop;

    int n_all_rtp_request;
    int n_rtcp_request;
 };

 int simple_sched_done(session_sched_t * sched){

    int ret;

    simple_sched_t * ssch = (simple_sched_t *)sched;

    xthr_lock(ssch->lock);
    ssch->stop = 1;
    xthr_unlock(ssch->lock);

    xthr_wait(ssch->rtp_run, (void*)&ret);
    ssch->rtp_run = NULL;

    xthr_wait(ssch->rtcp_run, (void*)&ret);
    ssch->rtcp_run = NULL;

    time_end(ssch->clock);
    
    xthr_done_lock(ssch->lock);
    xthr_done_cond(ssch->wait_rtp_request);
    xthr_done_cond(ssch->wait_rtcp_request);

    portman_done(ssch->rtp_portman);
    portman_done(ssch->rtcp_portman);

    free(ssch);

    return XRTP_OK;
 }

 int simple_sched_add(session_sched_t * sched, xrtp_session_t * ses){
    
    ssch_unit_t * unit = NULL;

    xrtp_port_t *rtp_port, *rtcp_port;
    
    simple_sched_t * ssch = (simple_sched_t *)sched;
    
    int i, free_slot = 0, found_slot = 0;

    xthr_lock(ssch->lock);

    if(ssch->n_session == MAX_SCHED_SESSION){

       xthr_unlock(ssch->lock);
       return XRTP_EREFUSE;
    }

    session_ports(ses, &rtp_port, &rtcp_port);

    if(portman_add_port(ssch->rtp_portman, rtp_port) < XRTP_OK || portman_add_port(ssch->rtcp_portman, rtcp_port) < XRTP_OK){

       portman_remove_port(ssch->rtp_portman, rtp_port);
       portman_remove_port(ssch->rtcp_portman, rtcp_port);
       
       xthr_unlock(ssch->lock);
       return XRTP_EREFUSE;
    }

    for(i=0; i<MAX_SCHED_SESSION; i++){
      
       if(!ssch->units[i] && !found_slot){

          free_slot = i;
          found_slot = 1;
          break;
       }
       
       if(ses == ssch->units[i]->session){

          xthr_unlock(ssch->lock);
          return XRTP_OK;
       }
    }

    if(!found_slot) free_slot = ssch->n_session;

    simple_sched_log(("simple_sched_add: free_slot = %d\n", free_slot));
    
    unit = (ssch_unit_t *)malloc(sizeof(struct ssch_unit_s));
    if(!unit){

       portman_remove_port(ssch->rtp_portman, rtp_port);
       portman_remove_port(ssch->rtcp_portman, rtcp_port);

       xthr_unlock(ssch->lock);

       return XRTP_EMEM;
    }
    memset(unit, 0, sizeof(struct ssch_unit_s));
    
    unit->session = ses;
    unit->period = session_rtp_period(ses);
    unit->rtp_range = session_rtp_delay(ses);
    unit->rtcp_range = session_rtcp_delay(ses);

    ssch->units[free_slot] = unit;
    ssch->rtp_queue[free_slot].ssch_unit = unit;
    ssch->rtp_queue[free_slot].prev = NULL;     /* Not in queue yet */
    ssch->rtp_queue[free_slot].next = NULL;
    unit->rtp_request = &(ssch->rtp_queue[free_slot]);
    
    ssch->rtcp_queue[free_slot].ssch_unit = unit;
    unit->rtcp_request = &(ssch->rtcp_queue[free_slot]);
    ssch->n_session++;
    
    session_set_schedinfo(ses, (sched_schedinfo_t *)unit);

    simple_sched_log(("simple_sched_add: Session[%d] added to scheduler with sched_info[@%u]\n", session_id(ses), (int)(unit)));
    simple_sched_log(("simple_sched_add: %d sessions in schedule\n", ssch->n_session));

    xthr_unlock(ssch->lock);
    /*
    xthr_cond_signal(ssch->wait_rtcp_request);
    */
    return XRTP_OK;
 }

 int simple_sched_remove(session_sched_t * sched, xrtp_session_t * ses){

    int i;
    xrtp_port_t *rtp_port, *rtcp_port;
    simple_sched_t * ssch = (simple_sched_t *)sched;

    xthr_lock(ssch->lock);
    
    for(i=0; i<MAX_SCHED_SESSION; i++){

       if(ssch->units[i]->session == ses){

          ssch_rtp_request_t * rtp_req = NULL;

          session_ports(ses, &rtp_port, &rtcp_port);

          portman_remove_port(ssch->rtp_portman, rtp_port);
          portman_remove_port(ssch->rtcp_portman, rtcp_port);

          session_set_schedinfo(ses, NULL);
          free(ssch->units[i]);
          ssch->units[i] = NULL;
          ssch->rtcp_queue[i].ssch_unit = NULL;

          /* Remove rtp request */
          rtp_req = &(ssch->rtp_queue[i]);
          if(rtp_req->prev){
            
              rtp_req->prev->next = rtp_req->next;
              
          }else{

              rtp_req->next->prev = NULL;
              ssch->rtp_queue_head = rtp_req->next;
          }
          
          if(rtp_req->next){

              rtp_req->next->prev = rtp_req->prev;

          }
          
          rtp_req->ssch_unit = NULL;
          
          ssch->n_session--;
          break;
       }
    }

    xthr_unlock(ssch->lock);

    return XRTP_OK;
 }

 int simple_sched_rtp_in(session_sched_t * sched, xrtp_session_t * ses, xrtp_hrtime_t hrt_arrival){

    /* Only useful for unithread stylish
    
    simple_sched_t * ssch = (simple_sched_t *)sched;

    ssch_unit_t * unit = (ssch_unit_t *)session_schedinfo(ses);

    if(!unit)
        return XRTP_INVALID;

     xthr_lock(unit->lock); 

    unit->rtp_arrived = 1;
    unit->rtp_hrt_arrival = hrt_arrival;
    
    xthr_unlock(unit->lock); 
    
    xthr_cond_signal(ssch->wait_rtp_request);
    */

    return XRTP_OK;
 }

 int simple_sched_rtcp_in(session_sched_t * sched, xrtp_session_t * ses, xrtp_lrtime_t lrt_arrival){

    ssch_unit_t * unit = (ssch_unit_t *)session_schedinfo(ses);

    if(!unit)
        return XRTP_INVALID;

    /* xthr_lock(unit->lock); */

    simple_sched_log(("simple_sched_rtcp_in: data waiting for session to receive\n"));
    unit->rtcp_arrived = 1;
    unit->rtcp_lrt_arrival = lrt_arrival;

    /* xthr_unlock(unit->lock); */

    return XRTP_OK;
 }

 /* Not used recently */
 int simple_sched_rtcp_out(session_sched_t * sched, xrtp_session_t * ses){

    simple_sched_t * ssch = (simple_sched_t *)sched;

    xthr_cond_signal(ssch->wait_rtcp_request);
    
    return XRTP_OK;
 }

 /* 
 int simple_hrts_earlier(t1, t2){

    xrtp_hrtime_t d = t2 - t1;
    if(d > 0) return 1;

    return 0;
 }
 */
 
 int simple_sched_rtp_out(session_sched_t * sched, xrtp_session_t * ses, xrtp_hrtime_t hrt_allow, int sync){

    simple_sched_t * ssch = (simple_sched_t *)sched;
    ssch_unit_t * unit = (ssch_unit_t *)session_schedinfo(ses);
    ssch_rtp_request_t * rtp_req = unit->rtp_request;

    xrtp_hrtime_t hrt_now = 0;
    xrtp_hrtime_t dl = 0;

    if(!unit)
        return XRTP_INVALID;
    
    xthr_lock(ssch->lock);
    
    if(unit->delay){
      
        return XRTP_EREFUSE;
    }
    
    /* Calc the new ts and request number to catch up the delay */
    hrt_now = time_nsec_now(ssch->clock);
    dl = hrt_now + hrt_allow;
    
    if(!rtp_req->prev && !rtp_req->next && ssch->rtp_queue_head != rtp_req){

        simple_sched_log(("simple_sched_rtp_out: Clock[@%u] is hrtime[%d] now, next deadline[%d] expired in %d nanosecs, request added\n", (int)(ssch->clock), hrt_now, dl, hrt_allow));

        rtp_req->hrts = dl;
        
    }else if(!sync){

        /* FIXME: The chance to check if time over buffer */
        return XRTP_OK; /* needn't queuing */
    }

    if(sync){

        rtp_req->hrts = dl;
        if(ssch->rtp_queue_head == rtp_req) ssch->rtp_queue_head = rtp_req->next;
        if(rtp_req->next) rtp_req->next->prev = rtp_req->prev;
        if(rtp_req->prev) rtp_req->prev->next = rtp_req->next;
        rtp_req->prev = rtp_req->next = NULL;
    }
    
    if(ssch->rtp_queue_head == NULL || (rtp_req->hrts - ssch->rtp_queue_head->hrts) < 0){

       rtp_req->prev = NULL;
       rtp_req->next = ssch->rtp_queue_head;
       ssch->rtp_queue_head = rtp_req;
       if(rtp_req->next)
          rtp_req->next->prev = rtp_req;
       
    }else{
    
       ssch_rtp_request_t * req = ssch->rtp_queue_head;
       while(req->next && (req->next->hrts - rtp_req->hrts) <= 0){
      
          req = req->next;
       }

       rtp_req->next = req->next;
       rtp_req->prev = req;

       req->next = rtp_req;
       rtp_req->next->prev = rtp_req;
    }
    
    xthr_unlock(ssch->lock);
    
    xthr_cond_signal(ssch->wait_rtp_request);
    
    return XRTP_OK;
 }

int simple_schedule_rtp(void * gen){

   simple_sched_t * ssch = (simple_sched_t *)gen;

   xrtp_hrtime_t now;
     
   ssch_unit_t * unit;

   simple_sched_log(("simple_schedule_rtp: rtp scheduler started\n"));

   while(1){

      xrtp_hrtime_t hrt_sleep = 0;
      ssch_rtp_request_t * req = NULL;
      xrtp_hrtime_t hrt_remain = 0;

      /* Check if need to quit */
      if(xthr_lock(ssch->lock) >= XRTP_OK){

         if(ssch->stop) break;

         if(ssch->rtp_queue_head == NULL){

            /* Time to schedule rtcp */
            /* xthr_cond_signal(sched->wait_on_rtcp); */
            simple_sched_log(("simple_schedule_rtp: rtp scheduler waiting for request\n"));
            xthr_cond_wait(ssch->wait_rtp_request, ssch->lock);
            simple_sched_log(("simple_schedule_rtp: Wake up, Got a rtp request!\n"));
         }
      }

      /* got the lock */
      now = time_nsec_now(ssch->clock);

      /* get a nearest future deadline
       * if a future already passed, what to do with it
       * depends on how late we are,
       * - action now if in tolarent range
       * - abandon otherwise
       */

      simple_sched_log(("simple_schedule_rtp: next request[@%u]\n", (int)(ssch->rtp_queue_head)));
      req = ssch->rtp_queue_head;
      
      while(req){

         unit = req->ssch_unit;

         hrt_sleep = req->hrts - now;
         
         if(hrt_sleep > 0) break;

         /* receive rtp if arrived 
         if(unit->rtp_arrived){

            session_rtp_to_receive(unit->session);
            unit->rtp_arrived = 0;
         }
         */
            
         /* Remove from the rtp queue */
            
         ssch->rtp_queue_head = req->next;
         if(req->next) req->next->prev = NULL;
         req->next = req->prev = NULL;
            
         simple_sched_log(("simple_schedule_rtp: hrtimer[%d] now, send rtp packet !\n", now));

         session_rtp_send_now(unit->session);
            
         req = ssch->rtp_queue_head;
      }
         
      /* before sleep check if need to quit */
      if(ssch->stop) break;
         
      xthr_unlock(ssch->lock);

      /* To detect all ports see if any incoming 
         simple_sched_log("simple_schedule_rtp: to scan the rtp ports\n");
         portman_poll(ssch->rtp_portman);

         NOTE: this is moved to seperate thread in session module
      */
         
      simple_sched_log(("simple_schedule_rtp: sleep %d nanosecs\n", hrt_sleep));         
      time_nsec_sleep(ssch->clock, hrt_sleep, &hrt_remain);            
   }

   xthr_unlock(ssch->lock);

   return XRTP_OK;
}

int simple_schedule_rtcp(void * gen){

   simple_sched_t * ssch = (simple_sched_t *)gen;

   xrtp_lrtime_t now, dt;
   ssch_unit_t * unit;

   int i_locked_ssch = 0;

   simple_sched_log(("simple_schedule_rtcp: rtcp scheduler started\n"));

   while(1){

      xrtp_lrtime_t dt_min = 0;
      int i, n_expired = 0;
      int n_arrived = 0;
      int nses_scaned = 0;

      if (xthr_lock(ssch->lock) == OS_OK) {

         if(ssch->stop){

            xthr_unlock(ssch->lock);
            break;
         }
         
         i_locked_ssch = 1;
      }

      xthr_lock(ssch->rtcp_lock);

      if(ssch->n_rtcp_request == 0){

         if(i_locked_ssch){

            xthr_unlock(ssch->lock);
            i_locked_ssch = 0;
         }

         simple_sched_log(("simple_schedule_rtcp: rtcp sched idles ...\n"));
         xthr_cond_wait(ssch->wait_rtcp_request, ssch->rtcp_lock);
         simple_sched_log(("simple_schedule_rtcp: Wake up!\n"));
      }

      if(i_locked_ssch){

         xthr_unlock(ssch->lock);
         i_locked_ssch = 0;
      }

      now = time_msec_now(ssch->clock);
      simple_sched_log(("simple_schedule_rtcp: clock[@%u] is %d csec now\n", (int)(ssch->clock), now));

      /* Scan if some deadline is reached */

      /* get a nearest future deadline
       * if a future already passed, what to do with it
       * depends on how late we are,
       * - action now if in tolarent range
       * - abandon otherwise
       */
      for(i=0; i<MAX_SCHED_SESSION; i++){

         if(nses_scaned == ssch->n_session) break;

         unit = ssch->rtcp_queue[i].ssch_unit;
         if(unit){

            nses_scaned++;

            if(unit->rtcp_arrived){

               n_arrived++;
            }

            if(!session_need_rtcp(unit->session)){

               int rtcp_wakeup = 0;

               /* Stop sending rtcp */
               if(unit->rtcp_doing){

                  unit->rtcp_doing = 0;
                  ssch->rtcp_queue[i].expired = 0;
                  ssch->n_rtcp_request--;
                  if(ssch->n_rtcp_request == 0){

                     simple_sched_log(("simple_schedule_rtcp: rtcp scheduler waiting for quest ...\n"));
                     xthr_cond_wait(ssch->wait_rtcp_request, ssch->rtcp_lock);
                     simple_sched_log(("simple_schedule_rtcp: Wake up! rtcp request pending ...\n"));
                     rtcp_wakeup = 1;
                  }
               }

               if(!rtcp_wakeup) continue;
            }

            if(!unit->rtcp_doing){

               /* Resume/Start the rtcp */
               if(ssch->n_rtcp_request == 0){

                  dt_min = session_rtcp_interval(unit->session);
                  ssch->rtcp_queue[i].next_lrts = now + dt_min;

               }else{

                  ssch->rtcp_queue[i].next_lrts = now + session_rtcp_interval(unit->session);
               }

               unit->rtcp_doing = 1;
               ssch->n_rtcp_request++;

               simple_sched_log(("simple_schedule_rtcp: first rtcp at %u csec\n", ssch->rtcp_queue[i].next_lrts));
               continue;
            }

            dt = ssch->rtcp_queue[i].next_lrts - now;

            if (dt_min == 0 && dt > 0) dt_min = dt;

            if (dt > 0 && dt < dt_min) {

               dt_min = dt;
               ssch->rtcp_queue[i].expired = 0;

            } else if(dt <= 0) {

               /* A deadline reached, assume all the dealine will be kept
                * May consider formula: (0-dt) < unit->range
                */

               /* Send rtcp now
                * simple_sched_log("simple_schedule_rtcp: session[%d] expired, send now\n", i);
                */
               ssch->rtcp_queue[i].expired = 1;
               n_expired++;
            }
         }
      }

      /* If any session need to send rtcp */
      if(n_expired){

         for(i=0; i<MAX_SCHED_SESSION; i++){

            if(n_expired == 0) break;

            if(ssch->rtcp_queue[i].expired){

               unit = ssch->rtcp_queue[i].ssch_unit;

               if(unit->rtcp_arrived){

                  simple_sched_log(("simple_schedule_rtcp: Session[%d] receive data before send!\n", session_id(unit->session)));
                  session_rtcp_to_receive(unit->session);
                  unit->rtcp_arrived = 0;
                  n_arrived--;
               }

               simple_sched_log(("simple_schedule_rtcp: Session[%d] send rtcp on time[%u]\n", session_id(unit->session), now));
               session_rtcp_to_send(unit->session);

               /* calcu the next ts */
               dt = session_rtcp_interval(unit->session);

               simple_sched_log(("simple_schedule_rtcp: session[%d]'s next rtcp in %u csec\n", i, dt));

               ssch->rtcp_queue[i].next_lrts = now + dt;
               
               if (dt < dt_min ) dt_min = dt;

               ssch->rtcp_queue[i].expired = 0;
               n_expired--;
            }
         }
      }

      /* If any session need to receive rtcp */
      if(n_arrived){

         for(i=0; i<MAX_SCHED_SESSION; i++){

            if(n_arrived == 0) break;

            unit = ssch->rtcp_queue[i].ssch_unit;

            if(unit->rtcp_arrived){

               simple_sched_log(("simple_schedule_rtcp: session receive data NOW !\n"));
               session_rtcp_to_receive(unit->session);
               unit->rtcp_arrived = 0;
               n_arrived--;
            }
         }
      }

      /* Before sleep, set signal handler for interrupt */
      if(ssch->stop){

         xthr_unlock(ssch->rtcp_lock);
         break;
      }

      /* To detect all ports see if any incoming */
      simple_sched_log(("simple_schedule_rtcp: to scan the rtcp ports\n"));
      portman_poll(ssch->rtcp_portman);

      xthr_unlock(ssch->rtcp_lock);

      simple_sched_log(("simple_schedule_rtcp: sleep %u msec\n", dt_min));
      time_msec_sleep(ssch->clock, dt_min, NULL);
   }

   return XRTP_OK;
}

session_sched_t * sched_new(){

   simple_sched_t * ssch = (simple_sched_t *)malloc(sizeof(struct simple_sched_s));

   if(!ssch) return NULL;

   memset(ssch, 0, sizeof(struct simple_sched_s));  /* zeroed */

   ssch->clock = time_start();

   if(!ssch->clock){

      free(ssch);

      return NULL;
   }
    
   ssch->$iface.done = simple_sched_done;

   ssch->$iface.add = simple_sched_add;
   ssch->$iface.remove = simple_sched_remove;

   ssch->$iface.rtp_out = simple_sched_rtp_out;
   ssch->$iface.rtp_in = simple_sched_rtp_in;
   ssch->$iface.rtcp_out = simple_sched_rtcp_out;
   ssch->$iface.rtcp_in = simple_sched_rtcp_in;

   ssch->lock = xthr_new_lock();
   ssch->rtcp_lock = xthr_new_lock();
    
   ssch->wait_rtp_request = xthr_new_cond(XTHREAD_NONEFLAGS);
   ssch->wait_rtcp_request = xthr_new_cond(XTHREAD_NONEFLAGS);

   /* set ports detecter */
   ssch->rtp_portman = portman_new();
   ssch->rtcp_portman = portman_new();
   if(!ssch->rtp_portman || !ssch->rtcp_portman){

      if(ssch->rtp_portman) portman_done(ssch->rtp_portman);
      if(ssch->rtcp_portman) portman_done(ssch->rtcp_portman);
       
      free(ssch);
      return NULL;
   }
    
   ssch->stop = 0;
   ssch->rtcp_run = xthr_new(simple_schedule_rtcp, ssch, XTHREAD_NONEFLAGS);
   ssch->rtp_run = xthr_new(simple_schedule_rtp, ssch, XTHREAD_NONEFLAGS);

   simple_sched_log(("sched_new: Sched[@%u] start ...\n", (int)(ssch)));
    
   return (session_sched_t *)ssch;
}
