/***************************************************************************
                          spu_sender.c  -  session tester
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

#include "t_session.h"
/*
#define SEND_LOG
*/
#ifdef SEND_LOG
 #define send_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define send_log(fmtargs)
#endif

#define SEND_IP "127.0.0.1"
#define SEND_RTP_PORT 3000 
#define SEND_RTCP_PORT 3001
 
#define SEND_CNAME "subtitle.srt"

/* subtitle sender */
struct subt_sender_s
{
   xrtp_session_t *  session;
   char cname[256];
   int cnlen;

   xrtp_media_t *    subt;
    
   demux_sputext_t * spu;
    
   int ms_seek;

   xrtp_clock_t * clock;

   xthr_lock_t * lock;
   xthr_cond_t * wait;
   
   int n_recvr;
   xthr_cond_t * wait_receiver;

   int end;
    
   xrtp_thread_t * thread;
    
   sipua_t *sipua;
};
 
/* Subtitle receiver */
typedef struct subt_frame_s
{
   rtime_t start;
   media_time_t dur;

   int len;
   char *data;
    
} subt_frame_t;
 
int cb_sender_media_sent(void *user, xrtp_media_t *media)
{
   send_log(("media_sent: one unit sent\n"));

   xthr_cond_signal(((subt_sender_t *)user)->wait);

   return XRTP_OK;
}

/* member mamgement by rtp */
int cb_sender_member_apply(void* user, uint32 member_src, char * cname, int cnlen, void **user_info)
{
    cname[cnlen] = '\0';

	send_log(("\nsender.cb_member_apply: reject '%s'\n", cname));
	return XRTP_EREFUSE;
}

int cb_sender_member_report(void* user, void* member_info, 
					 uint32 ssrc, char *cname, int clen,
					 uint npack_recv, uint bytes_recv,
					 uint8 frac_lost, uint32 intv_lost, 
					 uint32 jitter, uint32 rtt)
{
    subt_sender_t *sender = (subt_sender_t*)user;
	
	cname[clen] = '\0';
    
	printf("\n=RTCP[%s]================================\n", cname);
	printf("cb_sender_member_report: '%s' report\n", cname);
	printf("ssrc[%u],sent (packets[%d],bytes[%d])\n", ssrc, npack_recv, bytes_recv);
	printf("frac lost[%f]", frac_lost);
	printf(" interval lost[%u]p\n", intv_lost);
	printf("jitter[%u],rrt[%d]\n", jitter, rtt);
	printf("-------------------------------------------------\n\n");

	sender->n_recvr++;
	xthr_cond_signal(sender->wait_receiver);
    
    return XRTP_OK;
}
 
int cb_sender_member_deleted(void* user, uint32 member_src, void * user_info)
{
    ((subt_sender_t*)user)->n_recvr--;
    
    return XRTP_OK;
}

int spu_sender_run(void * param)
{
    subtitle_t * subt;
    char str[1024];
    int slen = 0;

	subt_sender_t *send = (subt_sender_t*)param;

    rtime_t ms_now, ms_begin;
    
    int line, tryagain = 0, first = 1;

    send_log(("\nspu_sender_run: sender started ...\n"));
    
	send->clock = time_start();    
    
    while(1)
	{
	   int ret;
	   rtime_t dms = 0;
	   uint32 timestamp;

       xthr_lock(send->lock);
       
       if(send->n_recvr == 0)
	   {
          send_log(("spu_sender_run: waiting for receiver ...\n"));
          xthr_cond_wait(send->wait_receiver, send->lock);
          send_log(("spu_sender_run: receiver available\n"));
       }
       
       if(!tryagain)
	   {
         if(send->ms_seek > 0)
		 {
            demux_sputext_seek_msec(send->spu, send->ms_seek);
			time_adjust(send->clock, send->ms_seek,0,0);
            send->ms_seek = 0;
         }
         
         subt = demux_sputext_next_subtitle(send->spu);
         if(!subt)
		 {
            send_log(("spu_sender_run: No more subtitle, quit\n"));
            break;
         }
         
         //start = (sender.spu->uses_time) ? subt->start * 10 : subt->start;
         //end = (sender.spu->uses_time) ? subt->end * 10 : subt->end;

         slen = 0;
         for (line = 0; line < subt->lines; line++)
		 {
            if(slen+strlen(subt->text[line])+1 >= 1024)
                break;
                
            strcpy(str+slen, subt->text[line]);
            slen += strlen(subt->text[line]);
         }
       }
	   else
	   {
          xthr_cond_wait(send->wait, send->lock);
       }

       if(first)
	   {
          first = 0;
          ms_now = ms_begin = time_msec_now(send->clock);
          send_log(("\nspu_sender_run: Begin at %dms\n", ms_begin));
       }
	   else
	   {
          ms_now = time_msec_now(send->clock);
          send_log(("\nspu_sender_run: %dms now\n", ms_now));
       }

       dms = subt->start_ms - ms_now;
       if(dms > 0)
	   {
          send_log(("spu_sender_run: sleep %dms\n\n", dms));
          
          xthr_unlock(send->lock);
          time_msec_sleep(send->clock, dms, NULL);
       }
	   else
	   {
          xthr_unlock(send->lock);
       }
       
       /* This is for debug perposal only
       demux_sputext_show_title(subt);
       */
       
       /* Sending */
	   timestamp = time_msec_now(session_clock(send->session));
       ret = send->subt->post(send->subt, str, slen, timestamp);
       if(ret == XRTP_EAGAIN)
	   {
		   tryagain = 1;
           continue;
       }

       tryagain = 0;         
    }

    xthr_unlock(send->lock);
    
    send_log(("\nspu_sender_run: end thread ...\n"));

    return XRTP_OK;
}

int sender_setup(subt_sender_t *send, int8 pt, char *type)
{
    char ip[] = SEND_IP;
	uint16 rtp_port = SEND_RTP_PORT;
	uint16 rtcp_port = SEND_RTCP_PORT;

	char cname[] = SEND_CNAME;
	int cnlen = strlen(SEND_CNAME);

	/* Sender Var */
    FILE *f;
    char *title = cname; /*"subtitle.srt";*/                         
    demux_sputext_t * spu = NULL;
    xrtp_session_t *ses = NULL; 

    profile_handler_t * mh = NULL;
    xrtp_media_t * med = NULL;

    xrtp_port_t * sender_rtp_port = NULL;
    xrtp_port_t * sender_rtcp_port = NULL;

	send_log(("\nsender_setup: FD_SETSIZE = %u\n", FD_SETSIZE));
    send_log(("sender_setup: [%s];pt[%d];cn[%s]\n", type,pt,cname));
    send_log(("sender_setup: rtp[%s:%u]\n", ip,rtp_port));
    send_log(("sender_setup: rtcp[%s:%u]\n\n", ip,rtcp_port));

    f = fopen(title, "r");

    spu = demux_sputext_new(f);
    send_log(("sender_setup: Subtitle '%s' specialised\n", title));

    //fclose(f);
    
    /* Create rtp port */
    sender_rtp_port = port_new(ip, rtp_port, RTP_PORT);
    sender_rtcp_port = port_new(ip, rtcp_port, RTCP_PORT);

	if(!sender_rtp_port || !sender_rtcp_port)
	{
		send_log(("sender_setup: fail to create ports, exit\n"));
		return -1;
	}

    send_log(("Sending tester: rtp_port@%d!\n", (int)sender_rtp_port));

    /* Create and initialise the session */
    ses = session_new(sender_rtp_port, sender_rtcp_port, cname, cnlen, xrtp_catalog()); 
    if(!ses){

        port_done(sender_rtp_port);
        port_done(sender_rtcp_port);
        send_log(("sender_setup: Session fail to create!\n"));
        return 1;
    }
    
    session_set_id(ses, 12345);
    
    session_allow_anonymous(ses, XRTP_YES);
    session_set_bandwidth(ses, 28800, 20000);

    mh = session_add_handler(ses, type);
    send_log(("sender_setup: '%s' plugin added.\n", type));

    med = session_new_media(ses, type, pt);
    send_log(("sender_setup: media handler for '%s' created.\n", type));

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

    session_set_callback(ses, CALLBACK_SESSION_MEDIA_SENT, cb_sender_media_sent, send);

    /* detect parties coming after session start */
    session_set_callback(ses, CALLBACK_SESSION_MEMBER_APPLY, cb_sender_member_apply, send);
    session_set_callback(ses, CALLBACK_SESSION_MEMBER_REPORT, cb_sender_member_report, send);
    session_set_callback(ses, CALLBACK_SESSION_MEMBER_DELETED, cb_sender_member_deleted, send);

    send->session = ses;
    send->subt = med;
    send->spu = spu;
    send->n_recvr = 0;
    send->lock = xthr_new_lock();
    send->wait = xthr_new_cond( XTHREAD_NONEFLAGS );
    send->wait_receiver = xthr_new_cond( XTHREAD_NONEFLAGS );

    /* seekable */
    send->ms_seek = 600000;

    send_log(("sender_setup: Sender session created\n"));

    session_set_scheduler(ses, xrtp_scheduler());
    send_log(("sender_setup: Scheduler added\n"));

    send->thread = xthr_new(spu_sender_run, send, XTHREAD_NONEFLAGS);

    send_log(("sender_setup: ready\n"));

	return XRTP_OK;
}

int sender_shutdown(subt_sender_t *send)
{
    send->subt = NULL;

    session_done(send->session);
    xthr_done_cond(send->wait);
    xthr_done_lock(send->lock);
    demux_sputext_done(send->spu);

	free(send);

    send_log(("sender_shutdown: sender down\n"));

	return XRTP_OK;
}

int cb_sender_oncall(void *user, char *from_cn, int from_cnlen, capable_t *caps[], int ncap)
{
	subt_sender_t *send = (subt_sender_t*)user;

	if(ncap == 1)
	{
		rtp_capable_t *rtpcap = (rtp_capable_t*)caps[0];
		xrtp_teleport_t *rtp_tport = teleport_new(rtpcap->ip, rtpcap->rtp_port);
		xrtp_teleport_t *rtcp_tport = teleport_new(rtpcap->ip, rtpcap->rtcp_port);
		send_log(("cb_sender_callin: call from cn[%s]@[%s:%u/%u]\n", from_cn, rtpcap->ip, rtpcap->rtp_port, rtpcap->rtcp_port));

		sender_setup(send, rtpcap->profile_no, rtpcap->profile_type);

		session_add_cname(send->session, from_cn, from_cnlen, rtp_tport, rtcp_tport, NULL);
	
		return UA_OK;
	}

	return UA_REJECT;
}

/* change capables, if ncap == 0, hangup completely */
int cb_sender_onreset(void *user, char *from_cn, int from_cnlen, capable_t *caps[], int ncap)
{
	subt_sender_t *send = (subt_sender_t*)user;

	send_log(("cb_sender_recall: recall from cn[%s]\n", from_cn));

	if(ncap == 0)
	{
		sender_shutdown(send);
	}

	return UA_OK;
}

int cb_sender_onbye(void *user, char *cname, int cnlen)
{
	subt_sender_t *send = (subt_sender_t*)user;

	if(session_delete_cname(send->session, cname, cnlen) == 1);
		sender_shutdown(send);

	return UA_OK;
}

subt_sender_t* sender_new(sipua_t *sipua)
{
	sipua_record_t *rec;
	capable_t *rtpcap;

	subt_sender_t *send = malloc(sizeof(subt_sender_t));
	memset(send, 0, sizeof(subt_sender_t));

	send->sipua = sipua;
	strcpy(send->cname, SEND_CNAME);
	send->cnlen = strlen(SEND_CNAME);

	rec = sipua_new_record(SEND_CNAME, strlen(SEND_CNAME), 
							send, cb_sender_oncall, 
							send, cb_sender_onreset,
							send, cb_sender_onbye);

	rtpcap = rtp_new_capable(PT, PROFILE_TYPE, SEND_IP, SEND_RTP_PORT, SEND_RTCP_PORT);
	rec->enable(rec, rtpcap);

	sipua->regist(sipua, SEND_CNAME, strlen(SEND_CNAME), rec);

	send_log(("sender_new: sip ready\n"));
	return send;
}
