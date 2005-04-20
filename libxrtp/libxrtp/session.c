/***************************************************************************
                          session.c  -  description
                             -------------------
    begin                : Wed Apr 30 2003
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
 
#include <string.h>
#include "internal.h"      
#include <timedia/md5.h>
#include <timedia/xmalloc.h>
 
#define MAX_PRODUCE_TIME  (20 * HRTIME_MILLI)
#define SESSION_DEFAULT_RTCP_NPACKET  0
#define USEC_POLLING_TIMEOUT	500000

/* issue 5 times media inf APP */
#define RENEW_MINFO_LEVEL 5
#define BUDGET_PERIOD_MSEC	1000  /* reset budget when large than one second */
/*
#define SESSION_LOG
#define SESSION_DEBUG
*/
#ifdef SESSION_LOG
 #define session_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define session_log(fmtargs)
#endif
    
#ifdef SESSION_DEBUG
 #define session_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define session_debug(fmtargs)
#endif

/* for sorting media packet */
typedef struct media_hold_s
{
	void* media;
	int bytes;
	int full_payload;   /* the payload of the timestamp is in full pieve */

	uint16 seqno;
	uint32 rtpts_play;
	rtime_t usec;
	rtime_t usec_ref;

	void *memory;  /* memory to free after last packet delivered in one payload */

} media_hold_t;
    
#define RTP_SEQ_MOD 65536
int session_init_seqno(member_state_t * mem, uint16 seq)
{
     mem->base_seq = seq;
     mem->max_seq = seq - 1;
     mem->bad_seq = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
     mem->cycles = 0;

	 mem->n_rtp_received = 0;
     mem->n_rtp_received_prior = 0;
     mem->n_rtp_expected_prior = 0;

     return XRTP_OK;
}

/**
 * Update seqno according the seq provided.
 *
 * return value: 1 if the seqno is a valid no, otherwise return 0.
 */
int session_update_seqno(member_state_t * mem, uint16 seq)
{
	 uint16 udelta = seq - mem->max_seq;
     
     const int MAX_DROPOUT = 3000;
     const int MAX_MISORDER = 100;

       /*
        * Source is not valid until MIN_SEQUENTIAL packets with
        * sequential sequence numbers have been received.
        */
       if (mem->probation) 
	   {
		   /* packet is in sequence */
           if (seq == mem->max_seq + 1) 
		   {
               mem->probation--;
               mem->max_seq = seq;

               if (mem->probation == 0) 
			   {
                   /* a valid seqno */
                   session_init_seqno(mem, seq);
                   mem->n_rtp_received++;

				   xlist_addto_first(mem->session->senders, mem);
				   mem->session->n_sender = xlist_size(mem->session->senders);

				   session_log(("session_update_seqno: %d senders now\n", mem->session->n_sender));
                   
				   return 1;
               }
           
			   session_log(("session_update_seqno: Sender probation[%d]\n",mem->probation));		   
           } 
		   else 
		   {
               /* packet is NOT in sequence, reset trying */
               mem->probation = MIN_SEQUENTIAL - 1;
               mem->max_seq = seq;
           }
           
           return 0;
       } 
	   else if (udelta < MAX_DROPOUT) 
	   {
           /* in order, with permissible gap */
           if (seq < mem->max_seq)
		   {
               /*
                * Sequence number wrapped - count another 64K cycle.
                */
               mem->cycles++;
           }
           
           mem->max_seq = seq;
       } 
	   else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) 
	   {
           /* the sequence number made a very large jump */
           if (seq == mem->bad_seq) 
		   {
               /*
                * Two sequential packets -- assume that the other side
                * restarted without telling us so just re-sync
                * (i.e., pretend this was the first packet).
                */
               session_init_seqno(mem, seq);
           }
		   else 
		   {
               mem->bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
               return 0;
           }
       } 
	   else 
	   {
           /* duplicate or reordered packet */
		   return 0;
       }
       	
       mem->n_rtp_received++;
       
       return 1;
 }

int session_done_media(void* gen)
{
    media_hold_t *hold = (media_hold_t*)gen;

	xfree(hold->media);
	xfree(hold);

    return XRTP_OK;
}

int session_done_active_member(void *gen)
{
    member_state_t *mem = (member_state_t*)gen;
	int ret_th;

	mem->stop_delivery = 1;
	xthr_wait(mem->deliver_thread, &ret_th);
	
	xlist_done(mem->delivery_buffer, session_done_media);

	xthr_done_cond(mem->wait_media_available);
	xthr_done_cond(mem->wait_seqno);
	xthr_done_lock(mem->delivery_lock);
	xthr_done_lock(mem->sync_lock);
    xthr_done_lock(mem->lock);    
    
    if(mem->cname)
		xfree(mem->cname);
    
    xfree(mem);

    return XRTP_OK;
}

int session_done_member(void *gen)
{
    member_state_t * mem = (member_state_t*)gen;
    
    if(mem->deliver_thread)
	{
		/* done in seperate thread */
		xthr_new(session_done_active_member, mem, XTHREAD_NONEFLAGS);
		return XRTP_OK;
	}
	
	xlist_done(mem->delivery_buffer, session_done_media);

	xthr_done_cond(mem->wait_media_available);
	xthr_done_cond(mem->wait_seqno);
	xthr_done_lock(mem->delivery_lock);
	xthr_done_lock(mem->sync_lock);
    xthr_done_lock(mem->lock);
    
    if(mem->cname)
		xfree(mem->cname);

	if(mem->rtp_port)
		teleport_done(mem->rtp_port);
	if(mem->rtcp_port)
		teleport_done(mem->rtcp_port);
    
    xfree(mem);

    return XRTP_OK;
}

/**
 * Release the Session
 */
int session_done(xrtp_session_t* ses)
{
	if(ses->set)
		xrtp_remove_session(ses->set, ses);

	pipe_done(ses->rtp_send_pipe);
    pipe_done(ses->rtcp_send_pipe);
    pipe_done(ses->rtp_recv_pipe);
    pipe_done(ses->rtcp_recv_pipe);

    queue_done(ses->packets_in_sched);

	xthr_done_lock(ses->renew_level_lock);
	xthr_done_lock(ses->members_lock);
	xthr_done_lock(ses->senders_lock);
	xthr_done_lock(ses->bandwidth_lock);
	xthr_done_lock(ses->rtp_incoming_lock);

   xlist_done(ses->senders, NULL);
   xlist_done(ses->members, session_done_member);
	
	xstr_done_string(ses->$state.profile_type);

    xfree(ses);

    return XRTP_OK;
}

 /**
  * Set mode of the Session, which could be
  *
  * 'SESSION_SEND', 'SESSION_RECV' or 'SESSION_DUPLEX' etc.
  */
int session_set_mode(xrtp_session_t * ses, int mode)
{
    switch(mode)
	{
       case(SESSION_SEND):

          ses->mode_trans = mode;
          if(!ses->rtp_send_pipe)
		  {
             ses->rtp_send_pipe = pipe_new(ses, XRTP_RTP, XRTP_SEND);
			if(!ses->rtp_send_pipe)
				goto fail;
             ses->rtcp_send_pipe = pipe_new(ses, XRTP_RTCP, XRTP_SEND);
			if(!ses->rtcp_send_pipe)
				goto fail;
          }
          break;
          
       case(SESSION_RECV):



          ses->mode_trans = mode;
          if(!ses->rtp_recv_pipe)
		  {
             ses->rtp_recv_pipe = pipe_new(ses, XRTP_RTP, XRTP_RECEIVE);
			if(!ses->rtp_recv_pipe)
				goto fail;
             ses->rtcp_recv_pipe = pipe_new(ses, XRTP_RTCP, XRTP_RECEIVE);
			if(!ses->rtcp_recv_pipe)
				goto fail;
          }
          break;
          
       case(SESSION_DUPLEX):

          ses->mode_trans = mode;
          if(!ses->rtp_send_pipe)
		  {
             ses->rtp_send_pipe = pipe_new(ses, XRTP_RTP, XRTP_SEND);
			if(!ses->rtp_send_pipe)
            {
                session_debug(("session_new: Fail to create rtp sending pipe!\n"));
				goto fail;
            }
             ses->rtcp_send_pipe = pipe_new(ses, XRTP_RTCP, XRTP_SEND);
			if(!ses->rtcp_send_pipe)
            {
                session_debug(("session_new: Fail to create rtcp sending pipe!\n"));
				goto fail;
            }
             ses->rtp_recv_pipe = pipe_new(ses, XRTP_RTP, XRTP_RECEIVE);
			if(!ses->rtp_recv_pipe)
            {
                session_debug(("session_new: Fail to create rtp receiving pipe!\n"));
				goto fail;
            }
             ses->rtcp_recv_pipe = pipe_new(ses, XRTP_RTCP, XRTP_RECEIVE);
			if(!ses->rtcp_recv_pipe)
            {
                session_debug(("session_new: Fail to create rtcp receiving pipe!\n"));
				goto fail;
            }
          }
          break;

       default:   
		   return XRTP_EUNSUP;
    }

    return XRTP_OK;





fail:
   if(ses->rtp_send_pipe) pipe_done(ses->rtp_send_pipe);
   if(ses->rtcp_send_pipe) pipe_done(ses->rtcp_send_pipe);
   if(ses->rtp_recv_pipe) pipe_done(ses->rtp_recv_pipe);
   if(ses->rtcp_recv_pipe) pipe_done(ses->rtcp_recv_pipe);

   return XRTP_FAIL;   
}
 
/**
 * Create a new Session
 */
xrtp_session_t* session_new(xrtp_set_t* set, char *cname, int clen, char *ip, uint16 default_rtp_portno, uint16 default_rtcp_portno, module_catalog_t *cata, void *media_control, int mode)
{
    char *name;
	xrtp_session_t* ses;

	ses = (xrtp_session_t *)xmalloc(sizeof(struct xrtp_session_s));
    if(!ses)
	{
       session_log(("session_new: No memory for session\n"));
       return NULL;
	}
    memset(ses, 0, sizeof(struct xrtp_session_s));

    ses->module_catalog = cata;
    
    name = (char *)xmalloc(clen + 1);
    if(!name)
	{
        session_log(("session_new: fail to allocate cname memery\n"));
        xfree(ses);
        return NULL;
    }

    memcpy(name, cname, clen);
    name[clen] = '\0';

    ses->rtcp_init = 1;
    
    ses->rtp_cancel_ts = HRTIME_INFINITY;
	strcpy(ses->ip, ip);
    
	ses->default_rtp_portno = default_rtp_portno;
	ses->default_rtcp_portno = default_rtcp_portno;

    ses->members = xlist_new();
    ses->senders = xlist_new();
    if(!ses->members || !ses->senders)
	{
       if(ses->members) xlist_done(ses->members, NULL);
       if(ses->senders) xlist_done(ses->senders, NULL);

       xfree(name);
       xfree(ses);
       return NULL;
    }

    /* Add self state struct */
    ses->self = session_new_member(ses, 0, NULL);    /* Need to assign a ssrc */
    if(!ses->self)
	{
       xlist_done(ses->members, NULL);
       xlist_done(ses->senders, NULL);

       xfree(name);
       xfree(ses);
       return NULL;      
    }

	ses->renew_level_lock = xthr_new_lock();
	ses->members_lock = xthr_new_lock();
	ses->senders_lock = xthr_new_lock();
	ses->bandwidth_lock = xthr_new_lock();
	ses->rtp_incoming_lock = xthr_new_lock();

	if(session_set_mode(ses, mode) < XRTP_OK)
	{
       xlist_done(ses->senders, NULL);
       xlist_done(ses->members, NULL);

       session_done_member(ses->self);
       
       xfree(name);
       xfree(ses);

       return NULL;
    }

   ses->clock = time_start();

   ses->media_control = media_control;

   ses->self->cname = name;
   ses->self->cname_len = clen;
    
   session_log(("session_new: cname is '%s', validated\n", ses->self->cname));
    
   session_init_seqno(ses->self, (uint16)(65535.0*rand()/RAND_MAX));

   ses->self->valid = XRTP_YES;
   ses->n_member = 1;

	ses->set = set;
    
   return ses;
 }

 uint32 session_member_max_exseqno(member_state_t * mem)
 {
    return mem->max_seq + (65536 * mem->cycles);
 }

 int session_set_id(xrtp_session_t * ses, int id){

    ses->id = id;

    return XRTP_OK;
 }

int session_id(xrtp_session_t * ses)
{
   return ses->id;
}

int session_match(xrtp_session_t *ses, char *cn, int cnlen, char *ip, uint16 rtp_pno, uint16 rtcp_pno, uint8 profno, char *proftype)
{
	if(strncmp(ses->self->cname, cn, ses->self->cname_len) != 0)
		return 0;

	if(!port_match(ses->rtp_port, ip, rtp_pno) || !port_match(ses->rtcp_port, ip, rtcp_pno))
		return 0;

	if(ses->$state.profile_no != profno)
		return 0;
	
	if(!ses->$state.profile_type || strcmp(ses->$state.profile_type, proftype) != 0)
		return 0;

	return 1;
}

int session_copy_cname(xrtp_session_t * ses, char * cname, int clen)
{
   int n;
   if(!cname || !clen)
   {
       session_log(("< session_set_cname: invalid parameters >\n"));
       return XRTP_EPARAM;
   }

   n = ses->self->cname_len < clen ? ses->self->cname_len : clen;
   memcpy(cname, ses->self->cname, n);

   cname[n-1] = '\0';

   return n;
}

char* session_cname(xrtp_session_t * ses)
{
   return ses->self->cname;
}

int session_allow_anonymous(xrtp_session_t * ses, int allow)
{
   ses->$state.receive_from_anonymous = allow;
    
   return XRTP_OK;
}

/**

 * Get session rtp and rtcp ports
 */
int session_ports(xrtp_session_t * ses, xrtp_port_t **r_rtp, xrtp_port_t **r_rtcp)
{
   *r_rtp = ses->rtp_port;
   *r_rtcp = ses->rtcp_port;

   return XRTP_OK;
}

/**
 * Get session number of rtp and rtcp ports
 */
int session_portno(xrtp_session_t * ses, int *rtp_portno, int *rtcp_portno)
{
	*rtp_portno = port_portno(ses->rtp_port);
	*rtcp_portno = port_portno(ses->rtcp_port);

	return XRTP_OK;
}

/**
 * Get session net address
 */
char* session_address(xrtp_session_t * ses)
{
	return ses->ip;
}

/**
 * Set session number of rtp and rtcp ports
 */
int session_set_portno(xrtp_session_t * ses, int rtp_portno, int rtcp_portno)
{
	if(ses->rtp_port)
		port_done(ses->rtp_port);

	if(ses->rtcp_port)
		port_done(ses->rtcp_port);

	ses->rtp_port = port_new(ses->ip, (uint16)rtp_portno, RTP_PORT);
	ses->rtcp_port = port_new(ses->ip, (uint16)rtcp_portno, RTP_PORT);

	return XRTP_OK;
}

/**
 * Get the actual payload type when it is dynamic.
 */
int session_payload_type(xrtp_session_t *ses)
{
	return ses->$state.profile_no;
}

/**

 * Get the owner's media info of the session
 */
void* session_mediainfo(xrtp_session_t *ses)
{
	return ses->self->mediainfo;
}

uint32 session_ssrc(xrtp_session_t *ses)
{
	uint32 ssrc;

	//xthr_lock(ses->self->lock);
	ssrc = ses->self->ssrc;
	//xthr_unlock(ses->self->lock);

	return ssrc;
}

xclock_t* session_clock(xrtp_session_t *session)
{
	return session->clock;
}

/**

 * Set bandwidth for this session, REMEMBER: multiuser need share the bandwidth!!!
 */
int session_set_bandwidth(xrtp_session_t * ses, int total_bw, int rtp_bw)
{
    ses->bandwidth = total_bw;
    ses->bandwidth_rtp = rtp_bw;
    ses->bandwidth_rtcp_budget = ses->bandwidth - ses->bandwidth_rtp;
    
    session_debug(("session_set_bandwidth: bandwidth[%dB/s]; rtp[%dB/s]\n", ses->bandwidth, ses->bandwidth_rtp));

    return XRTP_OK;
}

int session_bandwidth(xrtp_session_t * ses)
{
    session_debug(("session_bandwidth: bandwidth[%dB/s]\n", ses->bandwidth));

	return ses->bandwidth;
}

/*
 int session_set_rtp_bandwidth(xrtp_session_t * ses, int bw)
 {
    if(bw > ses->bandwidth) return XRTP_INVALID;
    
    if(ses->media == NULL)
	{
        ses->rtp_bw = bw;
        ses->rtcp_bw = ses->bandwidth - bw;
        return XRTP_OK;
    }

    ses->rtp_bw = bw / ses->media->clockrate;
    ses->rtcp_bw = ses->bandwidth - bw;
    return XRTP_OK;
 }
*/

uint32 session_rtp_bandwidth(xrtp_session_t * ses)
{
    session_debug(("session_set_bandwidth: rtp bandwidth[%dB/s]\n", ses->bandwidth_rtp));

    return ses->bandwidth_rtp;
}
		
/* Refer RFC-3550 A.7 */
double session_determistic_interval(int members,
                                    int senders,
                                    int rtcp_bw,
                                    int we_sent,

                                    int avg_rtcp_size,
                                    int initial)
{
       /*
        * Minimum average time between RTCP packets from this site (in
        * seconds).  This time prevents the reports from `clumping' when
        * sessions are small and the law of large numbers isn't helping
        * to smooth out the traffic.  It also keeps the report interval
        * from becoming ridiculously small during transient outages like
        * a network partition.
        */
       double const RTCP_MIN_TIME = 5.0;
       /*
        * Fraction of the RTCP bandwidth to be shared among active
        * senders.  (This fraction was chosen so that in a typical
        * session with one or two active senders, the computed report
        * time would be roughly equal to the minimum report time so that
        * we don't unnecessarily slow down receiver reports.)  The
        * receiver fraction must be 1 - the sender fraction.
        */
       double const RTCP_SENDER_BW_FRACTION = 0.25;
       double const RTCP_RCVR_BW_FRACTION = (1-RTCP_SENDER_BW_FRACTION);       

       double t;

       double rtcp_min_time = RTCP_MIN_TIME;
       int n;                      /* no. of members for computation */

       /*
        * Very first call at application start-up uses half the min
        * delay for quicker notification while still allowing some time
        * before reporting for randomization and to learn about other
        * sources so the report interval will converge to the correct
        * interval more quickly.
        */
       if(initial)
           rtcp_min_time /= 2;
       
       /*
        * Dedicate a fraction of the RTCP bandwidth to senders unless
        * the number of senders is large enough that their share is
        * more than that fraction.
        */
       n = members;

       if (senders <= members * RTCP_SENDER_BW_FRACTION)
	   {
           if (we_sent)
		   {
               rtcp_bw = (int)(rtcp_bw * RTCP_SENDER_BW_FRACTION);
               n = senders;
           } 
		   else
		   {
               rtcp_bw = (int)(rtcp_bw * RTCP_RCVR_BW_FRACTION);
               n -= senders;
           }
       }

       /*
        * The effective number of sites times the average packet size is
        * the total number of octets sent when each site sends a report.
        * Dividing this by the effective bandwidth gives the time
        * interval over which those packets must be sent in order to
        * meet the bandwidth target, with a minimum enforced.  In that
        * time interval we send one report so this time is also our
        * average time between reports.
        */
       if(rtcp_bw > 0)
		   t = avg_rtcp_size * n / rtcp_bw;
	   else
		   t = 0;

       if(t < rtcp_min_time)
		   t = rtcp_min_time; 
	   /*
	   session_debug(("session_determistic_interval: rtcp_avg[%dB]\n", avg_rtcp_size));
	   session_debug(("session_determistic_interval: rtcp_bw[%dB]\n", rtcp_bw));
	   session_debug(("session_determistic_interval: member[%d]\n", n));
	   session_debug(("session_determistic_interval: sender[%d]\n", senders));
       */
	   return t;
}
 
/* RFC 3550 A.7 */
xrtp_lrtime_t session_interval(int members,

                               int senders,
                               int rtcp_bw,
                               int we_sent,
                               int avg_rtcp_size,
                               int initial)
{
       /*
        * To compensate for "timer reconsideration" converging to a
        * value below the intended average.
        */
       double const COMPENSATION = 2.71828 - 1.5;
       
       double t = session_determistic_interval(members, senders, rtcp_bw, we_sent, avg_rtcp_size, initial);
       /*
        * To avoid traffic bursts from unintended synchronization with
        * other sites, we then pick our actual next report interval as a
        * random number uniformly distributed between 0.5*t and 1.5*t.
        */
       srand(rand());
       t = t * (1/(double)rand() + 0.5);
       t = t / COMPENSATION;

       return (rtime_t)(t*1000);  /* Convert to millisec unit */
}

int session_join(xrtp_session_t * ses, xrtp_teleport_t * rtp_port, xrtp_teleport_t * rtcp_port)
{
    /* Just tell the session there are join desire pending
     * Should be once a time
     */
    if(ses->join_to_rtcp_port)
        return XRTP_EAGAIN;

    ses->join_to_rtp_port = rtp_port;
    ses->join_to_rtcp_port = rtcp_port;

    ses->sched->rtcp_out(ses->sched, ses);

    return XRTP_OK;
}

int session_match_member_src(void * tar, void * pat)
{

    member_state_t *stat = (member_state_t *)tar;
    uint32 *src = (uint32 *)pat;

    return (stat->ssrc) - *src;

}
 

int session_match_member(void * tar, void * pat)
{
    member_state_t *stat = (member_state_t *)tar;

    member_state_t *mem = (member_state_t *)pat;

    return stat - mem;
}

/* return zero means match */
int session_match_cname(void *tar, void *pat)

{
    member_state_t * mem = (member_state_t *)tar;
    char *cn = (char*)pat;

	return strncmp(mem->cname, cn, mem->cname_len);
}

member_state_t * session_new_member(xrtp_session_t * ses, uint32 src, void *user_info)
{
	member_state_t * mem;
	
    mem = xrtp_list_find(ses->members, &src, session_match_member, &(ses->$member_list_block));
    if(mem)
	{
		return mem;
	}

    mem = (member_state_t *)xmalloc(sizeof(struct member_state_s));
    if(!mem)
	{
		session_debug(("session_new_member: No memory for member\n"));
		return NULL;
    }
    
    memset(mem, 0, sizeof(struct member_state_s));

    mem->lock = xthr_new_lock();

    if(!mem->lock)
	{
       xfree(mem);
       return NULL;
    }
    
    mem->ssrc = src;
    mem->session = ses;
    mem->user_info = user_info;

	/* recieve MIN_SEQUENTIAL rtp before became official member */
	mem->probation = MIN_SEQUENTIAL;
    
    mem->delivery_buffer = xlist_new();
	mem->delivery_lock = xthr_new_lock();
	mem->sync_lock = xthr_new_lock();
	mem->wait_seqno = xthr_new_cond(XTHREAD_NONEFLAGS);
	mem->wait_media_available = xthr_new_cond(XTHREAD_NONEFLAGS);
    
    xrtp_list_add_first(ses->members, mem);
    
    return mem;    
}

int session_delete_sender(xrtp_session_t * ses, member_state_t *senda)
{
    xrtp_list_remove(ses->senders, (void *)senda, session_match_member);

    return XRTP_OK;
}

int session_delete_cname(xrtp_session_t * ses, char *cname, int cnlen)
{
    member_state_t * member = NULL;
	int nmem;
	
	xthr_lock(ses->members_lock);
	member = xrtp_list_remove(ses->members, cname, session_match_cname);
	nmem = xlist_size(ses->members);
	xthr_unlock(ses->members_lock);

    if(member){

       ses = member->session;

       if(ses && ses->$callbacks.member_deleted){

          ses->$callbacks.member_deleted(ses->$callbacks.member_deleted_user, member->ssrc, member->user_info);
          member->user_info = NULL;
       }

       session_done_member(member);
    }

	session_log(("session_delete_cname: Member[%s] deleted, %d members now\n", cname, nmem));

    return nmem;
}

int session_delete_member(xrtp_session_t * ses, member_state_t *mem)
{
    int nmem;
	member_state_t * member = xrtp_list_remove(ses->members, (void *)mem, session_match_member);
	nmem = xlist_size(ses->members);

    if(member)
	{
       ses = member->session;

       if(ses && ses->$callbacks.member_deleted)
	   {
          ses->$callbacks.member_deleted(ses->$callbacks.member_deleted_user, member->ssrc, member->user_info);
          member->user_info = NULL;

       }

       session_done_member(member);
    }

    return nmem;
}

member_state_t* session_member_state(xrtp_session_t * ses, uint32 src)

{
    member_state_t * stat = xlist_find(ses->members, &src, session_match_member_src, &(ses->$member_list_block));
    return stat;
}

member_state_t* session_owner(xrtp_session_t * ses)
{
	return ses->self;
} 

int session_member_update_by_rtp(member_state_t* mem, xrtp_rtp_packet_t* rtp)
{
	uint16 seqno = rtp_packet_seqno(rtp);

    if(rtp->member_state_updated || mem->session->bye_mode)
	{
		session_log(("session_member_update_by_rtp: No need to update\n"));
        return XRTP_OK;
	}

   
	if(!session_update_seqno(mem, seqno))
	{
		session_log(("session_member_update_by_rtp: Invalid packet sequence number\n"));
		return XRTP_INVALID;
	}

	if((int32)seqno - (int32)mem->expect_seqno < 0)
	{
		session_log(("session_member_update_by_rtp: sequence number stall\n"));
		return XRTP_INVALID;
	}

    mem->n_payload_oct_received += rtp_packet_payload_bytes(rtp);
    session_log(("session_member_update_by_rtp: Member[%u]:{exseqno[%u],packet[%d],%uB}\n", mem->ssrc, session_member_max_exseqno(mem), mem->n_rtp_received, mem->n_payload_oct_received));

    rtp->member_state_updated  = 1;

    return XRTP_OK;
}

int session_member_seqno_stall(member_state_t *mem, int32 seqno)
{
	return (seqno - mem->expect_seqno) < 0;
}

int member_deliver_media_loop(void *gen)
{
	xlist_user_t lu;

	xrtp_session_t *session;
	member_state_t *mem;

	rtime_t us_play, us_now, us_sleep, us_remain;
	media_hold_t *hold;

	mem = (member_state_t*)gen;
   session = mem->session;

	mem->stop_delivery = 0;

   //session_debug(("member_deliver_media_loop: media from member[%s], seqno[%d], packno[%lld]\n", mem->cname, mem->expect_seqno, mem->packetno));
	while(1)
	{
         xthr_lock(mem->delivery_lock);

		if(mem->stop_delivery)
		{
			/* tell player eos */
			session_debug(("member_deliver_media_loop: stop\n"));

			session->media->play(mem->media_player, NULL, ++mem->packetno, 1, 1);
         
	        xthr_unlock(mem->delivery_lock);
			break;
		}

		 hold = (media_hold_t*)xlist_first(mem->delivery_buffer, &lu);
         if(!hold)
         {
	        xthr_cond_wait(mem->wait_media_available, mem->delivery_lock);
			xthr_unlock(mem->delivery_lock);

			continue;
         }

		us_play = hold->usec_ref + hold->usec;
		us_now = time_usec_now(session->clock);
		us_sleep = us_play - us_now;

		if((hold->seqno - mem->expect_seqno) <= 0 || us_sleep <= 0)

		{
		   xlist_remove_first(mem->delivery_buffer);
         
			if(us_sleep > 0)
			{
			   session_debug(("member_deliver_media_loop: sleep [%dus]\n", us_sleep));
				time_usec_sleep(session->clock, us_sleep, &us_remain);
			}
         
			/* if a gap, packetno is discontinuely by one */
			if((hold->seqno - mem->expect_seqno) > 0)
				++mem->packetno;

			mem->timestamp_playing = hold->rtpts_play;
			mem->usec_playing = us_now;

			/* now decode and play */
			session->media->play(mem->media_player, hold->media, ++mem->packetno, hold->full_payload, 0);

			xthr_unlock(mem->delivery_lock);

			/* free the whole payload */
			if(hold->full_payload)
			{
				mem->expect_seqno = hold->seqno + 1;

				/* in case, several packet in one payload memory */
				xfree(hold->memory);
			}

			xfree(hold);
		}
		else if((hold->seqno - mem->expect_seqno) > 0)
		{
			/* incoming packet in misorder or packet lost, check again later */
			mem->misorder = 1;

			session_debug(("member_deliver_media_loop: wait_ms[%u]\n", (us_now - hold->usec_ref + hold->usec)/1000));

			xthr_cond_wait_millisec(mem->wait_seqno, mem->delivery_lock, (us_now - hold->usec_ref + hold->usec)/1000);
		}
   }

   mem->deliver_thread = NULL;

	return XRTP_OK;
}

int session_cmp_media_usec(void *tar, void *pat)

{
	media_hold_t * med_t = (media_hold_t*)tar;
   media_hold_t * med_p = (media_hold_t*)pat;

   return med_t->seqno - med_p->seqno;
}

/******************************************************************************** 
 * NOTE: Only expected seqno or later will be cached, all seqno before expected 
 * should be discarded
 */
int session_member_hold_media(member_state_t * mem, void *media, int bytes, uint16 seqno, uint32 rtpts, rtime_t us_ref, rtime_t us, int full_payload, void *memblock)
{
	/*xrtp_session_t *ses = mem->session;*/
   int n_hold = 0;
	media_hold_t *hold = xmalloc(sizeof(media_hold_t));

	hold->media = media;
	hold->bytes = bytes;
	hold->seqno = seqno;
	hold->rtpts_play = rtpts;
	hold->usec_ref = us_ref;
	hold->usec = us;
	hold->full_payload = full_payload;
	hold->memory = memblock;	

	xthr_lock(mem->delivery_lock);

	xlist_addonce_ascent(mem->delivery_buffer, hold, session_cmp_media_usec);
	n_hold = xlist_size(mem->delivery_buffer);

	xthr_unlock(mem->delivery_lock);
   
    //session_debug(("\r session_member_hold_media: [%d+%d]us spx[%dB] %d\n", hold->usec_ref, hold->usec = us, hold->bytes, n_hold));

	if(mem->misorder)
		xthr_cond_signal(mem->wait_seqno);


	xthr_cond_signal(mem->wait_media_available);

	return XRTP_OK;
}
 
int session_member_deliver(member_state_t * mem, uint16 seqno, int64 packetno)
{
   if(mem->deliver_thread)
		return XRTP_OK;
	
	mem->expect_seqno = seqno;
	mem->packetno = packetno; /* count for decoder */
   
   mem->stop_delivery = 0;
	mem->deliver_thread = xthr_new(member_deliver_media_loop, mem, XTHREAD_NONEFLAGS);

   return XRTP_OK;
}

int session_member_media_playing(member_state_t * mem, uint32 *rtpts_playing, rtime_t *ms_playing, rtime_t *us_playing, rtime_t *ns_playing)
{
	*rtpts_playing = mem->timestamp_playing;
	*ms_playing = mem->msec_playing;
	*us_playing = mem->usec_playing;
	*ns_playing = mem->nsec_playing;

	return XRTP_OK;
}

/**
 * return rtpts_sync - rtpts_remote_sync;
 */
uint32 session_member_sync_point(member_state_t *mem, uint32 *rtpts_sync, rtime_t *ms_sync, rtime_t *us_sync, rtime_t *ns_sync)
{
	uint32 rtpts_d;

	xthr_lock(mem->sync_lock);

	*rtpts_sync = mem->timestamp_sync;
   
	*ms_sync = mem->msec_sync;
	*us_sync = mem->usec_sync;
	*ns_sync = mem->nsec_sync;

	rtpts_d = mem->timestamp_sync - mem->timestamp_remote_sync;

	xthr_unlock(mem->sync_lock);

	return rtpts_d;
}

int64 session_member_samples(member_state_t *mem, uint32 rtpts_payload)
{
	if(mem->samples == 0)
	{
		mem->rtpts_last_payload = rtpts_payload;
		mem->samples = 1;
	}
	else
	{
      mem->samples += rtpts_payload - mem->rtpts_last_payload;
		mem->rtpts_last_payload = rtpts_payload;
	}

	return mem->samples;
}

int session_member_set_connects(member_state_t * mem, session_connect_t *rtp_conn, session_connect_t *rtcp_conn)
{
    if(mem->rtp_connect && mem->rtcp_connect)
        return XRTP_OK;
        
    if(mem->session->$callbacks.member_connects)
	 {
      session_log(("session_member_set_connects: Callback to application for rtp and rtcp port allocation\n"));

      mem->session->$callbacks.member_connects(mem->session->$callbacks.member_connects_user, mem->ssrc, &rtp_conn, &rtcp_conn);
    }
	else
	{

      if(!rtp_conn && !rtcp_conn)
	  {
          session_log(("< session_member_set_connects: Need at least a connect of rtp or rtcp >\n"));
          return XRTP_EPARAM;
      }
      
      session_log(("session_member_set_connects: RFC1889 static port allocation\n"));
      
      if(!rtp_conn) rtp_conn = connect_rtcp_to_rtp(rtcp_conn);
      if(!rtcp_conn) rtcp_conn = connect_rtp_to_rtcp(rtp_conn);      
    }

    mem->rtp_connect = rtp_conn;
    mem->rtcp_connect = rtcp_conn;

	if(!mem->rtp_port)
		mem->rtp_port = connect_new_teleport(rtp_conn);

	if(!mem->rtcp_port)
		mem->rtcp_port = connect_new_teleport(rtcp_conn);

    return XRTP_OK;
}
 
int session_member_connects(member_state_t * mem, session_connect_t **rtp_conn, session_connect_t **rtcp_conn)
{
    *rtp_conn = mem->rtp_connect;
    *rtcp_conn = mem->rtcp_connect;

    return XRTP_OK;
}

int session_quit(xrtp_session_t * ses, int immidiately)
{
    ses->bye_mode = 1;


    if((immidiately = 1)) /* Always immidiately quit */
	 {     
       /* No Bye packet sent, other peers will timeout it */       
       session_done(ses);

       return XRTP_OK;
    }

    /* BYE Reconsideration is not implemented yet */

    return XRTP_OK;

}

/* Convert hrt to rtp ts */
uint32 session_hrt2ts(xrtp_session_t * ses, xrtp_hrtime_t hrt)
{
    return hrt / ses->media->clockrate * ses->media->sampling_instance;
}


/* Convert rtp ts to hrt */
xrtp_hrtime_t session_ts2hrt(xrtp_session_t * ses, uint32 ts)
{
    return ts / ses->media->sampling_instance * ses->media->clockrate;
}

 /* Map the rtp timestamp to the local playout timestamp 
  * level: jitter adjust level
  */
uint32 session_member_mapto_local_time(member_state_t * mem, uint32 rtpts_remote, uint32 rtpts_local, int level)
{
    rtime_t base_playout_time, desired_playout_offset, playout_offset = 0;

    rtime_t ms_now;
     
    double delta_var = 0;
	 uint32 rtpts_playout;
	 uint32 transit = 0;
     uint32 adj_skew = 0;
     
	 rtime_t skew_threshold = 0;
	 int delta_transit = 0;


     xthr_lock(mem->lock);

     
     transit = rtpts_local - rtpts_remote;
	 session_log(("session_member_mapto_local_time: l[#%u] - r[#%u] = delay[%u]\n", rtpts_local, rtpts_remote, transit));

     /* Adjust clock skew */
     skew_threshold = session_rtp_period(mem->session);
     
     if(mem->n_rtp_received == 1)

	 {
        /* First valid rtp packet */
		mem->delay_estimate = transit;
        mem->active_delay = transit;
        mem->last_transit = mem->last_last_transit = transit;

     }
	 else
	 {
        /* following packets */

		/* averaging low 16 bit, for some platform poor high value support */
		uint32 e16 = mem->delay_estimate & 0xFFFF;
		uint32 t16 = transit & 0xFFFF;
		uint32 e = (31*e16 + t16)/32;

		/* final 32bit result */
		mem->delay_estimate = (mem->delay_estimate & 0xFFFF0000) | (e & 0xFFFF);
	 }

	 session_log(("session_member_mapto_local_time: active[%u], estimate[%u]\n", mem->active_delay, mem->delay_estimate));

     if(mem->active_delay - mem->delay_estimate > skew_threshold)
	 {
        adj_skew = skew_threshold;
        mem->active_delay = mem->delay_estimate;
     }

     if(mem->active_delay - mem->delay_estimate < -skew_threshold)
	 {
        adj_skew = -skew_threshold;
        mem->active_delay = mem->delay_estimate;

     }

     /*
      * The minimum offset is base on the window of difference since

      * the last compensation for the skew adjustment.
      */
     if(adj_skew)
        mem->min_transit = transit;
	 else
        mem->min_transit = mem->min_transit > transit ? transit : mem->min_transit;

     base_playout_time = rtpts_remote + mem->min_transit;
     
     /* Adjust due to jitter */
     delta_transit = transit - mem->last_transit;
     if (delta_transit < 0)
		 delta_transit = -delta_transit;     /* abs(x) */

     session_log(("session_member_mapto_local_time: |delay[%u]-last[%u]|=delta[%d]\n", transit, mem->last_transit, delta_transit));

     mem->last_last_transit = mem->last_transit;
     mem->last_transit = transit;


     /* FIXME: Thershold should be adjustable */
	 if(delta_transit > SPIKE_THRESHOLD)
	 {     
        /* A new delay spike started */
        mem->play_mode = PLAYMODE_SPIKE;
        mem->spike_var = 0;
        mem->playout_adapt = 0;
        mem->in_spike = IN_SPIKE;
     }
	 else
	 {
        if(mem->play_mode == PLAYMODE_SPIKE)
		{
           mem->spike_var /= 2;
           delta_var = (abs(transit - mem->last_transit) + abs(transit - mem->last_last_transit))/8;
           mem->spike_var += delta_var;

           
           if(mem->spike_var < mem->spike_end)
		   {
              /* Slope is flat, return to normal operation */
              mem->play_mode = PLAYMODE_NORMAL;
              
              /* Not a spike, continue update jitter */
              mem->jitter += (1./16.) * ((double)delta_transit - mem->jitter);
           }
           mem->playout_adapt = 0;
        }
		else
		{
           /* Maintain spike end */
           if(!mem->in_spike)
		   {
              mem->spike_end = (abs(transit - mem->last_transit) + abs(transit - mem->last_last_transit))/8;
           }
		   else
		   {
              mem->in_spike--;
           }
           
           if(mem->consecutive_dropped > CONSECUTIVE_DROP_THRESHOLD)
		   {
              /* Too many consecutive packet dropped, need adaption */
              mem->playout_adapt = 1;
           }
           
           /* heart beat */

		   ms_now = time_msec_now(session_clock(mem->session));
           if((ms_now - mem->msec_last_heard) > INACTIVE_THRESHOLD)
		   {
              /* Silent source restartted, network condition maybe changed */

              mem->playout_adapt = 1 ;
           }
           mem->msec_last_heard = ms_now;

           /* Not a spike, continue update jitter */
           mem->jitter += (1./16.) * ((double)delta_transit - mem->jitter);
        }
     }
     
	 desired_playout_offset = level * (int)mem->jitter;
     if(mem->playout_adapt)
	 {
        mem->last_playout_offset = playout_offset;
        playout_offset = desired_playout_offset;
     }
	 else
	 {
        playout_offset = mem->last_playout_offset;
     }

     rtpts_playout = base_playout_time + adj_skew + playout_offset;
     xthr_unlock(mem->lock);

     session_log(("session_member_mapto_local_time: adjust level[%d], skew[%d], play offset[%d]\n", level, adj_skew, playout_offset));
     session_log(("session_member_mapto_local_time: playout[#%u], base[#%u]\n", rtpts_playout, base_playout_time));

     return rtpts_playout;
}

int session_member_synchronise(member_state_t *mem, uint32 ts_remote, uint32 hi_ntp, uint32 lo_ntp, int clockrate)
{
	rtime_t ms,us,ns;
   uint32 timestamp_tosync;

   int32 delta_sync;

	xrtp_session_t *ses = mem->session;
	
	time_rightnow(ses->clock, &ms,&us,&ns);

	/* refer point in advance */
	xthr_lock(mem->sync_lock);


	timestamp_tosync = mem->timestamp_last_sync + (uint32)((us - mem->usec_last_sync) / 1000000.0 * clockrate);

   /* make sure playing always beyond sync point */
   delta_sync = mem->timestamp_playing - timestamp_tosync;
   if(delta_sync > 0)
   {
	   mem->timestamp_last_sync = mem->timestamp_sync;
	   mem->timestamp_sync = timestamp_tosync;

      mem->timestamp_remote_sync = ts_remote;

	   mem->msec_last_sync = mem->msec_sync;
	   mem->msec_sync = ms;
	
	   mem->usec_last_sync = mem->usec_sync;
	   mem->usec_sync = us;
	
	   mem->nsec_last_sync = mem->nsec_sync;
	   mem->nsec_sync = ns;
   }

	xthr_unlock(mem->sync_lock);
	
   if(delta_sync > 0)
   {
      session_log(("\r\nsession_member_synchronise: ---------------------------\r\n"));
	   session_log(("session_member_synchronise: ts_remote[%d], remote_ntp[%u:%u]\r\n", ts_remote, hi_ntp, lo_ntp));
	   session_log(("session_member_synchronise: ts_local[%d], us_local_now[%dus]\r\n", mem->timestamp_sync, us));
	   session_log(("session_member_synchronise: ---------------------------\r\n"));
   }
	if(ses->media->sync && hi_ntp && lo_ntp)
		ses->media->sync(mem->media_player, ts_remote, hi_ntp, lo_ntp);

	return XRTP_OK;
}


uint32 session_make_ssrc(xrtp_session_t* ses)
{
    md5_state_t sign;
    uint32 salt;
    uint32 digest[4];

    uint32 ssrc = 0;
	
	do
	{
		int i;
		md5_init(&sign);

		salt = session_signature(ses);
		md5_append(&sign, (md5_byte_t*)(&salt), sizeof(salt));

		salt = ses->media->sign(ses->media);
		md5_append(&sign, (md5_byte_t*)(&salt), sizeof(salt));

		md5_finish(&sign, (md5_byte_t *)digest);

		for (i = 0; i < 3; i++)
		{
			ssrc ^= digest[i];
		}
	}
	while(ses->n_member > 1 && session_member_state(ses, ssrc));

    ses->self->ssrc = ssrc;

    return ssrc;
 }

 
/* retrieve media info of the member */
void* session_member_mediainfo(member_state_t *mem, uint32 *rtpts, int *signum)
{
	*rtpts = mem->rtpts_minfo;

	*signum = mem->minfo_signum;
	
	return mem->mediainfo;
}

/* new media info for the member */
void* session_member_update_mediainfo(member_state_t *mem, void *minfo, uint32 rtpts, int signum)
{
	void *exmi = mem->mediainfo;

	mem->mediainfo = minfo;
	mem->minfo_signum = signum;
	mem->rtpts_minfo = rtpts;

	return exmi;
}

/* check if need to renew the media info */
int session_member_renew_mediainfo(member_state_t *mem, void *minfo, uint32 rtpts, int signum)
{
	if(mem->minfo_signum != signum)
	{
		if(mem->mediainfo)
		mem->rtpts_minfo = rtpts;
		mem->minfo_signum = signum;

		return 1;
	}

	return 0;
}

/* session is asked to distribute media info to members */
int session_issue_mediainfo(xrtp_session_t *ses, void *minfo, int signum)
{
	if(signum != ses->self->minfo_signum || ses->self->mediainfo == NULL)
	{
		session_debug(("session_issue_media_info: mediainfo[#%d] @%d\n", signum, (uint)minfo));
   
		ses->self->mediainfo = minfo;
		ses->self->minfo_signum = signum;
	}

   xthr_lock(ses->renew_level_lock);
	ses->renew_minfo_level = RENEW_MINFO_LEVEL;

	xthr_unlock(ses->renew_level_lock);

	session_log(("session_issue_media_info: pending ...\n"));
   
	return XRTP_OK;
}

/* check if need to send media info to member */
int session_distribute_mediainfo(xrtp_session_t *ses, uint32 *rtpts, int *signum, void **minfo_ret)
{
	int n;

	*rtpts = ses->self->rtpts_minfo;
	*signum = ses->self->minfo_signum;
	*minfo_ret = ses->self->mediainfo;

	xthr_lock(ses->renew_level_lock);
	n = ses->renew_minfo_level--;
	xthr_unlock(ses->renew_level_lock);


	if(n > 0 && *minfo_ret != NULL)
	{
		session_log(("session_distribute_mediainfo: [%s] renew level = %d\n", ses->self->cname, n));
		return 1;
	}

	session_log(("session_distribute_mediainfo: [%s] has no media info to issue\n", ses->self->cname));

	return 0;
}

void* session_member_player(member_state_t *mem)
{
	return mem->media_player;
}

void* session_member_set_player(member_state_t *mem, void *player)
{
	void *ex = mem->media_player;



	if(mem->media_player != player)
		mem->media_player = player;

	return ex;
}

void* session_media_control(xrtp_session_t *ses)
{
	return ses->media_control;
}

uint32 session_signature(xrtp_session_t* ses)
{
    if(ses->$callbacks.need_signature)
        return ses->$callbacks.need_signature(ses->$callbacks.need_signature_user, ses);

	srand((int)ses);

    return rand();
}

int session_solve_collision(member_state_t * mem, uint32 ssrc)
{

     xrtp_session_t * ses = mem->session;

     if(ses->self == mem)
	 {

        /* The session is collided */
        
        if(ses->$callbacks.before_reset)
            ses->$callbacks.before_reset(ses->$callbacks.before_reset_user, ses);
            
        /* Silently changed, old ssrc will timeout by other participants
         * May implements bye mode later.
         */
        session_make_ssrc(ses);
        ses->$state.n_pack_sent = ses->$state.oct_sent = 0;

        if(ses->$callbacks.after_reset)
            ses->$callbacks.after_reset(ses->$callbacks.after_reset_user, ses);
     }

     if(ses->self != mem)
	 {
        /* A third party collision detected 
         * Nothing to do, just keep receiving the media for the exist member, ignore the
         * new coming participant, the situation will be solved soon by the collided parties.
         */
     }

     return XRTP_OK;
}

int session_add_cname(xrtp_session_t * ses, char *cn, int cnlen, char *ipaddr, uint16 rtp_portno, uint16 rtcp_portno, void *rtp_capable, void *userinfo)
{
	xlist_user_t lu;
	member_state_t *mem;

	int nmem;

	xthr_lock(ses->members_lock);

	mem = xlist_find(ses->members, cn, session_match_cname, &lu);
	if(mem)
	{

		xthr_unlock(ses->members_lock);

		/* info of the remote media to playback */
		if(!mem->mediainfo && rtp_capable)
			mem->mediainfo = ses->media->info(ses->media, rtp_capable);

		return XRTP_OK;
	}

   mem = (member_state_t *)xmalloc(sizeof(struct member_state_s));
   if(!mem)
	{
		session_debug(("session_new_member: No memory for member\n"));
		xthr_unlock(ses->members_lock);
		return XRTP_FAIL;
   }
    
   memset(mem, 0, sizeof(struct member_state_s));

   mem->lock = xthr_new_lock();
   if(!mem->lock)
	{
      xfree(mem);
		xthr_unlock(ses->members_lock);
		return XRTP_FAIL;
   }
    
	/* mem ssrc is just set by incoming rtcp packet 
	 * if ssrc == 0, means the member is not verified 
	 */
   mem->ssrc = 0;
   mem->session = ses;
   mem->user_info = userinfo;

	/* recieve MIN_SEQUENTIAL rtp before became official member */
	mem->probation = MIN_SEQUENTIAL;
    
   mem->delivery_buffer = xlist_new();
   if(!mem->delivery_buffer)
	{
      xthr_done_lock(mem->lock);
		xfree(mem);

		xthr_unlock(ses->members_lock);
		return XRTP_FAIL;
   }
    
	mem->delivery_lock = xthr_new_lock();
	mem->sync_lock = xthr_new_lock();
	mem->wait_seqno = xthr_new_cond(XTHREAD_NONEFLAGS);
	mem->wait_media_available = xthr_new_cond(XTHREAD_NONEFLAGS);
    
   xrtp_list_add_first(ses->members, mem);

	nmem = xlist_size(ses->members);

	mem->cname = xstr_nclone(cn, cnlen);
	mem->cname_len = cnlen;

	/* used to verify the incoming */
	mem->rtp_port = teleport_new(ipaddr, rtp_portno);
	mem->rtcp_port = teleport_new(ipaddr, rtcp_portno);

	mem->rtp_connect = connect_new(ses->rtp_port, mem->rtp_port);
	mem->rtcp_connect = connect_new(ses->rtcp_port, mem->rtcp_port);

	xthr_unlock(ses->members_lock);

	if(rtp_capable)
		mem->mediainfo = ses->media->info(ses->media, rtp_capable);
	
	session_log(("session_add_cname: cn[%s]@[%s:%u|%u] added, %d participants\n", cn, ipaddr, rtp_portno, rtcp_portno, nmem));

	/* start the rtcp schedule */
	if(nmem > 1)
		ses->sched->rtcp_out(ses->sched, ses);

	return nmem;
}

int session_reset_member(xrtp_session_t* ses, member_state_t *mem)
{
   int ret_th = 0;

	xthr_lock_t* delivery_lock;
	xthr_lock_t* sync_lock;
	xthr_cond_t* wait_seqno;
	xthr_cond_t* wait_media_available;
   xthr_lock_t* mem_lock;

   char* cname;
	int cname_len;

	xrtp_teleport_t* mem_rtp_port;
   xrtp_teleport_t* mem_rtcp_port;
   session_connect_t* mem_rtp_connect;
   session_connect_t* mem_rtcp_connect;

   void* user_info;

   if(mem->deliver_thread)
	{
	   mem->stop_delivery = 1;
	   xthr_wait(mem->deliver_thread, &ret_th);
	}

	xlist_done(mem->delivery_buffer, session_done_media);

   if(mem->mediainfo)
   {
      ses->media->done_info(ses->media, mem->mediainfo);
      mem->mediainfo = NULL;
   }

	delivery_lock = mem->delivery_lock;
	sync_lock = mem->sync_lock;
	wait_seqno = mem->wait_seqno;
	wait_media_available = mem->wait_media_available;

   mem_lock = mem->lock;

   cname = mem->cname;
	cname_len = mem->cname_len;

	mem_rtp_port = mem->rtp_port;
   mem_rtcp_port = mem->rtcp_port;
   mem_rtp_connect = mem->rtp_connect;
   mem_rtcp_connect = mem->rtcp_connect;

   user_info = mem->user_info;

   memset(mem, 0, sizeof(member_state_t));


   mem->delivery_buffer = xlist_new();

   mem->session = ses;
   mem->user_info = user_info;

	/* recieve MIN_SEQUENTIAL rtp before became official member */
	mem->probation = MIN_SEQUENTIAL;

	mem->delivery_lock = delivery_lock;
	mem->sync_lock = sync_lock;
	mem->wait_seqno = wait_seqno;
	mem->wait_media_available = wait_media_available;
   mem->lock = mem_lock;

   mem->cname = cname;
	mem->cname_len = cname_len;

	mem->rtp_port = mem_rtp_port;
   mem->rtcp_port = mem_rtcp_port;
   mem->rtp_connect = mem_rtp_connect;
   mem->rtcp_connect = mem_rtcp_connect;

   mem->user_info = user_info;

   return XRTP_OK;
}

/**
 * Move member from one rtp session to other session.
 */
int session_move_member(xrtp_session_t *ses, xrtp_session_t *to_ses, member_state_t* mem)
{
   xthr_lock(ses->members_lock);

   /* FIXME: xthr_lock(ses->senders_lock); */
   xlist_remove_item(ses->members, mem);

   if(mem->we_sent && xlist_remove_item(ses->senders, mem) >= OS_OK)
         ses->n_sender--;

   ses->n_member--;

   xthr_unlock(ses->members_lock);

   session_reset_member(ses, mem);

   session_add_member(to_ses, mem);

   return XRTP_OK;
}

/**
 * Move all members except self from one rtp session to other session.
 */
int
session_move_all_guests(xrtp_session_t *ses, xrtp_session_t *to_ses)
{
   xlist_user_t lu;

   member_state_t* mem = (member_state_t*)xlist_first(ses->members, &lu);
   while(mem)
   {
      if(mem != ses->self)
         session_move_member(ses, to_ses, mem);
      
      mem = (member_state_t*)xlist_next(ses->members, &lu);
   }

   if(ses->n_member == 1)
   {
      session_set_scheduler(ses, NULL);  
   }

   session_debug(("session_move_all_guests: Session[%s]-->Session[%s]\n", ses->self->cname, to_ses->self->cname));
   
   return XRTP_OK;   
}

/**
 * Move member from one rtp session to other session.
 */
member_state_t *
session_move_member_by_cname(xrtp_session_t *ses, xrtp_session_t *to_ses, char *cn)
{
	member_state_t *mem;

	xthr_lock(ses->members_lock);

	mem = xlist_remove(ses->members, cn, session_match_cname);
	if(!mem)
	{
		xthr_unlock(ses->members_lock);
		return NULL;
   }

   /* FIXME: xthr_lock(ses->senders_lock); */
   if(mem->we_sent && xlist_remove_item(ses->senders, mem) >= OS_OK)
         ses->n_sender--;


   ses->n_member--;

   xthr_unlock(ses->members_lock);

   session_reset_member(ses, mem);

   session_add_member(to_ses, mem);

   return mem;
}

int session_add_member(xrtp_session_t* ses, member_state_t* mem)
{
   xlist_user_t lu;

   member_state_t* m = xlist_find(ses->members, mem->cname, session_match_cname, &lu);
   if(m)
   {
      return XRTP_FAIL;
   }


   xthr_lock(ses->members_lock);

   xlist_addto_first(ses->members, mem);
   ses->n_member = xlist_size(ses->members);


   mem->session = ses;

   connect_set_session(mem->rtcp_connect, ses);
   connect_set_session(mem->rtp_connect, ses);

   xthr_unlock(ses->members_lock);

   return XRTP_OK;
}

member_state_t * session_update_member_by_rtcp(xrtp_session_t * ses, xrtp_rtcp_compound_t * rtcp)
{
   uint32 ssrc = rtcp_sender(rtcp);

   char * cname = NULL;
   int cname_len;

   session_connect_t *rtp_conn = NULL;
   session_connect_t *rtcp_conn = NULL;
   member_state_t * mem = NULL;
   int ssrc_collise = 0;

   session_log(("session_update_member_by_rtcp: my ssrc[%u], incoming ssrc[%u]\n", ses->self->ssrc, ssrc));

   cname_len = rtcp_sdes(rtcp, ssrc, RTCP_SDES_CNAME, &cname);

   /* Get connect of the participant */
   rtcp_conn = rtcp->connect;
   rtcp->connect = NULL; /* Dump the connect from rtcp packet */

	if(ssrc == 0)
	{
		session_debug(("session_update_member_by_rtcp: refuse illegal ssrc\n"));

      return NULL;
	}

	/* collision detect */
	if(ssrc == ses->self->ssrc)
	{
		/* collise with me, i need to change ssrc */
		uint32 my_ssrc;

		ses->self->in_collision = 1;
		my_ssrc = session_make_ssrc(ses);
		ses->self->in_collision = 0;

		ssrc_collise = 1;


		session_log(("session_update_member_by_rtcp: ssrc collises with cn[%s], [%s]'s new ssrc[%u]\n", cname, ses->self->cname, my_ssrc));
        
        if(rtp_conn) 
			connect_done(rtp_conn);

        if(rtcp_conn) 
			connect_done(rtcp_conn);

		return NULL;
	}

    /* search by ssrc */
	mem = session_member_state(ses, ssrc);
	if(!mem)
	{
		/* search by cname */
		xlist_user_t lu;

		session_log(("session_update_member_by_rtcp: search by cname[%s]\n", cname));

		mem = xlist_find(ses->members, cname, session_match_cname, &lu);
	}

   if(!mem)
	{
      void * user_info = NULL;

      session_debug(("session_update_member_by_rtcp: New visitor[%d] coming\n", ssrc));

      /* Need Authorization */
      if(ses->$callbacks.member_apply)
		{
         if(ses->$callbacks.member_apply(ses->$callbacks.member_apply_user, ssrc, cname, cname_len, &user_info) < XRTP_OK)
		   {
            /* Member application fail */
            if(rtp_conn) 
				   connect_done(rtp_conn);

            if(rtcp_conn) 
				   connect_done(rtcp_conn);
               
            session_debug(("session_update_member_by_rtcp: SSRC[%d]'s member application refused\n", ssrc));
               
			   return NULL;
         }
      }   

      if(cname_len==0 && !ses->$state.receive_from_anonymous)
		{
           /* Anonymous intend */
           if(rtp_conn) 
			   connect_done(rtp_conn);

           if(rtcp_conn) 
			   connect_done(rtcp_conn);

           session_debug(("session_update_member_by_rtcp: Anonymous refused\n"));
           return NULL;
      }

      mem = session_new_member(ses, ssrc, user_info);

      if(!mem)
		{
         if(rtp_conn) 
			   connect_done(rtp_conn);
           
		   if(rtcp_conn) 
			   connect_done(rtcp_conn);

         session_debug(("session_update_member_by_rtcp: Member[%d] can not created\n", ssrc));

         return NULL;
      }

      session_debug(("session_update_member_by_rtcp: new member ssrc[%u] created\n", ssrc));
   }

	{

      /* Free as member has connects already, Can't be validated yet */
      if(!mem->valid && cname_len == 0)

		{
			if(rtp_conn) 
				connect_done(rtp_conn);
			if(rtcp_conn) 
				connect_done(rtcp_conn);
        
         session_debug(("session_update_member_by_rtcp: No cname yet to validate member[%u]\n", ssrc));

         return NULL;
		}

		if(ssrc_collise)
		{
			/* member is expect to change ssrc in the future */
			mem->in_collision = 1;
            session_debug(("session_update_member_by_rtcp: [%s] collise session[%s]\n", cname, ses->self->cname));
		}
		else if(mem->rtcp_port && !connect_match_teleport(rtcp_conn, mem->rtcp_port))
		{
			/* third party collision detect*/
			mem->in_collision = 1;

            session_debug(("session_update_member_by_rtcp: mem[%u] collise with [%s]\n", mem->ssrc, cname));
		}
		else if(mem->in_collision == 1)
		{
         session_debug(("session_update_member_by_rtcp: WARNING! for security or in multicast environment, ssrc need to be authenticated before accept\n"));
			
			mem->ssrc = ssrc;

         session_debug(("session_update_member_by_rtcp: cname[%s] ssrc[%u], collision solved\n", cname, ssrc));
		}


		mem->in_collision = 0;
        
		if(mem->valid)
		{
			session_log(("session_update_member_by_rtcp: return validated member[%s]\n", mem->cname));

			if(rtp_conn) 
				connect_done(rtp_conn);

			if(rtcp_conn) 
				connect_done(rtcp_conn);

			/* callback when the member is valid */
			session_log(("session_update_member_by_rtcp: ses->$callbacks.member_update@%x\n", (int)ses->$callbacks.member_update));
		
			if(ses->$callbacks.member_update)
				ses->$callbacks.member_update(ses->$callbacks.member_update_user, ssrc, cname, cname_len);

			return mem;
		}
   }

	session_debug(("session_update_member_by_rtcp: To validate a petential member\n"));

	/* Validate a petential member */
   if(!mem->valid && cname_len > 0)
	{    
		if(!mem->cname)
		{
			mem->cname = xstr_nclone(cname, cname_len);
			mem->cname_len = cname_len;

			/* Free as member has connects already */
			if(rtp_conn) 
				connect_done(rtp_conn);
			if(rtcp_conn) 
				connect_done(rtcp_conn);

			session_log(("session_update_member_by_rtcp: ssrc[%u] identified as member[%s]\n", ssrc, cname));

			return mem;
		}
                
      if(mem->rtcp_port && !connect_match_teleport(rtcp_conn, mem->rtcp_port))
		{
			/* WARNING: Someone else tempt to break in */
			if(rtp_conn) 
				connect_done(rtp_conn);
			if(rtcp_conn) 
				connect_done(rtcp_conn);

			session_debug(("session_update_member_by_rtcp: Warning, ssrc[%u] intrusion detected!!!\n", ssrc));

			return NULL;
		}

		if(strncmp(mem->cname, cname, mem->cname_len) != 0)
		{
			/* WARNING: Someone else tempt to break in */
			if(rtp_conn) connect_done(rtp_conn);
			if(rtcp_conn) connect_done(rtcp_conn);

			session_debug(("session_update_member_by_rtcp: Warning, member[%s] collide with cname[%s]!!!\n", mem->cname, cname));
			
			return NULL;
		}


		session_log(("session_update_member_by_rtcp: to validate member[%s]\n", mem->cname));

		session_member_set_connects(mem, rtp_conn, rtcp_conn);

		mem->ssrc = ssrc;
      mem->valid = XRTP_YES;
      ses->n_member++;


      session_log(("session_update_member_by_rtcp: [%s],ssrc[%u]\n", cname, ssrc));

      if(ses->join_to_rtcp_port && connect_match_teleport(rtcp_conn, ses->join_to_rtcp_port))
		{
         session_log(("session_update_member_by_rtcp: [%s:%d] join succeed\n", teleport_name(ses->join_to_rtcp_port), teleport_portno(ses->join_to_rtcp_port)));

         /* participant joined, clear the join desire */
         teleport_done(ses->join_to_rtp_port);
         ses->join_to_rtp_port = NULL;
            
         teleport_done(ses->join_to_rtcp_port);
         ses->join_to_rtcp_port = NULL;
      }

      session_log(("session_update_member_by_rtcp: Member[%u][%s] validated\n", mem->ssrc, mem->cname));
		session_log(("session_update_member_by_rtcp: Session[%s], %d members\n", ses->self->cname, ses->n_member));
            
		/* callback when the member is valid */
		session_debug(("session_update_member_by_rtcp: ses->$callbacks.member_update@%x(2)\n", (int)ses->$callbacks.member_update));
		

		if(ses->$callbacks.member_update)
			ses->$callbacks.member_update(ses->$callbacks.member_update_user, mem->ssrc, mem->cname, mem->cname_len);

		/* issue media info to members */
		session_issue_mediainfo(ses, ses->self->mediainfo, ses->self->minfo_signum);
   }

   return mem;
}

 /*
  * Collect the sender state for the proposal:
  * - sync the media time
  * - assess the network condition
  */
int session_member_check_senderinfo(member_state_t * sender,
                                    uint32 hi_ntp, uint32 lo_ntp, uint32 rtp_ts,
                                    uint32 packet_sent, uint32 octet_sent)
{
    /* For report lsr: middle 32 bits of NTP */
    sender->lsr_ts = (hi_ntp << 16) + (lo_ntp >> 16);
    
    /* What to do with packet_sent, octet_sent */
	sender->n_rtp_sent = packet_sent;
	sender->n_payload_oct_sent = octet_sent;

    return XRTP_OK;
}


int session_member_check_report(member_state_t *mem, uint8 frac_lost, uint32 intv_lost,
								uint32 full_seqno, uint32 jitter,
								uint32 lsr_stamp, uint32 lsr_delay
							   )
{
    /* Calc network Round-trip Time(RTT) */
    xrtp_session_t *ses = mem->session;
    uint32 rtt = 0;
    /*
    session_debug(("session_member_check_report: Member[%s]\n", mem->cname));
    session_debug(("session_member_check_report: frac_lost[%d]\n", frac_lost));
    session_debug(("session_member_check_report: intv_lost[%d]\n", intv_lost));
    session_debug(("session_member_check_report: full_seqno[%d]\n", full_seqno));
    session_debug(("session_member_check_report: jitter[%d]\n", jitter));
    session_debug(("session_member_check_report: lsr_stamp[%d]\n", lsr_stamp));
    session_debug(("session_member_check_report: lsr_delay[%d]\n", lsr_delay));
    */
	if(mem == mem->session->self)
	{
       /* ntp_now - lsr_stamp - lsr_delay */

       uint32 hi_ntp = 0, lo_ntp = 0, ntpts_now;

       if(ses->$callbacks.ntp)
          ses->$callbacks.ntp(ses->$callbacks.ntp_user, &hi_ntp, &lo_ntp);

       ntpts_now = (hi_ntp << 16) + (lo_ntp >> 16);
       rtt = ntpts_now - lsr_stamp - lsr_delay;
    }
    /* Filter network condition and report to user:
     * Check packet lost and Network Jitter
     * if result over the limitation, report to user
     */
    if(ses->$callbacks.member_report)
	{
       ses->$callbacks.member_report
			(ses->$callbacks.member_report_user, mem->user_info, mem->ssrc, 
			 mem->cname, mem->cname_len, mem->n_rtp_sent, mem->n_payload_oct_sent, 
			 frac_lost, intv_lost, jitter, rtt
			);
	}

    return XRTP_OK;                        
 }

int session_member_make_report(member_state_t *mem, uint8 *frac_lost,
                                uint32 *total_lost, uint32 *exseqno)
{
    /**
     * Calc packet lost, copied from RFC 3550 A.3
     */
    uint32 expected, expected_interval;
    uint32 received_interval;
    
    *exseqno = session_member_max_exseqno(mem);
    expected = *exseqno - mem->base_seq + 1;

    expected_interval = expected - mem->n_rtp_expected_prior;
    mem->n_rtp_expected_prior = expected;
    
    received_interval = mem->n_rtp_received - mem->n_rtp_received_prior;
    mem->n_rtp_received_prior = mem->n_rtp_received;
    
    *total_lost = expected_interval - received_interval;   /* Within a interval */
    
    /* frac_lost is an 8-bit fixed point number with the binary
     * point at the left edge.
     */
    if (expected_interval == 0 || *total_lost <= 0)
      *frac_lost = 0;
    else
      *frac_lost = (*total_lost << 8) / expected_interval;

    return XRTP_OK;
}

/* Set session buffer time of packets in scheduling */
int session_set_buffer(xrtp_session_t *ses, int size, xrtp_hrtime_t timesize)
{
   if(!ses->packets_in_sched)
   {
      ses->packets_in_sched = queue_new(size);
      if(!ses->packets_in_sched)
	  {
         session_log(("< session_set_buffer: No packets queue created ! >\n"));
         return XRTP_EMEM;
      }
   }

   ses->timesize = timesize;

   return XRTP_OK;
}

int session_set_rtp_rate(xrtp_session_t *ses, int rate)
{
   ses->usec_period = 1000000 / rate;

   session_set_buffer(ses, RTP_MAX_PACKET_DELAY, ses->usec_period * RTP_DELAY_FACTOR);

   return XRTP_OK;
}

rtime_t session_rtp_period(xrtp_session_t *ses)
{
	return ses->usec_period;
}      

xrtp_hrtime_t session_rtp_delay(xrtp_session_t *ses)
{
    return ses->usec_period * RTP_DELAY_FACTOR;
}


rtime_t session_rtcp_interval(xrtp_session_t *ses)
{
    rtime_t t;

    t = session_interval(ses->n_member, 
						ses->n_sender,
						ses->bandwidth - ses->bandwidth_rtp, 
						ses->self->we_sent,
						ses->rtcp_avg_size, 
						ses->rtcp_init);
	/*
    session_log(("session_rtcp_interval: ses->n_member = %d\n", ses->n_member));
    session_log(("session_rtcp_interval: ses->n_sender = %d\n", ses->n_sender));
    session_log(("session_rtcp_interval: ses->self->we_sent = %d\n", ses->self->we_sent));
    session_log(("session_rtcp_interval: ses->rtcp_avg_size = %d\n", ses->rtcp_avg_size));
    session_log(("session_rtcp_interval: ses->rtcp_init = %d\n", ses->rtcp_init));
    session_log(("session_rtcp_interval: is %d csec\n", t));
	*/
    ses->msec_rtcp_interval = t;
    
    return t;
}

xrtp_lrtime_t session_rtcp_delay(xrtp_session_t *ses)
{
    return RTCP_MIN_INTERVAL / 2;
}

int session_count_rtcp(xrtp_session_t * ses, xrtp_rtcp_compound_t * rtcp)
{
    if(!ses->n_rtcp_recv)
	{
       ses->rtcp_avg_size = rtcp->bytes_received;
    }
	else
	{
       ses->rtcp_avg_size = (15 * ses->rtcp_avg_size + rtcp->bytes_received)/16;
    }
    
    ses->n_rtcp_recv++;

    return XRTP_OK;
}
 
int session_notify_delay(xrtp_session_t * ses, xrtp_hrtime_t howlate)
{
    if(ses->$callbacks.notify_delay)
       ses->$callbacks.notify_delay(ses->$callbacks.notify_delay_user, ses, howlate);

    return XRTP_OK;
}

int session_done_module(void *gen)

{
    profile_class_t * mod = (profile_class_t *)gen;

    mod->done(mod);

    return OS_OK;
}

int session_new_sdp(module_catalog_t* cata, char* nettype, char* addrtype, char* netaddr, int* default_rtp_portno, int* default_rtcp_portno, int pt, char* mime, int clockrate, int coding_param, int bw_budget, void* control, void* sdp_info)
{
	profile_class_t * modu = NULL;    
	profile_handler_t * prof = NULL;
	xrtp_media_t* media = NULL;
   
	int bw;

	xrtp_list_user_t $lu; 
	xrtp_list_t *list = NULL;

	/* Search a suitable media handling module */
	list = xrtp_list_new();
   
	if (catalog_create_modules(cata, "rtp_profile", list) <= 0)
	{
		xrtp_list_free(list, NULL);
		session_debug(("session_update_sdp: No media module found\n"));
      
		return -1;
	}

	modu = (profile_class_t *)xrtp_list_first(list, &$lu);
	while (modu)
	{
		if (modu->match_id(modu, mime))
		{
			xrtp_list_remove_item(list, modu);
			break;
		}
      
		modu = (profile_class_t *)xrtp_list_next(list, &$lu);
	}

	xrtp_list_free(list, session_done_module);
   
	if(!modu)
	{
		session_debug(("session_new_sdp: No '%s' module found\n", mime));
		return -1;
	}
    
	prof = modu->new_handler(modu, NULL);
	if(!prof)
	{
		session_debug(("session_new_sdp: Fail to create profile handler\n"));
		modu->done(modu);
		return -1;
	}

	media = prof->media(prof, clockrate, coding_param);


	/* * *
    * How to determine default portno for media type
	 * Need a default portmap table!
	 */
	bw = media->new_sdp(media, nettype, addrtype, netaddr, default_rtp_portno, default_rtcp_portno, pt, clockrate, coding_param, bw_budget, control, sdp_info, NULL);
		
	modu->done_handler(prof);
	modu->done(modu);

	return bw;
}


int session_mediainfo_sdp(xrtp_session_t* ses, char* nettype, char* addrtype, char* netaddr, int* default_rtp_portno, int* default_rtcp_portno, char* mime, int bw_budget, void* control, void* sdp_info, void* mediainfo)
{
	xrtp_media_t* media = NULL;
	int bw;
    
	media = session_media(ses);
 
	/* How to determine default portno for media type
	 * Need a default portmap table!
	 */
    bw = media->new_sdp(media, nettype, addrtype, netaddr, default_rtp_portno, default_rtcp_portno, ses->$state.profile_no, 0, 0, bw_budget, control, sdp_info, mediainfo);

    return bw;
}

xrtp_media_t * session_new_media(xrtp_session_t * ses, uint8 profile_no, char *profile_type, int clockrate, int coding_param)
{
	profile_class_t * modu = NULL;    
	profile_handler_t * med = NULL;

	xrtp_list_user_t $lu; 
	xrtp_list_t *list = NULL;

	int rtp_portno=0, rtcp_portno=0;
   
	if(ses->media)
	{
		if(!ses->rtp_port && !ses->rtcp_port)
		{
			ses->media->parameter(ses->media, "rtp_portno", &rtp_portno);

			ses->media->parameter(ses->media, "rtcp_portno", &rtcp_portno);

			session_log(("session_new_media: rtp[#%d]/rtcp[#%d] for [%s]", rtp_portno, rtcp_portno, profile_type));
			/* 
			if(rtp_portno == 0)
			{
				rtp_portno = ses->default_rtp_portno;
				rtcp_portno = ses->default_rtcp_portno;
			}
			*/
			ses->rtp_port = port_new(ses->ip, (uint16)rtp_portno, RTP_PORT);
			ses->rtcp_port = port_new(ses->ip, (uint16)rtcp_portno, RTCP_PORT);
    
			port_set_session(ses->rtp_port, ses);
			port_set_session(ses->rtcp_port, ses);
		
			session_log(("session_new_media: portno rtp[%d]rtcp[%d]\n", rtp_portno, rtcp_portno));
		}

		if(!profile_type || ses->media->match_mime(ses->media, profile_type))  /* Same module or new media mime is none */
		{
			ses->$state.profile_no = profile_no;
			return ses->media;
		}
		else
		{
			/* restore bandwidth per second */
			ses->bandwidth_budget = ses->bandwidth;
			ses->bandwidth_rtp_budget = ses->bandwidth_rtp;
		}
	}

	/* Search a suitable media handling module */
	list = xrtp_list_new();
   
	if (catalog_create_modules(ses->module_catalog, "rtp_profile", list) <= 0)
	{
		xrtp_list_free(list, NULL);
		session_log(("session_new_media: No media module found\n"));
      
		return NULL;
	}

	modu = (profile_class_t *)xrtp_list_first(list, &$lu);
	while (modu)
	{
		if (modu->match_id(modu, profile_type))
		{
			xrtp_list_remove_item(list, modu);
			break;
		}
      
		modu = (profile_class_t *)xrtp_list_next(list, &$lu);
	}

	xrtp_list_free(list, session_done_module);
   
	if(!modu)
	{
		session_log(("session_new_media: No '%s' module found\n", profile_type));
		return NULL;
	}
    
	med = modu->new_handler(modu, ses);
	if(!med)
	{
		modu->done(modu);
		return NULL;
	}

	if(ses->mode_trans == SESSION_SEND || ses->mode_trans == SESSION_DUPLEX)
	{
		if(!ses->media)
		{
			pipe_add(ses->rtp_send_pipe, med, XRTP_ENABLE);
			pipe_add(ses->rtcp_send_pipe, med, XRTP_ENABLE);
		}

		else if(!pipe_replace(ses->rtp_send_pipe, profile_type, med))
		{
			modu->done_handler(med);
			modu->done(modu);
			return NULL;
		}
	}
	
	if(ses->mode_trans == SESSION_RECV || ses->mode_trans == SESSION_DUPLEX)
	{
		if(!ses->media)
		{
			pipe_add(ses->rtp_recv_pipe, med, XRTP_ENABLE);
			pipe_add(ses->rtcp_recv_pipe, med, XRTP_ENABLE);
		}
		else if(!pipe_replace(ses->rtp_recv_pipe, profile_type, med))
		{
			return NULL;
		}
	}

	if(ses->media) 
		ses->media->done(ses->media);
    
	ses->media = med->media(med, clockrate, coding_param);

	if(!ses->rtp_port && !ses->rtcp_port)
	{
		ses->media->parameter(ses->media, "rtp_portno", &rtp_portno);
		ses->media->parameter(ses->media, "rtcp_portno", &rtcp_portno);

		session_log(("session_new_media: [%s] - rtp[#%d] rtcp[#%d]\n", profile_type, rtp_portno, rtcp_portno));
		session_log(("session_new_media: NOTE! New port pair allocation implement later\n"));

        ses->rtp_port = port_new(ses->ip, (uint16)rtp_portno, RTP_PORT);
		ses->rtcp_port = port_new(ses->ip, (uint16)rtcp_portno, RTCP_PORT);
    
		if(!ses->rtp_port)
		{
			session_debug(("session_new_media: RTP port[%d] not available\n", rtp_portno));
		}
		else if(!ses->rtcp_port)
		{
			session_debug(("session_new_media: RTCP port[%d] not available\n", rtcp_portno));
		}
		else
		{
			port_set_session(ses->rtp_port, ses);
			port_set_session(ses->rtcp_port, ses);
		}
	}

	ses->bandwidth_rtp = ses->media->bps / OS_BYTE_BITS;
	ses->bandwidth = (int)((2 * ses->bandwidth_rtp) * 1.05);
	
	if(ses->bandwidth != 0)
	{
		ses->bandwidth_budget = ses->bandwidth;
		ses->bandwidth_rtp_budget = ses->bandwidth_rtp;

		ses->bandwidth_rtcp_budget = ses->bandwidth - ses->bandwidth_rtp;
	}
 
	if(!ses->self->ssrc)
		session_make_ssrc(ses);
    
	ses->$state.profile_type = xstr_clone(profile_type);

	/* verify the payload type */
	ses->$state.profile_no = profile_no;

	return ses->media;
}

  
xrtp_media_t*
session_media(xrtp_session_t *ses)
{
   return ses->media;
}

/**
 * The middle process b/w import and export, order in addition
 * Can be compress module and crypo module, number of module less than MAX_PIPE_STEP

 * and be can enabled/disabled.
 */
profile_handler_t * session_add_handler(xrtp_session_t *ses, char *id)
{
   int mtype = 0;
   profile_handler_t * hand = NULL;
   profile_class_t * modu = NULL;
   
   /* Search a suitable media handling module */
   xrtp_list_user_t $lu;
   xrtp_list_t *list = xrtp_list_new();

   if (catalog_create_modules(ses->module_catalog, "rtp_profile", list) <= 0)
   {
      xrtp_list_free(list, NULL);
      session_log(("session_add_handler: No media module found\n"));

      return NULL;
   }

   modu = (profile_class_t *)xrtp_list_first(list, &$lu);
   while (modu)
   {
      if (modu->match_id(modu, id))
      {

         xrtp_list_remove_item(list, modu);
         break;
      }

      modu = (profile_class_t *)xrtp_list_next(list, &$lu);
   }


   xrtp_list_free(list, session_done_module);

   if(!modu)
   {
      session_log(("session_add_handler: No '%s' module found\n", id));
      return NULL;
   }

   session_log(("session_add_handler: '%s' module created.\n", id));
   
   mtype = modu->type(modu);
   if(mtype == XRTP_HANDLER_TYPE_MEDIA)
   {
      if(ses->media)
      {
         modu->done(modu);
         return ses->media_handler;
      }
   }

   hand = modu->new_handler(modu, ses);
   if(!hand)
   {
      session_log(("< session_add_handler: Fail to create '%s' handler. >\n", id));
      return NULL;
   }

   session_log(("session_add_handler: '%s' handler created.\n", id));
   if(mtype == XRTP_HANDLER_TYPE_MEDIA)
   {
		session_log(("session_add_handler: call session_new_media() insteed to add media handler\n"));

		return NULL;
   }

   pipe_add(ses->rtp_send_pipe, hand, XRTP_ENABLE);
   session_log(("session_add_handler: '%s' handler added to the rtp send pipe.\n", id));

   pipe_add(ses->rtcp_send_pipe, hand, XRTP_ENABLE);
   session_log(("session_add_handler: '%s' handler added to the rtcp send pipes.\n", id));

   pipe_add_before(ses->rtp_recv_pipe, hand, XRTP_ENABLE);
   session_log(("session_add_handler: '%s' handler added to the rtp recv pipes.\n", id));
   pipe_add_before(ses->rtcp_recv_pipe, hand, XRTP_ENABLE);
   session_log(("session_add_handler: '%s' handler added to the rtcp recv pipes.\n", id));
    
   return hand;
}

 /**
  * Get the packet process of the session
  * param type:

  *   RTP_SEND
  *   RTCP_SEND
  *   RTP_RECEIVE
  *   RTCP_RECEIVE
  */
 packet_pipe_t * session_process(xrtp_session_t * session, int type){

    switch(type){

       case RTP_SEND : return session->rtp_send_pipe;
       case RTCP_SEND : return session->rtcp_send_pipe;
       case RTP_RECEIVE : return session->rtp_recv_pipe;
       case RTCP_RECEIVE : return session->rtcp_recv_pipe;
    }

    return NULL;
 }

 int session_set_callback(xrtp_session_t *ses, int type, void* call, void* user){

    switch(type){
       
       case(CALLBACK_SESSION_MEDIA_SENT):
           ses->$callbacks.media_sent = call;
           ses->$callbacks.media_sent_user = user;
           session_log(("session_set_callback: 'media_sent' callback added\n"));
           break;

       case(CALLBACK_SESSION_MEDIA_RECVD):
           ses->$callbacks.media_recieved = call;
           ses->$callbacks.media_recieved_user = user;
           session_log(("session_set_callback: 'media_received' callback added\n"));
           break;

       case(CALLBACK_SESSION_APPINFO_RECVD):
           ses->$callbacks.appinfo_recieved = call;
           ses->$callbacks.appinfo_recieved_user = user;
           session_log(("session_set_callback: 'appinfo_recieved' callback added\n"));


           break;

       case(CALLBACK_SESSION_MEMBER_APPLY):
           ses->$callbacks.member_apply = call;
           ses->$callbacks.member_apply_user = user;
           session_log(("session_set_callback: 'member_apply' callback added\n"));
           break;

       case(CALLBACK_SESSION_MEMBER_UPDATE):
           ses->$callbacks.member_update = call;
           ses->$callbacks.member_update_user = user;
           session_log(("session_set_callback: 'member_update' callback added\n"));

           break;

       case(CALLBACK_SESSION_MEMBER_CONNECTS):
           ses->$callbacks.member_connects = call;
           ses->$callbacks.member_connects_user = user;
           session_log(("session_set_callback: 'member_connects' callback added\n"));
           break;


       case(CALLBACK_SESSION_MEMBER_DELETED):
           ses->$callbacks.member_deleted = call;
           ses->$callbacks.member_deleted_user = user;
           session_log(("session_set_callback: 'member_deleted' callback added\n"));
           break;

       case(CALLBACK_SESSION_MEMBER_REPORT):
           ses->$callbacks.member_report = call;
           ses->$callbacks.member_report_user = user;
           session_log(("session_set_callback: 'member_report' callback added\n"));
           break;

       case(CALLBACK_SESSION_BEFORE_RESET):
           ses->$callbacks.before_reset = call;
           ses->$callbacks.before_reset_user = user;
           session_log(("session_set_callback: 'before_reset' callback added\n"));
           break;

       case(CALLBACK_SESSION_AFTER_RESET):
           ses->$callbacks.after_reset = call;
           ses->$callbacks.after_reset_user = user;
           session_log(("session_set_callback: 'after_reset' callback added\n"));
           break;

       case(CALLBACK_SESSION_NEED_SIGNATURE):

           ses->$callbacks.need_signature = call;
           ses->$callbacks.need_signature_user = user;
           session_log(("session_set_callback: 'need_signature' callback added\n"));
           break;

       case(CALLBACK_SESSION_NOTIFY_DELAY):
           ses->$callbacks.notify_delay = call;
           ses->$callbacks.notify_delay_user = user;
           session_log(("session_set_callback: 'notify_delay' callback added\n"));
           break;


       case(CALLBACK_SESSION_NTP):
           ses->$callbacks.ntp = call;
           ses->$callbacks.ntp_user = user;
           session_log(("session_set_callback: 'ntp' callback added\n"));
           break;


       case(CALLBACK_SESSION_SYNC):
           ses->$callbacks.synchronise = call;
           ses->$callbacks.synchronise_user = user;
           session_log(("session_set_callback: 'synchronise' callback added\n"));
           break;

       default:
           session_debug(("session_set_callback: callback[%d] unsupported\n", type));
           return XRTP_EUNSUP;
    }

    return XRTP_OK;
 }

int session_rtp_outgoing(xrtp_session_t * ses, xrtp_rtp_packet_t *rtp, rtime_t usec_deadline)
{

    xrtp_buffer_t * buf= NULL;
    char * data = NULL;
    uint datalen = 0;

    session_connect_t * conn = NULL;
    /* for log purpose
    xclock_t *ses_clock = session_clock(ses);
    */
    buf= rtp_packet_buffer(rtp);
    data = buffer_data(buf);
    datalen = buffer_datalen(buf);

    if(port_is_multicast(ses->rtp_port))
	{
        session_log(("session_rtp_outgoing: Multicasting is not implemented yet\n"));
    }
	else
	{
        struct xrtp_list_user_s ul;
        member_state_t * mem = NULL;
        int nsent = 0;
        
        mem = xrtp_list_first(ses->members, &ul);

        while(mem)
		{
            if(mem != ses->self && mem->valid)
			{

                /*
                rtime_t us_now = time_usec_now(ses_clock);

                */
				conn = mem->rtp_connect;

                connect_send(conn, data, datalen);
                nsent++;

                /*
                session_log(("session_rtp_outgoing: due in %dus, [%s]=========RTP=>>>>>\n", usec_deadline - us_now, ses->self->cname));
                */
            }


            mem = xrtp_list_next(ses->members, &ul);
        }

      if(ses->join_to_rtp_port)
		{
         session_log(("session_rtp_outgoing: Session[%d] has a join desire[%u] pending\n", session_id(ses), (int)(ses->join_to_rtp_port)));
            
			conn = connect_new(ses->rtcp_port, ses->join_to_rtp_port);
         if(conn)
			{
                session_log(("session_rtp_outgoing: send to joining connect\n"));

                connect_send(conn, data, datalen);
                connect_done(conn);
         }
			else
            session_log(("session_rtp_outgoing: Fail to conect\n"));
      }
   }

   ses->self->max_seq++;

   return XRTP_OK;
}


/* bandwidth stuff */
int session_member_bandwidth(xrtp_session_t * ses)
{
    if(port_is_multicast(ses->rtp_port) || ses->n_member <= 1)
        return ses->bandwidth_rtp;
        
	return ses->bandwidth_rtp / (ses->n_member-1);   /* minus self */
}





int32 session_receiver_rtp_bw_left(xrtp_session_t * ses)
{
    if(port_is_multicast(ses->rtp_port) || ses->n_member-1 == 0)
        return ses->bandwidth_rtp_budget;
    else
        return ses->bandwidth_rtp_budget / (ses->n_member-1);   /* minus self */
}

int session_set_member_bandwidth(xrtp_session_t * ses, int left)
{
    if(port_is_multicast(ses->rtp_port) || ses->n_member-1 == 0)
	{
        ses->bandwidth_rtp_budget = left;
	}
    else
	{
        ses->bandwidth_rtp_budget = left * (ses->n_member-1);   /* minus self */
	}

    session_log(("session_set_rtp_bw_left: %d members in the session, %d bytes bw in total\n", ses->n_member, ses->bandwidth_rtp_budget));
    
	return XRTP_OK;
}

/* book bandwidth for packet, if not enough return -1, otherwise return bandwith left */
int session_book_bandwidth(xrtp_session_t *ses, int bytes)
{
    rtime_t ms_bw;

	int per_bw; /* per member excluse self */

    ms_bw = time_msec_checkpass(ses->clock, &ses->self->msec_last_rtp_sent);

	xthr_lock(ses->bandwidth_lock);

	if(ms_bw > BUDGET_PERIOD_MSEC)
	{
		ses->bandwidth_budget = ses->bandwidth;
		ses->bandwidth_rtp_budget = ses->bandwidth_rtp;
	}
	else
	{
		ses->bandwidth_budget += ses->bandwidth * ms_bw / 1000;
		ses->bandwidth_rtp_budget += ses->bandwidth_rtp * ms_bw / 1000;
	}
		
	per_bw = session_member_bandwidth(ses);
        
	if(per_bw < bytes)
	{
		xthr_unlock(ses->bandwidth_lock);
		return -1;
	}

	per_bw -= bytes;

	session_set_member_bandwidth(ses, per_bw);

	xthr_unlock(ses->bandwidth_lock);

	return per_bw;
}

/**
 * Send number of empty rtp packet to out/in pipe for containing media unit.
 */
int session_rtp_to_send(xrtp_session_t *ses, rtime_t usec_deadline, int last_packet)
{
    xrtp_rtp_packet_t * rtp;
    int packet_bytes;
    int pump_ret = 0;

    /* Check time */
	rtime_t ms_now;
    rtime_t us_now;
	rtime_t ns_now;
	
	xclock_t *ses_clock;

    if(queue_is_full(ses->packets_in_sched))
	{
       session_debug(("session_rtp_to_send: Too many packets waiting for sending\n"));
       /* FIXME: May callback to notify the applicaton */
       return XRTP_EBUSY;
    }

	/*
	 session_log(("\nsession_rtp_to_send: [%s] +++++++++++++RTP++++++++++++\n", ses->self->cname));
	 */

    /* FIXME: May consider time buffer full condition */

    /* FIXME: May race condition with rtcp, need sychronization */
    if(ses->self->ssrc == 0)
	 {
       if(ses->$callbacks.need_signature)
	    {
          ses->self->ssrc = ses->$callbacks.need_signature(ses->$callbacks.need_signature_user, ses);
       }
	    else
	    {
          session_make_ssrc(ses);
       }
    
	    session_log(("session_rtp_to_send: ssrc[%d]\n", ses->self->ssrc));
    }

    /* Test Time Atom
    ts_pass = time_passed(ses->clock, ts_start);
    session_log("session_rtp_to_send: time atom is %d\n", ts_pass);
    */     
    rtp = rtp_new_packet(ses, ses->$state.profile_no, RTP_SEND, NULL, 0, 0, 0);

    if(!rtp)
	 {
       session_debug(("session_rtp_to_send: Can't create rtp packet for sending\n"));
       return XRTP_EMEM;
    }

    rtp_packet_set_head(rtp, ses->self->ssrc, ses->self->max_seq);

    pump_ret = pipe_pump(ses->rtp_send_pipe, usec_deadline, rtp, &packet_bytes);
    if(pump_ret == XRTP_ETIMEOUT)
	 {
       session_debug(("session_rtp_to_send: timeout, cancelled sending\n"));
       rtp_packet_done(rtp);

       return pump_ret;
    }                             

    if(pump_ret < XRTP_OK)

	{
       session_debug(("session_rtp_to_send: Fail to produce packet\n"));
       rtp_packet_done(rtp);

       return pump_ret;
    }

	if(!session_book_bandwidth(ses, packet_bytes))
	{
       session_debug(("session_rtp_to_send: discard due to short bandwidth\n"));

       rtp_packet_done(rtp);
	   exit(1);

       return XRTP_FAIL;
	}

	session_rtp_outgoing(ses, rtp, usec_deadline);

	ses_clock = session_clock(ses);

	time_rightnow(ses_clock, &ms_now, &us_now, &ns_now);

    if(ses->$state.n_pack_sent == 0)
	{
		ses->$state.usec_start_send = us_now;
		ses->$state.lrts_start_send = ms_now;
    }
   
	ses->self->msec_last_rtp_sent = ms_now;
	ses->self->usec_last_rtp_sent = us_now;
	ses->$state.n_pack_sent++;
	/*
	session_debug(("\nsession_rtp_to_send: last_rtp_sent at [%dms][%dus]@%d\n\n", ms_now, ns_now, (int)ses_clock));
	*/
	rtp_packet_done(rtp);

    return XRTP_OK;
}
  
int session_cancel_rtp_sending(xrtp_session_t *ses, xrtp_hrtime_t ts)
{
    pipe_discard(ses->rtp_send_pipe, ts);

    ses->sched->rtp_out(ses->sched, ses, ts, 1);
    
    return XRTP_OK;
}

#if 0

/* Return n bytes sent */
//int session_rtp_send_now(xrtp_session_t *ses)
//{
//    int nbytes = 0;

//    int bw_per_recvr = 0;   //bandwidth per member to receive media per period
//    int packet_bytes = 0;

//    rtime_t us_now = time_usec_now(ses->clock);

//    xrtp_rtp_packet_t *rtp = (xrtp_rtp_packet_t *)queue_head(ses->packets_in_sched);

    // Get next packet info

//    rtp_packet_info(rtp, &packet_bytes);

    // Calculate current bandwidth

//    if(ses->$state.n_pack_sent)
//	{
//        int rtp_bw = session_member_bandwidth(ses);
        
//        time_spent(ses->clock, ses->self->msec_last_rtp_sent, ses->self->usec_last_rtp_sent, 0, &lrt_passed, &us_passed, &ns_dummy);
        
//        bw_per_recvr += (int)(rtp_bw * lrt_passed / ((double)ses->usec_period / 1000));
//        bw_per_recvr += rtp_bw * us_passed / ses->usec_period;

        
        /* Use some bandwidth left */
//        bw_per_recvr += session_receiver_rtp_bw_left(ses);

//        session_log(("session_rtp_send_now: rtp packet[%dB], bandwidth[%dB]\n", rtp_packet_bytes(rtp), bw_per_recvr));
//   }
//	else
//	{
//        time_spent(ses->clock, ses->$state.lrts_start_send, ses->$state.usec_start_send, 0, &lrt_passed, &us_passed, &ns_dummy);

//        bw_per_recvr = session_member_bandwidth(ses);    /* minus self */
        
//        bw_per_recvr = bw_per_recvr + session_member_bandwidth(ses) * (1 + us_passed / ses->usec_period);

//        session_log(("session_rtp_send_now: initial rtp bandwidth is %d bytes\n", bw_per_recvr));
//    }

//    while( rtp && bw_per_recvr >= packet_bytes)
//    {
//       queue_serve(ses->packets_in_sched);

//       session_rtp_outgoing(ses, rtp);
//       rtp_packet_done(rtp);

//       nbytes += rtp_packet_payload_bytes(rtp);

//       ses->$state.n_pack_sent++;
//       bw_per_recvr -= packet_bytes;


//       session_log(("session_rtp_send_now: rtp bandwidth %d bytes available\n", bw_per_recvr));

//       rtp = (xrtp_rtp_packet_t *)queue_head(ses->packets_in_sched);
//       if(!rtp)

//          break;

//       rtp_packet_info(rtp, &packet_bytes);

//       session_log(("session_rtp_send_now: rtp bandwidth %d bytes available for next packet with %d bytes\n", bw_per_recvr, packet_bytes));
//    }
    
//    session_set_member_bandwidth(ses, bw_per_recvr);
    
//    if(rtp && bw_per_recvr < packet_bytes)
//	 {
       /* FIXME: Consider delay resynchronization */
//      ses->sched->rtp_out(ses->sched, ses, ses->usec_period, 0); 
//    }
    
//    ses->self->msec_last_rtp_sent = ms_now;
//    ses->self->usec_last_rtp_sent = us_now;
    
//    ses->$state.oct_sent += nbytes;
    
//    return nbytes;
// }

#endif

int session_rtp_to_receive(xrtp_session_t *ses)
{
    xrtp_rtp_packet_t * rtp;

    char *pdata = NULL;
    int datalen;
	int packet_header_bytes;

    xrtp_buffer_t * buf;
    
    rtime_t ms_arrival;
    rtime_t us_arrival;
    rtime_t ns_arrival;


    int packet_bytes;
	session_connect_t *in;



	/*
    session_debug(("session_rtp_to_receive: [%s] <<<<<=RTP============\n", ses->self->cname));
    */

	/* retrieve incoming */
	xthr_lock(ses->rtp_incoming_lock);

	in = ses->rtp_incoming;
    ses->rtp_incoming = NULL;

	xthr_unlock(ses->rtp_incoming_lock);

	if(in == NULL)
	{
       session_log(("session_rtp_to_receive: incoming packet lost\n"));

       return XRTP_FAIL;


	}

	datalen = connect_receive(in, &pdata, &packet_header_bytes, &ms_arrival, &us_arrival, &ns_arrival);

    /* New RTP Packet */
    rtp = rtp_new_packet(ses, ses->$state.profile_no, RTP_RECEIVE, in, ms_arrival, us_arrival, ns_arrival);
    if(!rtp)
	{
       session_debug(("session_rtp_to_receive: Can't create rtp packet for receiving\n"));
       return XRTP_EMEM;
    }

    buf = buffer_new(0, NET_BYTEORDER);
    buffer_mount(buf, pdata, datalen);
    
    rtp_packet_set_buffer(rtp, buf);
    
    if(pipe_pump(ses->rtp_recv_pipe, 0, rtp, &packet_bytes) != XRTP_CONSUMED)
    {
        //session_log(("session_rtp_to_receive: [%s] ######======RTP=====######\n\n", ses->self->cname));
        rtp_packet_done(rtp);
    }
    else
    {
        //session_log(("session_rtp_to_receive: [%s] xxxxx=====RTP====xxxxxx\n\n", ses->self->cname));
    }
    

    return XRTP_OK;
}
 
/* Seperate thread for rtp receiving */
int session_rtp_recv_loop(void * gen)
{
    xrtp_session_t * ses = (xrtp_session_t *)gen;


    ses->thread_run = 1;

    session_debug(("session_rtp_recv_loop: [%s] start reception\n", ses->self->cname));
    do
	{
        if(port_poll(ses->rtp_port, USEC_POLLING_TIMEOUT) <= 0)
		{
			/* Should be ONE actually if got incoming */
            /* FIXME: Maybe a little condition racing */
            if(ses->thread_run == 0)
			{
                session_log(("session_rtp_recv_loop: quit loop.\n"));
                break;
            }

            continue;
        }

        xthr_lock(ses->rtp_recv_lock);

        if(ses->rtp_incoming)
            session_rtp_to_receive(ses);

        xthr_unlock(ses->rtp_recv_lock);


    }while(1);

	ses->thr_rtp_recv = NULL;

    return XRTP_OK;
}

int session_start_reception(xrtp_session_t * ses)
{
    if(ses->thr_rtp_recv == NULL)
	{
		ses->rtp_recv_lock = xthr_new_lock();
		if(ses->rtp_recv_lock == NULL)
		{
			session_debug(("session_start: fail to create lock\n"));
			return XRTP_FAIL;
		}


		ses->thr_rtp_recv = xthr_new(session_rtp_recv_loop, ses, XTHREAD_NONEFLAGS);
		if(ses->thr_rtp_recv == NULL)
		{
			session_debug(("session_start: fail to create thread\n"));
			return XRTP_FAIL;
		}
	}

    return XRTP_OK;
}
 
 int session_stop_reception(xrtp_session_t * ses)
 {
    int th_ret;
    ses->thread_run = 0;
    
    xthr_wait(ses->thr_rtp_recv, &th_ret);
    ses->thr_rtp_recv = NULL;
    
    xthr_done_lock(ses->rtp_recv_lock);
    ses->rtp_recv_lock = NULL;

    session_log(("session_start: Session[%d] receives no more media\n", session_id(ses)));
    return XRTP_OK;
 }

 int session_cancel_rtp_receiving(xrtp_session_t *ses, xrtp_hrtime_t ts){

    pipe_discard(ses->rtp_recv_pipe, ts);

    if(ts > 0 && ts < HRTIME_MAXNSEC){
      
      ses->rtp_cancelled = 1;
      ses->rtp_cancel_ts = ts;
      
    }else{

      ses->rtp_cancelled = 0;
    }
    
    return XRTP_OK;
 }

 int session_rtp_receiving_cancelled(xrtp_session_t * ses, xrtp_hrtime_t ts){

    if(!ses->rtp_cancelled) return 0;



    return TIME_NEWER(ses->rtp_cancel_ts, ts);
 }

 int session_member_timeout(member_state_t * mem)
 {
    rtime_t rtcp_tpass, rtp_tpass;

    xrtp_session_t * ses = mem->session;

    rtcp_tpass = ses->tc - mem->lrt_last_rtcp_sent;

    if(mem->we_sent)
    {
        rtp_tpass = ses->tc - mem->msec_last_rtp_sent;

        if(rtp_tpass > ses->msec_rtcp_interval * RTCP_SENDER_TIMEOUT_MULTIPLIER)

        {
            mem->we_sent = 0;


            session_delete_sender(ses, mem);
            ses->n_sender--;
        }
    }

    if(mem == ses->self)
         return XRTP_NO;
    
    if(rtcp_tpass > ses->msec_rtcp_interval * RTCP_MEMBER_TIMEOUT_MULTIPLIER)
    {
        return XRTP_YES;
    }

    return XRTP_NO;
 }

 /* In cyclic fashion: If reash the last, the next one is the first one */
 member_state_t * session_next_valid_member_of(xrtp_session_t * ses, uint32 ssrc)
 {
    int next_report_ssrc_stall = 0;
    
    member_state_t * mem = xrtp_list_find(ses->members, &ssrc, session_match_member_src, &(ses->$member_list_block));
    while(mem)
	 {

       mem = xrtp_list_next(ses->members, &(ses->$member_list_block));
       if(!mem)
          mem = xrtp_list_first(ses->members, &(ses->$member_list_block));

	   /* exclude petential visitor without booking by SIP, etc */
	   if(!mem->valid && mem->ssrc != 0)
          continue;

       if(session_member_timeout(mem))
	   {
            session_log(("session_next_valid_member_of: Member['%s'] timeout, to delete\n", mem->cname));

            
            if(ses->next_report_ssrc == mem->ssrc)
                next_report_ssrc_stall = 1;

            session_delete_member(ses, mem);
            ses->n_member--;

            continue;
       }

       if(next_report_ssrc_stall)

	   {
          ses->next_report_ssrc = mem->ssrc;
          next_report_ssrc_stall = 0;
       }
          
       break;
    }
    
    return mem;

 }

 int session_need_rtcp(xrtp_session_t *ses)
 {
	 int nmem;

	 xthr_lock(ses->members_lock);
	 nmem = xlist_size(ses->members);
	 xthr_unlock(ses->members_lock);
    
	 if(nmem > 1 || ses->join_to_rtcp_port)
		 return 1;
    
	 return 0;
 }

 int session_rtcp_send_now(xrtp_session_t * ses, xrtp_rtcp_compound_t * rtcp)
 {
    session_connect_t * conn = NULL;

    xrtp_buffer_t * buf= rtcp_buffer(rtcp);
    char * data = buffer_data(buf);
    uint datalen = buffer_datalen(buf);

    int i;
    for(i=0; i<ses->n_rtcp_out; i++)
	{

       conn = ses->outgoing_rtcp[i];
       connect_send(conn, data, datalen);
    }

    if(ses->join_to_rtcp_port)
	{
       conn = connect_new(ses->rtcp_port, ses->join_to_rtcp_port);
       if(conn)
	   {
          session_log(("session_rtcp_send_now: contacting [%s:%d] ...\n", teleport_name(ses->join_to_rtcp_port), teleport_portno(ses->join_to_rtcp_port)));
          connect_send(conn, data, datalen);
          connect_done(conn);
       }
	   else
	   {
          session_log(("session_rtcp_send_now: Fail to join to petential member.\n"));
       }
    }

	session_log(("session_rtcp_send_now: [%s]----------RTCP----->>>>>>>\n", ses->self->cname));

    return XRTP_OK;
 }

int session_report(xrtp_session_t *ses, xrtp_rtcp_compound_t * rtcp, uint32 timestamp, rtime_t ms_now, rtime_t us_now, rtime_t ns_now, uint32 hi_ntp, uint32 lo_ntp)
{
   uint8 frac_lost; 
   uint32 total_lost;
   uint32 exseqno;
    
   uint32 lsr_delay = 0;

   /* Here is the implementation */
   member_state_t * self = ses->self;
   member_state_t * mem = NULL;

   int rtcp_end = 0;    
   int max_report;

   int ncname;
    
   max_report = XRTP_MAX_REPORTS > ses->n_member ? ses->n_member : XRTP_MAX_REPORTS;

   /* Set sender info */

   if(self->we_sent)
   {
      uint32 hi_ntp = 0, lo_ntp = 0;

      /* SR Sender Info */
      if(ses->$callbacks.ntp)
         ses->$callbacks.ntp(ses->$callbacks.ntp_user, &hi_ntp, &lo_ntp);
      else
         time_ntp(ses->clock, &hi_ntp, &lo_ntp);

      /*
      session_log(("session_report: ======ssrc[%u]=======\n", self->ssrc));
      session_log(("session_report: ntp[%u,%u];[%u]\n", hi_ntp,lo_ntp,timestamp));
      session_log(("session_report: sent[%dp;%dB]\n", ses->$state.n_pack_sent, ses->$state.oct_sent));
	   */
      
      rtcp_set_sender_info(rtcp, self->ssrc, hi_ntp, lo_ntp, timestamp, 
                           ses->$state.n_pack_sent, ses->$state.oct_sent);
   }
   else
   {
	   /*
	   session_log(("session_report: ======ssrc[%u]=======\n", self->ssrc));
	   */
   }

   /* report peers */

   /* Add SDES packet */
   if(self->cname)
   {
      rtcp_add_sdes(rtcp, self->ssrc, RTCP_SDES_CNAME, (uint8)self->cname_len, self->cname);
      rtcp_end_sdes(rtcp, self->ssrc);
   }
    
   /* Bye not implemented */

   /* report of members */
   xthr_lock(ses->members_lock);

   ncname = xlist_size(ses->members);
   	   
   session_log(("session_report: %d participants, self.ssrc[%u]\n", ncname, self->ssrc));

   ses->n_rtcp_out = 0;

   if(ncname > 1)
   {
      /* Add more report if more valid members other than self */

      uint32 start_report_ssrc = 0;
      int report_end = 0;

      if(ses->next_report_ssrc == 0)
      {
         mem = session_next_valid_member_of(ses, self->ssrc);
         ses->next_report_ssrc = mem->ssrc;
      }
      else
      {
         /* !mem MUST NOT happen if member is left, this should avoid
          * FIXME: always update ses->next_report_ssrc whenever member left
          */
         mem = session_member_state(ses, ses->next_report_ssrc);
         if(mem == self)
         {    
            /* Actually, this never happen, as next_report_ssrc never record self */
            session_log(("session_report: skip Self[%u]\n", self->ssrc));
            mem = session_next_valid_member_of(ses, self->ssrc);
         }
      }
        
      start_report_ssrc = ses->next_report_ssrc;
    
      while(!report_end && max_report > 0)
      {
         /* report other participants */

         session_member_make_report(mem, &frac_lost, &total_lost, &exseqno);

         /* Calculate last lsr_delay(sender received delay) */
         if(mem->we_sent)
         {

            lsr_delay = ((us_now - mem->lsr_usec) % 1000000) & 0xFF;
            lsr_delay += ((ms_now - mem->lsr_msec) / 1000) << 16;
         }
         else
         {
            lsr_delay = 0;
         }
		 /*
         session_log(("session_report:-----------------------------\n"));
         session_log(("session_report: ssrc[%u]\n", mem->ssrc));
         session_log(("session_report: frac_lost[%u],total_lost[%u]\n", frac_lost, total_lost));
         session_log(("session_report: exseqno[%u],jitter[%f]\n", exseqno, mem->jitter));
         session_log(("session_report: lsr_ts[%dus],lsr_delay[%dus]\n", mem->lsr_ts, lsr_delay));
         session_log(("session_report:-----------------------------\n"));
		 */
         if(rtcp_add_report(rtcp, mem->ssrc, frac_lost, total_lost,
                          exseqno, (uint32)mem->jitter, mem->lsr_ts, lsr_delay) == XRTP_EREFUSE)
         {
              report_end = 1;
         }
         else
         {
              max_report--;
         }
       
         /* Which connect send to for this member */
		   ses->outgoing_rtcp[ses->n_rtcp_out++] = mem->rtcp_connect;

         mem = session_next_valid_member_of(ses, mem->ssrc);
         if(mem == self)
         {
            mem = session_next_valid_member_of(ses, self->ssrc);
         }

         if(mem->ssrc == start_report_ssrc)
         {
            break;
         }
      }
       
      /* Record next report member */

      ses->next_report_ssrc = mem->ssrc;
   }


   xthr_unlock(ses->members_lock);

   /* Send SDES of member */
   if(!rtcp_end)
   {
      /* Add SDES packet */
   }

   return XRTP_OK;
}
 
 int session_rtcp_to_send(xrtp_session_t *ses)
 {
    member_state_t * self = ses->self;

    xrtp_rtcp_compound_t * rtcp;

    int rtcp_bytes;
    rtime_t ms_pass;

    session_log(("\nsession_rtcp_to_send: [%s] ++++++++++++++RTCP+++++++++++++\n", ses->self->cname));
    
    while(self->ssrc == 0)
    {
       if(ses->$callbacks.need_signature)
       {
          self->ssrc = ses->$callbacks.need_signature(ses->$callbacks.need_signature_user, ses);
       }
       else
       {
          session_make_ssrc(ses);
       }
    }

    ms_pass = time_msec_spent(ses->clock, self->msec_last_rtp_sent);

    if(ms_pass > ses->msec_rtcp_interval * RTCP_SENDER_TIMEOUT_MULTIPLIER || ses->$state.n_pack_sent == 0)
    {
        session_log(("session_rtcp_to_send: receiver, send no rtp within %dms\n", ms_pass));
        rtcp = rtcp_receiver_report_new(ses, self->ssrc, XRTP_YES);
        
		  self->we_sent = 0;
        ses->bandwidth_rtp_budget = 0;

        xlist_remove_item(ses->senders, ses->self);
        ses->n_sender = xlist_size(ses->senders);
    }
    else
    {
        session_log(("session_rtcp_to_send: sender, send rtp within %dms\n", ms_pass));
        rtcp = rtcp_sender_report_new(ses, XRTP_YES); /* need padding */
        
		  self->we_sent = 1;

        xlist_add_once(ses->senders, ses->self);
        ses->n_sender = xlist_size(ses->senders);
    }

	 if(!rtcp)
	 {
		   session_debug(("session_rtcp_to_send: Can't create rtp packet for receiving\n"));
        return XRTP_EMEM;
    }

    if(pipe_pump(ses->rtcp_send_pipe, 0, rtcp, &rtcp_bytes) >= XRTP_OK)
    {
       session_log(("session_rtcp_to_send: %d reports, %d bytes\n", ses->n_rtcp_out, rtcp_bytes));

	    session_rtcp_send_now(ses, rtcp);
       
       ses->rtcp_init = 0;

       rtcp_compound_done(rtcp);
    }
    else      
    {
       session_debug(("session_rtcp_to_send: Fail to produce the rtcp packet\n"));
       rtcp_compound_done(rtcp);
        
       return XRTP_FAIL;
    }

	 session_log(("session_rtcp_to_send: sent\n"));

    return XRTP_OK;
 }

 int session_rtcp_to_receive(xrtp_session_t *ses)
 {
    xrtp_rtcp_compound_t * rtcp;

    char *pdata = NULL;
    int datalen;

    xrtp_buffer_t * buf;


    rtime_t ms_arrival;
    rtime_t us_arrival;
    rtime_t ns_arrival;

    int rtcp_bytes;
	int packet_header_bytes;


    session_log(("session_rtcp_to_receive: [%s] <<<<<<-----RTCP--------\n", ses->self->cname));
    
	datalen = connect_receive(ses->rtcp_incoming, &pdata, &packet_header_bytes, &ms_arrival, &us_arrival, &ns_arrival);

	/* New RTCP Packet, 0 packet means default implementation depends value */
    rtcp = rtcp_new_compound(ses, RTP_RECEIVE, ses->rtcp_incoming, ms_arrival, us_arrival, ns_arrival);
    if(!rtcp)


       return XRTP_EMEM;

     buf = buffer_new(0, NET_BYTEORDER);
     buffer_mount(buf, pdata, datalen);

    rtcp_set_buffer(rtcp, buf);
    rtcp->bytes_received = datalen + packet_header_bytes; /* eg. (UDP+IP) header bytes */

    if(pipe_pump(ses->rtcp_recv_pipe, 0, rtcp, &rtcp_bytes) != XRTP_CONSUMED)
       rtcp_compound_done(rtcp);

    session_log(("session_rtcp_to_receive: [%s] xxxxxxx-----RTCP-----xxxxxxx\n\n", ses->self->cname));

    return XRTP_OK;
 }


/**
 * Need a scheduler to schedule the packet piping according timer
 */
int session_set_scheduler(xrtp_session_t *ses, session_sched_t *sched)
{
   if(ses->sched == sched)
		return XRTP_OK;
       
   if(ses->sched)
		ses->sched->remove(ses->sched, ses);

   ses->sched = sched;
    
   if(sched)

   {
		sched->add(sched, ses);
      
      return xrtp_add_session(ses->set, ses);
   }

	return xrtp_remove_session(ses->set, ses);
}

 session_sched_t * session_scheduler(xrtp_session_t *ses){

    return ses->sched;
 }

 int session_set_schedinfo(xrtp_session_t *ses, sched_schedinfo_t * si){

    ses->schedinfo = si;

    return XRTP_OK;
 }


 sched_schedinfo_t * session_schedinfo(xrtp_session_t *ses){

    return ses->schedinfo;
 }

 /* Notify the session a rtp arrival and schedule the process */
 int session_rtp_arrived(xrtp_session_t * ses, session_connect_t * conn)
 {


    xthr_lock(ses->rtp_incoming_lock);
	ses->rtp_incoming = conn;
    xthr_unlock(ses->rtp_incoming_lock);

    return XRTP_OK;
 }


 /* Notify the session a rtcp arrival and schedule the process */
 int session_rtcp_arrival_notified(xrtp_session_t * ses, session_connect_t * conn, rtime_t msec_arrival)
 {
    ses->rtcp_incoming = conn;

    ses->sched->rtcp_in(ses->sched, ses, msec_arrival);

    return XRTP_OK;
 }
