/***************************************************************************
                          spu_recvr.c  -  session tester
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
#define RECV_LOG
*/
#ifdef RECV_LOG
 #define recv_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define recv_log(fmtargs)
#endif

#define RECV_IP "127.0.0.1"
#define RECV_RTP_PORT 4000 
#define RECV_RTCP_PORT 4001

#define RECV_CNAME "spu_recvr" 

/* Subtitle receiver */
typedef struct subt_frame_s
{
   rtime_t start;
   media_time_t dur;

   int len;
   char *data;
    
} subt_frame_t;
 
struct subt_recvr_s
{
   xrtp_session_t *  session;
   char cname[256];
   int cnlen;

   xrtp_media_t *    subt;

   media_time_t next_ts;

   xrtp_list_t * frames;
   
   xthr_lock_t * lock;
   xthr_cond_t * wait;

   int end;

   xrtp_thread_t * thread;

   sipua_t *sipua;
   sipua_record_t *sip_rec;
   capable_t *sip_cap;
};

void subt_show(subt_frame_t * frm)
{
   char str[SUB_MAXLEN];

   memcpy(str, frm->data, frm->len);
   str[frm->len] = '\0';
    
   printf("\n\nRTP['%s']\n\n", str);

   return;
}

int spu_done_frame(void * gen)
{
    free(((subt_frame_t*)gen)->data);
    free(gen);

    return XRTP_OK;
}
 
int frame_cmp_mts(void * targ, void * patt)
{
    subt_frame_t *f_targ = (subt_frame_t *)targ;
    subt_frame_t *f_patt = (subt_frame_t *)patt;

    return f_targ->start - f_patt->start;
}
 
int cb_media_recvd(void* u, char *data, int dlen, uint32 ssrc, uint32 ts)
{
    subt_recvr_t * r = (subt_recvr_t *)u;
    subt_frame_t * frm = NULL;

    recv_log(("test.cb_media_recvd: Media[#%u] ready\n", ts));

    frm = malloc(sizeof(struct subt_frame_s));
    if(!frm)
    {
        xthr_unlock(r->lock);
        recv_log(("cb_media_recvd: Fail to allocated frame memery\n"));
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

int cb_recvr_member_apply(void* user, uint32 member_src, char * cname, int cnlen, void **user_info)
{
    cname[cnlen] = '\0';

	printf("\nrecvr.cb_member_apply: accept '%s'\n", cname);
	return XRTP_OK;
}

int cb_recvr_member_report(void* user, void* member_info, 
					 uint32 ssrc, char *cname, int clen,
					 uint npack_recv, uint bytes_recv,
					 uint8 frac_lost, uint32 intv_lost, 
					 uint32 jitter, uint32 rtt)
{
    cname[clen] = '\0';
    
	printf("\n=RTCP[%s]=================================\n", cname);
	printf("cb_recvr_member_report: '%s' report\n", cname);
	printf("ssrc[%u],sent (packets[%d],bytes[%d])\n", ssrc, npack_recv, bytes_recv);
	printf("lost packet(frac[%f],interval[%up])\n", (double)frac_lost, intv_lost);
	printf("jitter[%u],rrt[%d]\n", jitter, rtt);
	printf("-------------------------------------------------\n\n");
    
    return XRTP_OK;
}
 
int cb_recvr_member_deleted(void* user, uint32 member_src, void * user_info)
{
    return XRTP_OK;
}

int spu_recvr_run(void * param)
{
    subt_frame_t *f;
    
    xrtp_hrtime_t now, dt;
    
    struct xrtp_list_user_s lu;

	subt_recvr_t *recvr = (subt_recvr_t*)param;

    xclock_t *sclock;
    
    recv_log(("\nspu_recvr_run: start thread ...\n"));

    sclock = session_clock(recvr->session);
    
    while(1){

       xthr_lock(recvr->lock);
       
       f = (subt_frame_t *)xrtp_list_first(recvr->frames, &lu);
       
       if(!f && recvr->end){
         
          xthr_unlock(recvr->lock);
          break;
       }

       if(!f && !recvr->end){
         
          recv_log(("spu_recvr_run: wait next...\n"));
          xthr_cond_wait(recvr->wait, recvr->lock);

          f = (subt_frame_t *)xrtp_list_first(recvr->frames, &lu);
          recv_log(("spu_recvr_run: play Media[#%d]\n", f->start));
       }

       now = time_msec_now(sclock);
       
       dt = f->start - now;
       recv_log(("spu_recvr_run: %dms now, wait %dms\n", now, dt));

       if(dt > 0)
       {
          /* snap sometime */
          xthr_unlock(recvr->lock);
          time_msec_sleep(sclock, dt, NULL);
          continue;
       }

       f = (subt_frame_t *)xrtp_list_remove_first(recvr->frames);
       xthr_unlock(recvr->lock);
       
       subt_show(f);
       spu_done_frame(f);
    }

    recv_log(("\nspu_recvr_run: end thread ...\n"));
    
    return XRTP_OK;
}

void test_xthr()
{
    xthr_lock_t * lock = xthr_new_lock();
    printf("test_xthr: lock ok\n");
    xthr_done_lock(lock);
}
 
int recvr_setup(subt_recvr_t *recvr, int8 pt, char *type)
{
    char ip[] = RECV_IP;
	uint16 rtp_portno = RECV_RTP_PORT;
	uint16 rtcp_portno = RECV_RTCP_PORT;

	char cname[] = RECV_CNAME;
	int cnlen = strlen(RECV_CNAME);

	/* Recvr Var */
    xrtp_session_t *ses = NULL;
	char *sender_cn = "spu_sender";
	
    profile_handler_t * mh = NULL;
    xrtp_media_t * med = NULL;

    recv_log(("\nrecvr_setup: [%s];pt[%d];cn[%s]\n", type,pt,cname));
    recv_log(("recvr_setup: rtp[%s:%u]\n", ip,rtp_port));
    recv_log(("recvr_setup: rtcp[%s:%u]\n\n", ip,rtcp_port));

    /** recvr preparation **/
    recv_log(("recvr_setup: port pair created!\n"));

    /* Create and initialise the session */
    ses = session_new(cname, strlen(cname), ip, rtp_portno, rtcp_portno, xrtp_catalog(), NULL);
    if(!ses)
	{
        recv_log(("recvr_setup: Session fail to create!\n"));
        return 1;
    }

    session_set_id(ses, 54321);

    //session_allow_anonymous(ses, XRTP_YES);
    session_set_bandwidth(ses, 28800, 20000);

    mh = session_add_handler(ses, type);
    recv_log(("recvr_setup: '%s' plugin added.\n", type));

    med = session_new_media(ses, type, pt);
    media_set_callback(med, CALLBACK_MEDIA_ARRIVED, cb_media_recvd, recvr);
    recv_log(("recvr_setupr: media handler for '%s' created.\n", type));

    /* detect parties coming after session start */
	session_set_callback(ses, CALLBACK_SESSION_MEMBER_APPLY, cb_recvr_member_apply, recvr);
    session_set_callback(ses, CALLBACK_SESSION_MEMBER_REPORT, cb_recvr_member_report, recvr);
    session_set_callback(ses, CALLBACK_SESSION_MEMBER_DELETED, cb_recvr_member_deleted, recvr);

    recvr->session = ses;
    recvr->subt = med;
    recvr->frames = xlist_new();
    recvr->lock = xthr_new_lock();
    recvr->wait = xthr_new_cond( XTHREAD_NONEFLAGS );
    recv_log(("recvr_setup: Recvr session created\n"));
    
    /* Put the session in schedule */
    session_set_scheduler(ses, xrtp_scheduler());
    recv_log(("recvr_setup: Scheduler added\n"));
    
    recvr->thread = xthr_new(spu_recvr_run, recvr, XTHREAD_NONEFLAGS);

    recv_log(("recvr_setup: ready\n"));

    return XRTP_OK;
}
 
int recvr_shutdown(subt_recvr_t *recvr)
{
    recvr->subt = NULL;

    session_done(recvr->session);
    xthr_done_cond(recvr->wait);
    xthr_done_lock(recvr->lock);
    xrtp_list_free(recvr->frames, spu_done_frame);

    recv_log(("recvr_shutdown: recvr down\n"));

	return XRTP_OK;
}

int recvr_start(subt_recvr_t *recvr, char *to_cn, int to_cnlen)
{
	return recvr->sipua->connect(recvr->sipua, recvr->cname, recvr->cnlen, to_cn, to_cnlen);
}

int recvr_end(subt_recvr_t *recvr, char *to_cn, int to_cnlen)
{
	return recvr->sipua->disconnect(recvr->sipua, recvr->cname, recvr->cnlen, to_cn, to_cnlen);
}

int cb_recvr_oncall(void *user, char *from_cn, int from_cnlen, capable_t *caps[], int ncap)
{
	subt_recvr_t *recv = (subt_recvr_t*)user;

	if(ncap == 1)
	{
		rtp_capable_t *rtpcap = (rtp_capable_t*)caps[0];

		recv_log(("cb_recvr_callok: cn[%s]@[%s:%u/%u] ok\n", from_cn, rtpcap->ip, rtpcap->rtp_port, rtpcap->rtcp_port));

		recvr_setup(recv, rtpcap->profile_no, rtpcap->profile_type);
		
		session_add_cname(recv->session, from_cn, from_cnlen, rtpcap->ip, rtpcap->rtp_port, rtpcap->rtcp_port, NULL);

		session_start_receipt(recv->session);
		return UA_OK;
	}

	return UA_REJECT;
}

/* change capables, if ncap == 0, hangup completely */
int cb_recvr_onreset(void *user, char *cname, int cnlen, capable_t *caps[], int ncap)
{
	subt_recvr_t *recv = (subt_recvr_t*)user;

	if(ncap == 0)
	{
		recvr_shutdown(recv);
	}

	return UA_OK;
}

int cb_recvr_onbye(void *user, char *cname, int cnlen)
{
	subt_recvr_t *recv = (subt_recvr_t*)user;

	recvr_shutdown(recv);

	return UA_OK;
}

subt_recvr_t* recvr_new(sipua_t *sipua)
{
	sipua_record_t *rec;
	capable_t *rtpcap;

	subt_recvr_t *recvr = malloc(sizeof(subt_recvr_t));
	memset(recvr, 0, sizeof(subt_recvr_t));

	recvr->sipua = sipua;
	strcpy(recvr->cname, RECV_CNAME);
	recvr->cnlen = strlen(RECV_CNAME);

	rec = sipua_new_record(recvr, cb_recvr_oncall, 
							recvr, cb_recvr_onreset,
							recvr, cb_recvr_onbye);

	rtpcap = rtp_new_capable(PT, PROFILE_TYPE, RECV_IP, RECV_RTP_PORT, RECV_RTCP_PORT);
	rec->enable(rec, rtpcap);

	sipua->regist(sipua, RECV_CNAME, strlen(RECV_CNAME), rec);

	recvr->sip_rec = rec;
	recvr->sip_cap = rtpcap;

	recv_log(("recvr_new: sip ready\n"));

	return recvr;
}

