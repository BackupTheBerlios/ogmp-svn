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

 #include "internal.h"

 struct sched_unit_s{

    packet_pipe_t * pipe;
    xrtp_session_t * session;
    session_sched_t * sched;

    xrtp_hrtime_t dl;   /* packet timestamp */

    xrtp_hrtime_t period;

    int max_budget;
    int num_quest;

    int delay; /* the distance b/w unit ts and now */
 };

 struct sched_rtcp_unit_s{

    packet_pipe_t * pipe;
    xrtp_session_t * session;
    session_sched_t * sched;

    xrtp_lrtime_t next_ts;
    xrtp_lrtime_t interval;
 };

 struct session_sched_s {

    xrtp_clock_t * clock;
    
    xrtp_list_t * units;

    xrtp_list_t * rtp_q;

    xrtp_list_t * rtcp_q;

    xthr_lock_t * lock;
    xthr_lock_t * lock_rtcp;

    xthr_cond_t * wait_more_quest;
    xthr_cond_t * wait_rtcp_quest;
    
    xrtp_thread_t * sched_rtp_runner;
    xrtp_thread_t * sched_rtcp_runner;

    int stop;

    int num_quest;
 };

 int sched_schedule_rtp(void * gen);
 int sched_schedule_rtcp(void * gen);
 
 session_sched_t * sched_new(xrtp_clock_t * clock){

    session_sched_t * sched = (session_sched_t *)malloc(sizeof(struct session_sched_s));

    if(!sched){
       
       return NULL;
    }
    
    sched->clock = clock;
    
    sched->units = xrtp_list_new();
    
    sched->rtp_q = xrtp_list_new();

    sched->rtcp_q = xrtp_list_new();

    sched->lock = xthr_new_lock();
    sched->wait_more_quest = xthr_new_cond(XTHREAD_NONEFLAGS);
    sched->wait_rtcp_quest = xthr_new_cond(XTHREAD_NONEFLAGS);

    sched->sched_rtp_runner = NULL;
    sched->sched_rtcp_runner = NULL;
    
    return sched;
 }

 int sched_done_unit(void * unit){

    free(unit);

    return XRTP_OK;
 }

 int sched_done(session_sched_t * sched){

    if(sched->sched_rtp_runner)
        return XRTP_EBUSY;

    xthr_done_lock(sched->lock);
    xthr_done_cond(sched->wait_more_quest);
    xthr_done_cond(sched->wait_rtcp_quest);

    xrtp_list_free(sched->units, sched_done_unit);
    
    xrtp_list_free(sched->rtp_q, sched_done_unit);
    
    xrtp_list_free(sched->rtcp_q, sched_done_unit);

    free(sched);

    return XRTP_OK;
 }

 int sched_start(session_sched_t * sched){

    sched->sched_rtcp_runner = xthr_new(sched_schedule_rtcp, sched, XTHREAD_NONEFLAGS);
    sched->sched_rtp_runner = xthr_new(sched_schedule_rtp, sched, XTHREAD_NONEFLAGS);

    return XRTP_OK;
 }

 int sched_stop(session_sched_t * sched){

    int ret;

    xthr_lock(sched->lock);
    sched->stop = 1;
    xthr_unlock(sched->lock);

    xthr_wait(sched->sched_rtp_runner, (void*)&ret);
    sched->sched_rtp_runner = NULL;
    
    xthr_wait(sched->sched_rtcp_runner, (void*)&ret);
    sched->sched_rtcp_runner = NULL;    

    return XRTP_OK;
 }

 int sched_match_session(void * target, void * pattern){

    sched_unit_t * unit = (sched_unit_t *)target;

    return unit->session == (xrtp_session_t *)pattern;
 }

 int sched_add(session_sched_t * sched, xrtp_session_t * ses){
    
    sched_unit_t * rtp_unit = NULL;
    sched_rtcp_unit_t * rtcp_unit = NULL;
    
    xrtp_list_user_t * lu = xrtp_list_newuser(sched->units);
    if(!lu) return XRTP_FAIL;
    
    xthr_lock(sched->lock);
    
    if(xrtp_list_find(sched->units, ses, sched_match_session, lu)){

       xthr_unlock(sched->lock);
       xrtp_list_freeuser(lu);

       return XRTP_OK;
    }

    rtp_unit = (sched_unit_t *)malloc(sizeof(struct sched_unit_s));
    if(!rtp_unit){

       xthr_unlock(sched->lock);
       xrtp_list_freeuser(lu);

       return XRTP_EMEM;
    }
       
    if(xrtp_list_find(sched->rtcp_q, ses, sched_match_session, lu)){

       xthr_unlock(sched->lock);
       xrtp_list_freeuser(lu);

       return XRTP_OK;
    }
          
    rtcp_unit = (sched_rtcp_unit_t *)malloc(sizeof(struct sched_rtcp_unit_s));
    if(!rtcp_unit){

        if(rtp_unit) free(rtcp_unit);

        xthr_unlock(sched->lock);
        xrtp_list_freeuser(lu);
        
        return XRTP_EMEM;
    }

    if(rtp_unit){

       rtp_unit->pipe = ses->rtp_send_pipe;
       rtp_unit->session = ses;
       rtp_unit->sched = sched;
       rtp_unit->dl = HRTIME_INFINITY;
       rtp_unit->period = session_rtp_period(ses);
       rtp_unit->max_budget = session_rtp_bandwidth(ses);

       xrtp_list_add_first(sched->units, rtp_unit);
       session_set_rtp_schedinfo(ses, rtp_unit);
    }

    xthr_unlock(sched->lock);

    if(rtcp_unit){

       rtcp_unit->pipe = ses->rtcp_send_pipe;
       rtcp_unit->session = ses;
       rtcp_unit->sched = sched;
       rtcp_unit->next_ts = -1;
       rtcp_unit->interval = session_rtcp_interval(ses);
       
       xthr_lock(sched->lock_rtcp);
       
       xrtp_list_add_first(sched->rtcp_q, rtcp_unit);
       session_set_rtcp_schedinfo(ses, rtcp_unit);

       xthr_unlock(sched->lock_rtcp);
    }
    
    xrtp_list_freeuser(lu);

    return XRTP_OK;
 }

 int sched_remove(session_sched_t * sched, xrtp_session_t * ses){

    xthr_lock(sched->lock);
    xrtp_list_remove(sched->rtp_q, ses, sched_match_session);
    session_set_rtp_schedinfo(ses, NULL);
    xthr_unlock(sched->lock);

    xthr_lock(sched->lock_rtcp);
    xrtp_list_remove(sched->rtcp_q, ses, sched_match_session);
    session_set_rtcp_schedinfo(ses, NULL);
    xthr_unlock(sched->lock_rtcp);
    
    return XRTP_OK;
 }

 int sched_rtcp_newer(void * tar, void * pat){

    sched_rtcp_unit_t * rtcp = (sched_rtcp_unit_t *)tar;
    sched_rtcp_unit_t * newunit = (sched_rtcp_unit_t *)pat;

    return lrtime_newer(rtcp->sched->clock, rtcp->next_ts, newunit->next_ts);
 }

 int sched_rtp_newer(void * tar, void * pat){

    sched_unit_t * rtp = (sched_unit_t *)tar;
    sched_unit_t * newunit = (sched_unit_t *)pat;

    return time_newer(rtp->sched->clock, rtp->dl, newunit->dl);
 }

 int sched_rtp_sending(session_sched_t * sched, xrtp_session_t * ses, xrtp_hrtime_t ts){

    sched_unit_t * rtpu = (sched_unit_t *)session_rtp_schedinfo(ses);

    if(!rtpu)
        return XRTP_INVALID;

    xthr_lock(sched->lock);
    
    if(rtpu->delay){
      
        return XRTP_EREFUSE;
    }
    
    if(rtpu->num_quest){

        rtpu->num_quest++;
        xthr_unlock(sched->lock);
        
        return XRTP_OK;
    }

    rtpu->dl = ts;
    /* FIXME: Maybe multiaddition for one schedule unit */
    xrtp_list_add_ascent_once(sched->rtp_q, rtpu, sched_rtp_newer);
    
    xthr_unlock(sched->lock);
    return XRTP_OK;
 }

 int sched_catchup_rtp(session_sched_t * sched, xrtp_session_t * ses, xrtp_hrtime_t ts){

    sched_unit_t * rtpu = (sched_unit_t *)session_rtp_schedinfo(ses);
    xrtp_hrtime_t dt;
    int nq;

    if(!rtpu)
        return XRTP_INVALID;

    xthr_lock(sched->lock);

    if(!rtpu->delay){

        return XRTP_EREFUSE;
    }

    dt = time_dec(sched->clock, ts, rtpu->dl);
    nq = dt / rtpu->period;
    if(rtpu->num_quest > nq){

        rtpu->dl = time_inc(sched->clock, rtpu->dl, rtpu->period * nq);
        
    }else{

        rtpu->dl = ts;
    }

    rtpu->delay = 0;
    xrtp_list_add_ascent_once(sched->rtp_q, rtpu, sched_rtp_newer);
    
    xthr_unlock(sched->lock);
    return XRTP_OK;   
 }

 int sched_schedule_rtp(void * gen){

     session_sched_t * sched = (session_sched_t *)gen;

     xrtp_hrtime_t now, now2, dt, now_send, next_ts, pass, wt;
     
     sched_unit_t * unit;
     int budget;
     
     int nts; /* how many timeslot complete */

     xrtp_list_user_t * lu = xrtp_list_newuser(NULL);

     while(1){

         /* Check if need to quit */
         if(xthr_lock(sched->lock) >= XRTP_OK){

            if(sched->stop){
                break;
            }

            if(!sched->num_quest){

                /* Time to schedule rtcp */
                /* xthr_cond_signal(sched->wait_on_rtcp); */
                xthr_cond_wait(sched->wait_more_quest, sched->lock);
            }
         }

         /* got the lock */
         now = time_now(sched->clock);
         unit = xrtp_list_first(sched->rtp_q, lu);
         
         dt = time_dec(sched->clock, unit->dl, now);
         if(dt > 0){

            xthr_unlock(sched->lock);
            
            /* before sleep check if need to quit */
            if(sched->stop)
                break;

            hrtime_sleep(sched->clock, dt);
            continue;
         }

         /* reached the deadline */
         dt = -dt;

         budget = (dt / unit->period + 1) * unit->max_budget;
         
         {

              xrtp_list_remove_first(sched->rtp_q);
              
              now_send = time_now(sched->clock);
              pipe_complete(unit->pipe, now, budget, &next_ts, &nts);
              
              /* calcu the num_quest */
              unit->num_quest -= nts;
              
              sched->num_quest -= nts;
              if(!sched->num_quest){

                  /* xthr_cond_signal(sched->wait_on_rtcp); */
                  xthr_cond_wait(sched->wait_more_quest, sched->lock);
                  continue;
              }
              
              if(unit->num_quest){
                
                  unit->dl = next_ts;
              
                  dt = time_dec(sched->clock, next_ts, now);
                  if(dt < 0){

                      /* the packet sending is lag, do sth. */
                      unit->delay = 1;
                      
                      /*
                       * the unit with much delay will be pull out of queue,
                       * No delayed unit can affect the coming unit.
                       * The delayed unit has be notified to the session for further
                       * process
                       */
                      session_notify_delay(unit->session, dt);
                      
                  }else{
              
                      xrtp_list_add_ascent_once(sched->rtp_q, unit, sched_rtp_newer);
                  }
              }
         }

         xthr_unlock(sched->lock);
         
         /* before sleep check if need to quit */
         if(sched->stop)
            break;
         
         now2 = time_now(sched->clock);
         
         pass = time_dec(sched->clock, now2, now_send);
         
         dt = time_dec(sched->clock, next_ts, now2);

         wt = dt + pass > unit->period ? dt : unit->period - pass;
         
         /* may schedule rtcp */
         /* xthr_cond_signal(sched->wait_on_rtcp); */
         
         hrtime_sleep(sched->clock, wt);
     }

     return XRTP_OK;
 }

 int sched_schedule_rtcp(void * gen){

    session_sched_t * sched = (session_sched_t *)gen;

    xrtp_lrtime_t now, delta;
    sched_rtcp_unit_t * rtcp;
    int cond_wake = 0;
    
    while(1){
      
        if(!cond_wake && xthr_lock(sched->lock) >= XRTP_OK){

           if(sched->stop){
              xthr_unlock(sched->lock);
              break;
           }

           xthr_unlock(sched->lock);

        }else{

            if(!cond_wake){
                cond_wake = 0;
                xthr_lock(sched->lock_rtcp);
            }
          
            now = lrtime_now(sched->clock);

            rtcp = (sched_rtcp_unit_t *)xrtp_list_remove_first(sched->rtcp_q);
            if(!rtcp){

                xthr_cond_wait(sched->wait_rtcp_quest, sched->lock_rtcp);
                cond_wake = 1;
                continue;
            }
            
            while( rtcp->next_ts < now ){

                session_rtcp_to_send(rtcp->session);

                rtcp->next_ts += session_rtcp_interval(rtcp->session);

                xrtp_list_add_ascent_once(sched->rtcp_q, rtcp, sched_rtcp_newer);

                rtcp = (sched_rtcp_unit_t *)xrtp_list_remove_first(sched->rtcp_q);
            }

            xthr_lock(sched->lock_rtcp);

            now = lrtime_now(sched->clock);

            delta = rtcp->next_ts - now;

            /* Before sleep, set signal handler for interrupt */

            lrtime_sleep(sched->clock, delta);
        }
    }
    
    return XRTP_OK;
 }
