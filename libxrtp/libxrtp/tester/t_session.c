/***************************************************************************
                          t_session.c  -  session tester
                             -------------------
    begin                : Tue Nov 11 2003
    copyright            : (C) 2003 by Heming Ling
    email                : heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "../xrtp.h"
 
#include "spu_text.h"
#include <string.h>                           
#include <stdlib.h>

typedef struct subt_frame_s{

   xrtp_hrtime_t start;
   media_time_t dur;

   int len;
   char *data;
    
} subt_frame_t;
 
typedef struct subt_sender_s subt_sender_t;

struct subt_sender_s{

   xrtp_session_t *  session;
    
   xrtp_media_t *    subt;
    
   demux_sputext_t * spu;
    
   int need_seek;
   int seek_time;

   xrtp_clock_t * clock;

   xthr_lock_t * lock;
   xthr_cond_t * wait;
   
   int n_recvr;
   xthr_cond_t * wait_receiver;

   int end;
    
} sender;
 
typedef struct subt_recvr_s subt_recvr_t;

struct subt_recvr_s{

   xrtp_session_t *  session;
   xrtp_media_t *    subt;

   media_time_t next_ts;

   xrtp_list_t * frames;
   xrtp_clock_t * clock;
   
   xthr_lock_t * lock;
   xthr_cond_t * wait;

   int end;

   int tolerant_delay_ns;

} recvr;

void subt_show(subt_frame_t * frm){

   char str[SUB_MAXLEN];

   memcpy(str, frm->data, frm->len);
   str[frm->len] = '\0';
    
   printf("\n\n['%s']\n\n", str);

   return;
}

int cb_media_sent(void * u, xrtp_media_t * media){

   subt_sender_t * s = (subt_sender_t *)u;
   printf("media_sent: one unit sent\n");
   xthr_cond_signal(s->wait);

   return XRTP_OK;
}

 int spu_sender_run(void * param){

    subtitle_t * subt;
    char str[1024];
    int slen = 0;

    int delt = 0;

    xrtp_lrtime_t lrnow, lrbegin;
    
    int line, tryagain = 0, first = 1;

    printf("\nspu_sender_run: sender started ...\n");
    
    while(1){

       xthr_lock(sender.lock);
       
       if(sender.n_recvr == 0){

          printf("spu_sender_run: waiting for receiver ...\n");
          xthr_cond_wait(sender.wait_receiver, sender.lock);
          printf("spu_sender_run: Wake up, got a receiver ...\n");
       }
       
       if(!tryagain){

         if(sender.need_seek){

            demux_sputext_seek(sender.spu, sender.seek_time);
            sender.need_seek = 0;
         }
         
         subt = demux_sputext_next_subtitle(sender.spu);
         if(!subt) break;

         //start = (sender.spu->uses_time) ? subt->start * 10 : subt->start;
         //end = (sender.spu->uses_time) ? subt->end * 10 : subt->end;

         slen = 0;
         for (line = 0; line < subt->lines; line++) {

            if(slen+strlen(subt->text[line])+1 >= 1024)
                break;
                
            strcpy(str+slen, subt->text[line]);
            slen += strlen(subt->text[line]);
         }
         
       }else{

          xthr_cond_wait(sender.wait, sender.lock);
       }

       if(first){

          first = 0;
          lrbegin = time_msec_now(sender.clock);
          lrnow = lrbegin;
          printf("spu_sender_run: Got lrtime first lrtime[%d]\n", lrbegin);

       }else{

          lrnow = time_msec_now(sender.clock);
          printf("spu_sender_run: Get lrtime[%d]\n", lrnow);
       }

       delt = subt->start_ms - lrnow;
       if(delt > 0){

          printf("spu_sender_run: sleep %u msec\n", (uint)delt);
          
          xthr_unlock(sender.lock);
          time_msec_sleep(sender.clock, (xrtp_lrtime_t)delt, NULL);

       }else{

          xthr_unlock(sender.lock);
       }
       
       /* This is for debug perposal only
       demux_sputext_show_title(subt);
       */
       
       { /* Send really */

          int ret = sender.subt->post(sender.subt, str, slen, (subt->end_ms - subt->start_ms)*1000);
          if(ret == XRTP_EAGAIN){

              tryagain = 1;
              continue;
          }

          tryagain = 0;
          
       } /* Send really */
    }

    xthr_unlock(sender.lock);
    
    printf("\nspu_sender_run: end thread ...\n");

    return XRTP_OK;
 }

 int spu_done_frame(void * gen){

    free(((subt_frame_t*)gen)->data);
    free(gen);

    return XRTP_OK;
 }
 
 int frame_cmp_mts(void * targ, void * patt){

    subt_frame_t *f_targ = (subt_frame_t *)targ;
    subt_frame_t *f_patt = (subt_frame_t *)patt;

    return f_targ->start - f_patt->start;
 }
 
 int cb_media_recvd(void* u, char *data, int dlen, uint32 ssrc, xrtp_hrtime_t later, media_time_t dur){

    subt_recvr_t * r = (subt_recvr_t *)u;
    subt_frame_t * frm = NULL;
    /* if the ts is passed, the frame is discarded, otherwise keep for show */

    printf("test.cb_media_recvd: media is ready to play in %dns\n", later);
    xthr_lock(r->lock);
    
    if(later < 0 ) {

      if ( -later > r->tolerant_delay_ns ) {

         /* the frame is stale, discard */
         free(data);
         
         xthr_unlock(r->lock);
         printf("< test.cb_media_recvd: discard staled media >\n");
         return XRTP_OK;

      } else {

         later = 0;
      }
    }
      
    frm = malloc(sizeof(struct subt_frame_s));
    if(!frm){

        xthr_unlock(r->lock);
        printf("< cb_media_recvd: Fail to allocated frame memery >\n");
        return XRTP_EMEM;
    }
      
    frm->start = time_nsec_now(r->clock) + later;
    frm->dur = dur;
    frm->len = dlen;
    frm->data = data;
    
    xrtp_list_add_ascent_once(r->frames, frm, frame_cmp_mts);
    
    xthr_unlock(r->lock);

    xthr_cond_signal(r->wait);

    return XRTP_OK;
 }

 /* make no sense */
 xrtp_hrtime_t cb_mt2hrt(media_time_t ts){

    //printf("test.cb_mt2hrt: mt = %d, hrt = %d\n", ts, ts * 10000000);
    return (xrtp_hrtime_t)(ts * 1000000);
 }

 /* make no sense */
 media_time_t cb_hrt2mt(xrtp_hrtime_t ts){

    return ts / 1000000;
 }

 int cb_member_update(void* user, uint32 member_src, char * cname, int clen){

    cname[clen] = '\0';
    
    if(user == &sender){
      
        printf("tester.cb_member_update: '%s' accepted\n", cname);
        sender.n_recvr++;
        xthr_cond_signal(sender.wait_receiver);
    }
    
    return XRTP_OK;
 }
 
 int cb_member_deleted(void* user, uint32 member_src, void * user_info){

    if(user == &sender){

        sender.n_recvr--;
    }
    
    return XRTP_OK;
 }

 int spu_recvr_run(void * param){

    subt_frame_t *f;
    
    xrtp_hrtime_t now, dt, mts;
    
    struct xrtp_list_user_s lu;
    
    printf("\nspu_recvr_run: start thread ...\n");
    
    while(1){

       xthr_lock(recvr.lock);
       
       f = (subt_frame_t *)xrtp_list_first(recvr.frames, &lu);
       
       if(!f && recvr.end){
         
          xthr_unlock(recvr.lock);
          break;
       }

       if(!f && !recvr.end){
         
          printf("spu_recvr_run: No more media to play, idle. zzzzzzzzzzz\n");
          xthr_cond_wait(recvr.wait, recvr.lock);
          printf("spu_recvr_run: Wake up, Media received\n");
          f = (subt_frame_t *)xrtp_list_first(recvr.frames, &lu);
       }

       printf("spu_recvr_run: Frame[@%d]\n", (int)(f));
       mts = f->start;
       
       printf("spu_recvr_run: Media ts is hrt[%d]\n", mts);
       now = time_nsec_now(recvr.clock);
       
       dt = f->start - now;
       printf("spu_recvr_run: hrt[%d] now, dt=%d\n", now, dt);
       if(dt > 0){
          
          /* snap sometime */
          xthr_unlock(recvr.lock);
          printf("spu_recvr_run: Media plays in %d nsecs, sleep\n", dt);
          time_nsec_sleep(recvr.clock, dt, NULL);
          continue;
       }

       f = (subt_frame_t *)xrtp_list_remove_first(recvr.frames);
       xthr_unlock(recvr.lock);
       
       subt_show(f);
       spu_done_frame(f);
    }

    printf("\nspu_recvr_run: end thread ...\n");
    
    return XRTP_OK;
 }

 void test_xthr(){

    xthr_lock_t * lock = xthr_new_lock();
    printf("test_xthr: lock ok\n");
    xthr_done_lock(lock);
 }
 
 #include <timedia/inet.h>

 int main(int argc, char** argv){

    int8 profile_no = 96; /* dynamic assignment */
    
    char * title = "subtitle.srt";                         
    char * plugid = "text/rtp-test";

    FILE * f;
    demux_sputext_t * spu = NULL;
    xrtp_port_t * sender_rtp_port = NULL;
    xrtp_port_t * sender_rtcp_port = NULL;

    char cname_sender[] = "spu_sender";
    xrtp_session_t *ses = NULL; 

    profile_handler_t * mh = NULL;
    xrtp_media_t * med = NULL;

    /* var for receiver */
    xrtp_port_t * recvr_rtp_port = NULL;
    xrtp_port_t * recvr_rtcp_port = NULL;
    char cname_recvr[] = "spu_recvr";

    int ret;
    xrtp_thread_t * th_recvr = NULL;
    xrtp_thread_t * th_sender = NULL;
        
    xrtp_teleport_t * tp_rtp = NULL;
    xrtp_teleport_t * tp_rtcp = NULL;
    
	printf("Session Tester: FD_SETSIZE = %u", FD_SETSIZE);
    printf("\nSession Tester:{ This is a RTP Session tester }\n\n");

    f = fopen(title, "r");

    spu = demux_sputext_new(f);
    printf("Session tester: Subtitle specialised\n");

    //fclose(f);
    
    /* Realised the xrtp lib */
    if(xrtp_init() < XRTP_OK)
        return -1;

    /* Create rtp port */
    sender_rtp_port = port_new("127.0.0.1", 9000, RTP_PORT);
    sender_rtcp_port = port_new("127.0.0.1", 9001, RTCP_PORT);
    printf("Sending tester: port pair created!\n");

    /* Create and initialise the session */
    ses = session_new(sender_rtp_port, sender_rtcp_port, cname_sender, strlen(cname_sender), xrtp_catalog()); 
    if(!ses){

        port_done(sender_rtp_port);
        port_done(sender_rtcp_port);
        printf("Sending tester: Session fail to create!\n");
        return 1;
    }
    
    session_set_id(ses, 12345);
    
    session_allow_anonymous(ses, XRTP_YES);
    session_set_bandwidth(ses, 28800, 20000);

    mh = session_add_handler(ses, plugid);
    printf("Sending tester: plugin '%s' added.\n", plugid);

    med = session_new_media(ses, plugid, profile_no);
    printf("Sending tester: media handler for '%s' created.\n", plugid);
    /* Can add more handler for maybe compression, security or whatever handler */

    /* Set callbacks of the session
    session_callbacks_t callbacks[CALLBACK_SESSION_MAX] = {

       CALLBACK_SESSION_MEDIA_RECVD,        media_recvd,        NULL;
       CALLBACK_SESSION_MEDIA_SENT,         media_sent,         NULL;
       CALLBACK_SESSION_APPINFO_RECVD,      NULL,               NULL;
       CALLBACK_SESSION_MEMBER_APPLY,       cb_member_apply,    NULL;
       CALLBACK_SESSION_MEMBER_DELETED,     cb_member_deleted,  NULL;
       CALLBACK_SESSION_MEMBER_REPORT,      NULL,               NULL;
       CALLBACK_SESSION_BEFORE_RESET,       NULL,               NULL;
       CALLBACK_SESSION_AFTER_RESET,        NULL,               NULL;
       CALLBACK_SESSION_NEED_SIGNATURE,     NULL,               NULL;
       CALLBACK_SESSION_NOTIFY_DELAY,       NULL,               NULL;
       CALLBACK_SESSION_SYNC_TIMESTAMP,     NULL,               NULL;
       CALLBACK_SESSION_RESYNC_TIMESTAMP,   NULL,               NULL;
       CALLBACK_SESSION_RTP_TIMESTAMP,      NULL,               NULL;
       CALLBACK_SESSION_NTP_TIMESTAMP,      NULL,               NULL;
       CALLBACK_SESSION_MT2HRT,             mt2hrt,             NULL;
       CALLBACK_SESSION_HRT2MT,             hrt2mt,             NULL;
    };
    */
    session_set_callback(ses, CALLBACK_SESSION_MEDIA_SENT, cb_media_sent, &sender);
    session_set_callback(ses, CALLBACK_SESSION_MEMBER_UPDATE, cb_member_update, &sender);
    session_set_callback(ses, CALLBACK_SESSION_MEMBER_DELETED, cb_member_deleted, &sender);
    session_set_callback(ses, CALLBACK_SESSION_MT2HRT, cb_mt2hrt, &sender);
    session_set_callback(ses, CALLBACK_SESSION_HRT2MT, cb_hrt2mt, &sender);

    sender.session = ses;
    sender.subt = med;
    sender.spu = spu;
    sender.n_recvr = 0;
    sender.lock = xthr_new_lock();
    sender.wait = xthr_new_cond( XTHREAD_NONEFLAGS );
    sender.wait_receiver = xthr_new_cond( XTHREAD_NONEFLAGS );
    
    /* seekable */
    sender.clock = time_start();
	time_adjust(sender.clock, 600000,0,0);
    
    sender.seek_time = 600000;
    sender.need_seek = 1;

    printf("Sending tester: Sender session created\n");

    /* Member management test

    member_state_t * mem = NULL;
    if(session_new_member(ses, 34567, NULL) < XRTP_OK)
       printf("Session test: Can't create member 34567 state info!\n");

    if(session_new_member(ses, 56789, NULL) < XRTP_OK)
       printf("Session test: Can't create member 56789 state info!\n");

    mem = session_member_state(ses, 56789);
    if(!mem){

       printf("Session test: Bug of finding a exist member!\n");
       exit(1);
    }

    mem = session_member_state(ses, 87654);
    if(mem){

       printf("Session test: Bug of finding a none exist member!\n");
       exit(1);
    }

    printf("Session test: Member management test passed !\n");

    End of member management testing */

    session_set_scheduler(ses, xrtp_scheduler());
    printf("Sending tester: Scheduler added\n");

    /** recvr preparation **/
    recvr_rtp_port = port_new("127.0.0.1", 8000, RTP_PORT);
    recvr_rtcp_port = port_new("127.0.0.1", 8001, RTCP_PORT);
    printf("Receiving tester: port pair created!\n");

    /* Create and initialise the session */
    ses = session_new(recvr_rtp_port, recvr_rtcp_port, cname_recvr, strlen(cname_recvr), xrtp_catalog());
    if(!ses){

        port_done(recvr_rtp_port);
        port_done(recvr_rtcp_port);
        printf("Receiving tester: Session fail to create!\n");
        return 1;
    }

    session_set_id(ses, 54321);

    //session_allow_anonymous(ses, XRTP_YES);
    session_set_bandwidth(ses, 28800, 20000);

    mh = session_add_handler(ses, plugid);
    printf("Receiving tester: plugin '%s' added.\n", plugid);

    med = session_new_media(ses, plugid, profile_no);
    media_set_callback(med, CALLBACK_MEDIA_ARRIVED, cb_media_recvd, &recvr);
    printf("Receiving tester: media handler for '%s' created.\n", plugid);

    session_set_callback(ses, CALLBACK_SESSION_MEMBER_UPDATE, cb_member_update, &recvr);
    session_set_callback(ses, CALLBACK_SESSION_MEMBER_DELETED, cb_member_deleted, &recvr);
    session_set_callback(ses, CALLBACK_SESSION_MT2HRT, cb_mt2hrt, &recvr);
    session_set_callback(ses, CALLBACK_SESSION_HRT2MT, cb_hrt2mt, &recvr);

    recvr.session = ses;
    recvr.subt = med;
    recvr.tolerant_delay_ns = 1000000000;
    recvr.frames = xrtp_list_new();
    recvr.lock = xthr_new_lock();
    recvr.wait = xthr_new_cond( XTHREAD_NONEFLAGS );
    recvr.clock = time_start();
    printf("Receiving tester: Recvr session created\n");
    
    /* Put the session in schedule */
    session_set_scheduler(ses, xrtp_scheduler());
    printf("Receiving tester: Scheduler added\n");
    
    th_recvr = xthr_new(spu_recvr_run, NULL, XTHREAD_NONEFLAGS);
    th_sender = xthr_new(spu_sender_run, NULL, XTHREAD_NONEFLAGS);
        
    tp_rtp = teleport_new("127.0.0.1", 9000);
    tp_rtcp = teleport_new("127.0.0.1", 9001);
    
    xthr_lock(recvr.lock);
    session_join(recvr.session, tp_rtp, tp_rtcp);
    session_start_receipt(recvr.session);
    xthr_unlock(recvr.lock);
    printf("Session tester: Contacting the sender ...\n");

    xthr_wait(th_sender, &ret);
    xthr_wait(th_recvr, &ret);
    
    printf("Session tester: to free session\n");
    
    recvr.subt = NULL;
    session_done(recvr.session);
    xthr_done_cond(recvr.wait);
    xthr_done_lock(recvr.lock);
    xrtp_list_free(recvr.frames, spu_done_frame);

    sender.subt = NULL;
    session_done(sender.session);
    xthr_done_cond(sender.wait);
    xthr_done_lock(sender.lock);
    demux_sputext_done(sender.spu);

    /* Finalised the xrtp lib */
    xrtp_done();
        
    return 0;
 }
