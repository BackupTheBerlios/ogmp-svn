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
/*
#define TEST_LOG
*/
#ifdef TEST_LOG
 #define test_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define test_log(fmtargs)
#endif

typedef struct subt_frame_s
{
   rtime_t start;
   media_time_t dur;

   int len;
   char *data;
    
} subt_frame_t;
 
typedef struct subt_sender_s subt_sender_t;

struct subt_sender_s
{
   xrtp_session_t *  session;
    
   xrtp_media_t *    subt;
    
   demux_sputext_t * spu;
    
   int ms_seek;

   xrtp_clock_t * clock;

   xthr_lock_t * lock;
   xthr_cond_t * wait;
   
   int n_recvr;
   xthr_cond_t * wait_receiver;

   int end;
    
} sender;
 
typedef struct subt_recvr_s subt_recvr_t;

struct subt_recvr_s
{
   xrtp_session_t *  session;
   xrtp_media_t *    subt;

   media_time_t next_ts;

   xrtp_list_t * frames;
   //xrtp_clock_t * clock;
   
   xthr_lock_t * lock;
   xthr_cond_t * wait;

   int end;

   int tolerant_delay_ns;

} recvr;

void subt_show(subt_frame_t * frm)
{
   char str[SUB_MAXLEN];

   memcpy(str, frm->data, frm->len);
   str[frm->len] = '\0';
    
   printf("\n\n['%s']\n\n", str);

   return;
}

int cb_media_sent(void * u, xrtp_media_t * media)
{
   subt_sender_t * s = (subt_sender_t *)u;
   printf("media_sent: one unit sent\n");
   xthr_cond_signal(s->wait);

   return XRTP_OK;
}

int spu_sender_run(void * param)
{
    subtitle_t * subt;
    char str[1024];
    int slen = 0;

    rtime_t ms_now, ms_begin;
    
    int line, tryagain = 0, first = 1;

    printf("\nspu_sender_run: sender started ...\n");
    
	sender.clock = time_start();    
    
    while(1)
	{
	   int ret;
	   rtime_t dms = 0;
	   uint32 timestamp;

       xthr_lock(sender.lock);
       
       if(sender.n_recvr == 0){

          test_log(("spu_sender_run: waiting for receiver ...\n"));
          xthr_cond_wait(sender.wait_receiver, sender.lock);
          test_log(("spu_sender_run: receiver available\n"));
       }
       
       if(!tryagain){

         if(sender.ms_seek > 0){

            demux_sputext_seek_msec(sender.spu, sender.ms_seek);
			time_adjust(sender.clock, sender.ms_seek,0,0);
            sender.ms_seek = 0;
         }
         
         subt = demux_sputext_next_subtitle(sender.spu);
         if(!subt){
           
            printf("spu_sender_run: No more subtitle, quit\n");
            break;
         }
         
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
          ms_now = ms_begin = time_msec_now(sender.clock);
          test_log(("\nspu_sender_run: Begin at %dms\n", ms_begin));

       }else{

          ms_now = time_msec_now(sender.clock);
          test_log(("\nspu_sender_run: %dms now\n", ms_now));
       }

       dms = subt->start_ms - ms_now;
       if(dms > 0){

          test_log(("spu_sender_run: sleep %dms\n\n", dms));
          
          xthr_unlock(sender.lock);
          time_msec_sleep(sender.clock, dms, NULL);

       }else{

          xthr_unlock(sender.lock);
       }
       
       /* This is for debug perposal only
       demux_sputext_show_title(subt);
       */
       
       /* Send really */
	   timestamp = time_msec_now(session_clock(sender.session));
       ret = sender.subt->post(sender.subt, str, slen, timestamp);
       if(ret == XRTP_EAGAIN)
	   {
		   tryagain = 1;
           continue;
       }

       tryagain = 0;         
    }

    xthr_unlock(sender.lock);
    
    test_log(("\nspu_sender_run: end thread ...\n"));

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
 
 int cb_media_recvd(void* u, char *data, int dlen, uint32 ssrc, uint32 ts){

    subt_recvr_t * r = (subt_recvr_t *)u;
    subt_frame_t * frm = NULL;

    test_log(("test.cb_media_recvd: Media[#%u] ready\n", ts));

    frm = malloc(sizeof(struct subt_frame_s));
    if(!frm)
    {
        xthr_unlock(r->lock);
        test_log(("cb_media_recvd: Fail to allocated frame memery\n"));
        return XRTP_EMEM;
    }
      
    frm->start = (rtime_t)ts;
    frm->len = dlen;
    frm->data = data;
    
    xthr_lock(r->lock);
    xrtp_list_add_ascent_once(r->frames, frm, frame_cmp_mts);    
    xthr_unlock(r->lock);

    xthr_cond_signal(r->wait);

    return XRTP_OK;
 }

 int cb_member_update(void* user, uint32 member_src, char * cname, int clen){

    cname[clen] = '\0';
    
    if(user == &sender){
      
        printf("\ntester.cb_member_update: '%s' accepted\n", cname);
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
    
    xrtp_hrtime_t now, dt;
    
    struct xrtp_list_user_s lu;

    xclock_t *sclock = session_clock(recvr.session);
    
    test_log(("\nspu_recvr_run: start thread ...\n"));
    
    while(1){

       xthr_lock(recvr.lock);
       
       f = (subt_frame_t *)xrtp_list_first(recvr.frames, &lu);
       
       if(!f && recvr.end){
         
          xthr_unlock(recvr.lock);
          break;
       }

       if(!f && !recvr.end){
         
          test_log(("spu_recvr_run: wait next...\n"));

          xthr_cond_wait(recvr.wait, recvr.lock);

          f = (subt_frame_t *)xrtp_list_first(recvr.frames, &lu);
          test_log(("spu_recvr_run: play Media[#%d]\n", f->start));
       }

       now = time_msec_now(sclock);
       
       dt = f->start - now;
       test_log(("spu_recvr_run: %dms now, wait %dms\n", now, dt));

       if(dt > 0)
       {
          /* snap sometime */
          xthr_unlock(recvr.lock);
          time_msec_sleep(sclock, dt, NULL);
          continue;
       }

       f = (subt_frame_t *)xrtp_list_remove_first(recvr.frames);
       xthr_unlock(recvr.lock);
       
       subt_show(f);
       spu_done_frame(f);
    }

    test_log(("\nspu_recvr_run: end thread ...\n"));
    
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
    printf("Session tester: Subtitle '%s' specialised\n", title);

    //fclose(f);
    
    /* Realised the xrtp lib */
    if(xrtp_init() < XRTP_OK)
        return -1;

    /* Create rtp port */
    sender_rtp_port = port_new("127.0.0.1", 3000, RTP_PORT);
    sender_rtcp_port = port_new("127.0.0.1", 3001, RTCP_PORT);
	if(!sender_rtp_port || !sender_rtcp_port){
    
		printf("Sending tester: fail to create ports, exit\n");
		return -1;
	}

    printf("Sending tester: rtp_port@%d!\n", (int)sender_rtp_port);

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

    sender.session = ses;
    sender.subt = med;
    sender.spu = spu;
    sender.n_recvr = 0;
    sender.lock = xthr_new_lock();
    sender.wait = xthr_new_cond( XTHREAD_NONEFLAGS );
    sender.wait_receiver = xthr_new_cond( XTHREAD_NONEFLAGS );
    
    /* seekable */
    sender.ms_seek = 600000;

    printf("Sending tester: Sender session created\n");

    session_set_scheduler(ses, xrtp_scheduler());
    printf("Sending tester: Scheduler added\n");

    /** recvr preparation **/
    recvr_rtp_port = port_new("127.0.0.1", 4000, RTP_PORT);
    recvr_rtcp_port = port_new("127.0.0.1", 4001, RTCP_PORT);

    printf("Receiving tester: port pair created!\n");

    /* Create and initialise the session */
    ses = session_new(recvr_rtp_port, recvr_rtcp_port, cname_recvr, strlen(cname_recvr), xrtp_catalog());
    if(!ses)
	{
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

    recvr.session = ses;
    recvr.subt = med;
    recvr.tolerant_delay_ns = 1000000000;
    recvr.frames = xrtp_list_new();
    recvr.lock = xthr_new_lock();
    recvr.wait = xthr_new_cond( XTHREAD_NONEFLAGS );
    //session clock instead: recvr.clock = time_start();
    printf("Receiving tester: Recvr session created\n");
    
    /* Put the session in schedule */
    session_set_scheduler(ses, xrtp_scheduler());
    printf("Receiving tester: Scheduler added\n");
    
    th_recvr = xthr_new(spu_recvr_run, NULL, XTHREAD_NONEFLAGS);
    th_sender = xthr_new(spu_sender_run, NULL, XTHREAD_NONEFLAGS);
        
    tp_rtp = teleport_new("127.0.0.1", 3000);
    tp_rtcp = teleport_new("127.0.0.1", 3001);
    
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
