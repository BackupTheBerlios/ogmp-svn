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
 
 #define MAX_PRODUCE_TIME  (20 * HRTIME_MILLI)
 #define SESSION_DEFAULT_RTCP_NPACKET  0
 
 #define SESSION_LOG
 
 #ifdef SESSION_LOG
   const int session_log = 1;
 #else
   const int session_log = 0;
 #endif
 #define session_log(fmtargs)  do{if(session_log) printf fmtargs;}while(0)
    
 #define RTP_SEQ_MOD 65536
 void session_init_seqno(member_state_t * mem, uint16 seq){

     mem->base_seq = seq;
     mem->max_seq = seq - 1;
     mem->bad_seq = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
     mem->cycles = 0;
     mem->received = 0;
     mem->received_prior = 0;
     mem->expected_prior = 0;

     return;
 }

 /**
  * Update seqno according the seq provided.
  *
  * return value: 1 if the seqno is a valid no, otherwise return 0.
  */
 int session_update_seqno(member_state_t * mem, uint16 seq){
   
     uint16 udelta = seq - mem->max_seq;
     
       const int MAX_DROPOUT = 3000;
       const int MAX_MISORDER = 100;

       /*
        * Source is not valid until MIN_SEQUENTIAL packets with
        * sequential sequence numbers have been received.
        */
       if (mem->probation) {
         
           /* packet is in sequence */
           if (seq == mem->max_seq + 1) {
             
               mem->probation--;
               mem->max_seq = seq;
               if (mem->probation == 0) {
                 
                   session_init_seqno(mem, seq);
                   mem->received++;
                   /* a valid seqno */
                   return 1;
               }
               
           } else {
             
               /* packet is NOT in sequence, reset trying */
               mem->probation = MIN_SEQUENTIAL - 1;
               mem->max_seq = seq;
           }
           
           return 0;
           
       } else if (udelta < MAX_DROPOUT) {
         
           /* in order, with permissible gap */
           if (seq < mem->max_seq) {
               /*
                * Sequence number wrapped - count another 64K cycle.
                */
               mem->cycles++;
           }
           
           mem->max_seq = seq;
           
       } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
         
           /* the sequence number made a very large jump */
           if (seq == mem->bad_seq) {
               /*
                * Two sequential packets -- assume that the other side
                * restarted without telling us so just re-sync
                * (i.e., pretend this was the first packet).
                */
               session_init_seqno(mem, seq);
               
           }else {
             
               mem->bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
               return 0;
           }
           
       } else {
         
           /* duplicate or reordered packet */
       }
       
       mem->received++;
       
       return 1;
 }

 int session_done_member(member_state_t * mem);
 
 /**
  * Create a new Session
  */
 xrtp_session_t * session_new(xrtp_port_t *rtp_port, xrtp_port_t *rtcp_port, char * cname, int clen, module_catalog_t * cata){

    xrtp_session_t * ses = (xrtp_session_t *)malloc(sizeof(struct xrtp_session_s));
    if(!ses){
       
       return NULL;
    }
    
    memset(ses, 0, sizeof(struct xrtp_session_s));

    ses->module_catalog = cata;
    
    char *name = (char *)malloc(clen + 1);
    if(!name){

        session_log(("< session_set_cname: fail to allocate cname memery >\n"));
        free(ses);
        return NULL;
    }

    memcpy(name, cname, clen);
    name[clen] = '\0';

    ses->rtcp_init = 1;
    
    ses->rtp_cancel_ts = HRTIME_INFINITY;
    
    ses->rtp_port = rtp_port;
    port_set_session(rtp_port, ses);
    
    ses->rtcp_port = rtcp_port;
    port_set_session(rtcp_port, ses);
    
    ses->clock = time_begin(0,0);

    ses->members = xrtp_list_new();
    ses->senders = xrtp_list_new();
    if(!ses->members || !ses->senders){

       if(ses->members) xrtp_list_free(ses->members, NULL);
       if(ses->senders) xrtp_list_free(ses->senders, NULL);

       free(name);
       free(ses);
       return NULL;
    }

    /* Add self state struct */
    ses->self = session_new_member(ses, 0, NULL);    /* Need to assign a ssrc */
    if(!ses->self){

       xrtp_list_free(ses->members, NULL);
       xrtp_list_free(ses->senders, NULL);

       free(name);
       free(ses);
       return NULL;      
    }

    ses->rtp_send_pipe = pipe_new(ses, XRTP_RTP, XRTP_SEND);
    ses->rtcp_send_pipe = pipe_new(ses, XRTP_RTCP, XRTP_SEND);
    ses->rtp_recv_pipe = pipe_new(ses, XRTP_RTP, XRTP_RECEIVE);
    ses->rtcp_recv_pipe = pipe_new(ses, XRTP_RTCP, XRTP_RECEIVE);

    if(!ses->rtp_send_pipe || !ses->rtcp_send_pipe || !ses->rtp_recv_pipe || !ses->rtcp_recv_pipe){

       if(ses->rtp_send_pipe) pipe_done(ses->rtp_send_pipe);
       if(ses->rtcp_send_pipe) pipe_done(ses->rtcp_send_pipe);
       if(ses->rtp_recv_pipe) pipe_done(ses->rtp_recv_pipe);
       if(ses->rtcp_recv_pipe) pipe_done(ses->rtcp_recv_pipe);
       
       xrtp_list_free(ses->senders, NULL);
       xrtp_list_free(ses->members, NULL);

       session_done_member(ses->self);
       free(name);
       free(ses);
       session_log(("< session_new: Fail to create pipes! >\n"));

       return NULL;
    }

    ses->self->cname = name;
    ses->self->cname_len = clen;
    
    session_log(("session_new: cname is '%s', validated\n", ses->self->cname));
    
    session_init_seqno(ses->self, (uint16)(65535.0*rand()/RAND_MAX));
    ses->self->valid = XRTP_YES;
    ses->n_member = 1;
    
    /* The initial value in range of 32bits, less than 64bits, but good enough */
    ses->timestamp = rand();
    session_log(("session_new: timestamp start at [%d]\n", ses->timestamp));

    return ses;
 }

 uint32 session_member_max_exseqno(member_state_t * mem){

    return mem->max_seq + (65536 * mem->cycles);
 }

 int _session_free_rtp(void * gen){

    xrtp_rtp_packet_t * rtp = (xrtp_rtp_packet_t *)gen;

    rtp_packet_done(rtp);

    return XRTP_OK;
 }

 int _session_free_member(void * gen){

    member_state_t * mem = (member_state_t *)gen;

    xrtp_list_free(mem->rtp_buffer, _session_free_rtp);
    free(mem);

    return XRTP_OK;
 } 
 /**
  * Release the Session
  */
 int session_done(xrtp_session_t * ses){

    pipe_done(ses->rtp_send_pipe);
    pipe_done(ses->rtcp_send_pipe);
    pipe_done(ses->rtp_recv_pipe);
    pipe_done(ses->rtcp_recv_pipe);

    queue_done(ses->packets_in_sched);
    xrtp_list_free(ses->senders, NULL);
    xrtp_list_free(ses->members, _session_free_member);
    free(ses);

    return XRTP_OK;
 }

 int session_set_id(xrtp_session_t * ses, int id){

    ses->id = id;

    return XRTP_OK;
 }

 int session_id(xrtp_session_t * ses){

    return ses->id;
 }

 int session_cname(xrtp_session_t * ses, char * cname, int clen){

    if(!cname || !clen){

        session_log(("< session_set_cname: invalid parameters >\n"));
        return XRTP_EPARAM;
    }

    int n = ses->self->cname_len < clen ? ses->self->cname_len : clen;
    memcpy(cname, ses->self->cname, n);

    return n;
 }

 int session_allow_anonymous(xrtp_session_t * ses, int allow){

    ses->$state.receive_from_anonymous = allow;
    
    return XRTP_OK;
 }

 /**
  * Get session rtp and rtcp ports
  */
 int session_ports(xrtp_session_t * ses, xrtp_port_t **r_rtp, xrtp_port_t **r_rtcp){

    *r_rtp = ses->rtp_port;
    *r_rtcp = ses->rtcp_port;

    return XRTP_OK;
 }

 int session_set_bandwidth(xrtp_session_t * ses, int total_bw, int rtp_bw){

    if(ses->media == NULL){

        ses->bandwidth = total_bw;
        ses->rtp_bw = rtp_bw;
        ses->rtcp_bw = ses->bandwidth - rtp_bw;
        return XRTP_OK;
    }

    ses->bandwidth = total_bw / ses->media->clockrate;
    ses->rtp_bw = rtp_bw / ses->media->clockrate;
    ses->rtcp_bw = ses->bandwidth - ses->rtp_bw;
    
    return XRTP_OK;
 }

 /*
 int session_set_rtp_bandwidth(xrtp_session_t * ses, int bw){

    if(bw > ses->bandwidth) return XRTP_INVALID;
    
    if(ses->media == NULL){
      
        ses->rtp_bw = bw;
        ses->rtcp_bw = ses->bandwidth - bw;
        return XRTP_OK;
    }

    ses->rtp_bw = bw / ses->media->clockrate;
    ses->rtcp_bw = ses->bandwidth - bw;
    return XRTP_OK;
 }*/

 uint32 session_rtp_bandwidth(xrtp_session_t * ses){

    return ses->rtp_bw;
 }
 
 /* Refer RFC-3550 A.7 */
 double session_determistic_interval(int members,
                                     int senders,
                                     double rtcp_bw,
                                     int we_sent,
                                     double avg_rtcp_size,
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
       double const RTCP_MIN_TIME = 5.;
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
       if (initial) {
           rtcp_min_time /= 2;
       }
       
       /*
        * Dedicate a fraction of the RTCP bandwidth to senders unless
        * the number of senders is large enough that their share is
        * more than that fraction.
        */
       n = members;
       if (senders <= members * RTCP_SENDER_BW_FRACTION) {
           if (we_sent) {
               rtcp_bw *= RTCP_SENDER_BW_FRACTION;
               n = senders;
           } else {
               rtcp_bw *= RTCP_RCVR_BW_FRACTION;
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
       t = avg_rtcp_size * n / rtcp_bw;
       if (t < rtcp_min_time) t = rtcp_min_time;

       return t;
 }
 
 /* RFC 3550 A.7 */
 xrtp_lrtime_t session_interval(int members,
                                int senders,
                                double rtcp_bw,
                                int we_sent,
                                double avg_rtcp_size,
                                int initial){

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
       drand48();
       t = t * (drand48() + 0.5);
       t = t / COMPENSATION;

       return (xrtp_lrtime_t)(t*1000);  /* Convert to millisec unit */
 }

 int session_join(xrtp_session_t * ses, xrtp_teleport_t * rtp_port, xrtp_teleport_t * rtcp_port){

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

 int session_match_member_src(void * tar, void * pat){

    member_state_t * stat = (member_state_t *)tar;
    uint32 * src = (uint32 *)pat;

    session_log(("session_match_member_src: member.src=%u, wanted src=%u\n", stat->ssrc, *src));
    return (stat->ssrc) - *src;
 }
 
 int session_match_member(void * tar, void * pat){

    member_state_t * stat = (member_state_t *)tar;
    member_state_t *mem = (member_state_t *)pat;

    return stat - mem;
 }

 member_state_t * session_new_member(xrtp_session_t * ses, uint32 src, void * user_info){

    member_state_t * stat = xrtp_list_find(ses->members, &src, session_match_member, &(ses->$member_list_block));
    if(stat){
       /* Exist, quit */
       return stat;
    }

    stat = (member_state_t *)malloc(sizeof(struct member_state_s));
    if(!stat){

       return NULL;
    }
    
    bzero(stat, sizeof(struct member_state_s));

    stat->lock = xthr_new_lock();
    if(!stat->lock) {

       free(stat);
       return NULL;
    }
    
    stat->ssrc = src;
    stat->session = ses;
    stat->user_info = user_info;
    
    stat->rtp_buffer = xrtp_list_new();
    
    xrtp_list_add_first(ses->members, stat);
    
    /* Only count valid member
     ses->n_member++;
     */
     
    session_log(("session_new_member: petential member[%u] added, Session[%d] has %d valid members.\n", stat->ssrc, session_id(ses), ses->n_member));

    return stat;    
 }

 int session_done_rtp(void* gen){

    xrtp_rtp_packet_t * rtp = (xrtp_rtp_packet_t *)gen;

    rtp_packet_done(rtp);

    return XRTP_OK;
 }

 int session_done_member(member_state_t * mem){

    xrtp_list_free(mem->rtp_buffer, session_done_rtp);

    xthr_done_lock(mem->lock);
    
    if(mem->cname) free(mem->cname);
    
    free(mem);

    return XRTP_OK;
 }

 int session_delete_sender(xrtp_session_t * ses, member_state_t *senda){

    xrtp_list_remove(ses->senders, (void *)senda, session_match_member);

    return XRTP_OK;
 }

 int session_delete_member(xrtp_session_t * ses, member_state_t *mem){

    member_state_t * member = xrtp_list_remove(ses->members, (void *)mem, session_match_member);
    if(member){

       ses = member->session;

       if(ses && ses->$callbacks.member_deleted){

          ses->$callbacks.member_deleted(ses->$callbacks.member_deleted_user, member->ssrc, member->user_info);
          member->user_info = NULL;
       }

       session_done_member(member);
    }

    return XRTP_OK;
 }

 member_state_t * session_member_state(xrtp_session_t * ses, uint32 src){

    member_state_t * stat = xrtp_list_find(ses->members, &src, session_match_member_src, &(ses->$member_list_block));
    
    return stat;
 }

 int session_member_rtp_lost(member_state_t * mem, uint32 *r_lost, uint8 *r_frac){

    uint32 extended_max, expected;
    uint32 expected_interval, received_interval, lost_interval;

    /* Calcu lost & fraction rate RFC3550 A.3 */
    extended_max = (mem->cycles << 16) + mem->max_seq;
    expected = extended_max - mem->base_seq + 1;

    *r_lost = expected - mem->received;  /* total lost */

    expected_interval = expected - mem->expected_prior;
    mem->expected_prior = expected;

    received_interval = mem->received - mem->received_prior;
    mem->received_prior = mem->received;

    lost_interval = expected_interval - received_interval;

    if (expected_interval == 0 || lost_interval <= 0)
        *r_frac = 0;
    else
        *r_frac = (lost_interval << 8) / expected_interval;   /* lost fraction */

    return XRTP_OK;
 }

 int session_member_update_rtp(member_state_t * mem, xrtp_rtp_packet_t * rtp){

    if(rtp->member_state_updated || mem->session->bye_mode){

        return XRTP_OK;
    }

    if(mem->n_rtp_received == 0)  mem->base_seq = rtp_packet_seqno(rtp);

    mem->n_rtp_received++;
    mem->n_payload_oct_received += rtp_packet_payload_bytes(rtp);
    session_log(("session_member_update_rtp: %d packets and %u bytes received from Member[%u]\n", mem->n_rtp_received, mem->n_payload_oct_received, mem->ssrc));

    rtp->member_state_updated  = 1;

    return XRTP_OK;
 }

 int _session_cmp_rtp(void *tar, void *pat){

    xrtp_rtp_packet_t * rtp_tar = (xrtp_rtp_packet_t *)tar;
    xrtp_rtp_packet_t * rtp_pat = (xrtp_rtp_packet_t *)pat;

    if(!rtp_tar->is_sync && !rtp_pat->is_sync){
      
        int32 d = rtp_tar->exseqno - rtp_pat->exseqno;

        /* Consider the roundup of seqno */
        if(d < 0 && -d > RTCP_MAX_SEQNO_DIST){

           return -d - 1;
        }

        return d;

    }

    return rtp_packet_timestamp(rtp_tar) - rtp_packet_timestamp(rtp_pat);
 }

 int session_member_hold_rtp(member_state_t * mem, xrtp_rtp_packet_t * rtp){

    xrtp_list_add_ascent_once(mem->rtp_buffer, rtp, _session_cmp_rtp);

    return XRTP_OK;
 }
 
 xrtp_rtp_packet_t * session_member_next_rtp_withhold(member_state_t * mem){

    xrtp_rtp_packet_t * rtp = (xrtp_rtp_packet_t *)xrtp_list_remove_first(mem->rtp_buffer);

    return rtp;
 }

 int session_member_set_connects(member_state_t * mem, session_connect_t *rtp_conn, session_connect_t *rtcp_conn){

    if(mem->rtp_connect && mem->rtcp_connect)
        return XRTP_OK;
        
    if(mem->session->$callbacks.member_connects){

      session_log(("session_member_set_connects: Callback to application for rtp and rtcp port allocation\n"));
      mem->session->$callbacks.member_connects(mem->session->$callbacks.member_connects_user, mem->ssrc, &rtp_conn, &rtcp_conn);

    }else{

      if(!rtp_conn && !rtcp_conn){

          session_log(("< session_member_set_connects: Need at least a connect of rtp or rtcp >\n"));
          return XRTP_EPARAM;
      }
      
      session_log(("session_member_set_connects: RFC1889 static port allocation\n"));
      
      if(!rtp_conn) rtp_conn = connect_rtcp_to_rtp(rtcp_conn);
      if(!rtcp_conn) rtcp_conn = connect_rtp_to_rtcp(rtp_conn);      
    }

    mem->rtp_connect = rtp_conn;
    mem->rtcp_connect = rtcp_conn;
    session_log(("session_member_set_connects: Member[%u] linked with rtp_connect_io[%u], rtcp_connect_io[%u]\n", mem->ssrc, connect_io(rtp_conn), connect_io(rtcp_conn)));

    return XRTP_OK;
 }
 
 int session_member_connects(member_state_t * mem, session_connect_t **rtp_conn, session_connect_t **rtcp_conn){

    *rtp_conn = mem->rtp_connect;
    *rtcp_conn = mem->rtcp_connect;

    return XRTP_OK;
 }

 int session_quit(xrtp_session_t * ses, int immidiately){

    ses->bye_mode = 1;

    if((immidiately = 1)){     /* Always immidiately quit */

       /* No Bye packet sent, other peers will timeout it */       
       session_done(ses);

       return XRTP_OK;
    }

    /* BYE Reconsideration is not implemented yet */
    return XRTP_OK;
 }

 /* Convert hrt to rtp ts */
 uint32 session_hrt2ts(xrtp_session_t * ses, xrtp_hrtime_t hrt){

    return hrt / ses->media->clockrate * ses->media->sampling_instance;
 }

 /* Convert rtp ts to hrt */
 xrtp_hrtime_t session_ts2hrt(xrtp_session_t * ses, uint32 ts){

    return ts / ses->media->sampling_instance * ses->media->clockrate;
 }

 /* Convert hrt to media time */
 media_time_t session_hrt2mt(xrtp_session_t * ses, xrtp_hrtime_t hrt){

    return ses->$callbacks.hrtime_to_mediatime(ses->$callbacks.hrtime_to_mediatime_user, hrt);
 }

 /* Convert mediatime to hrtime */
 xrtp_hrtime_t session_mt2hrt(xrtp_session_t * ses, media_time_t mt){

    return ses->$callbacks.mediatime_to_hrtime(ses->$callbacks.hrtime_to_mediatime_user, mt);
 }

 /* Map the rtp timestamp to the local playout time */
 xrtp_hrtime_t session_member_mapto_local_time(member_state_t * mem, xrtp_rtp_packet_t * rtp){

     xrtp_hrtime_t base_playout_time, desired_playout_offset, playout_offset = 0;
     xrtp_lrtime_t lrts_now;
     
     double delta_var = 0;

     if(rtp->local_mapped) return rtp->hrt_local;

     xthr_lock(mem->lock);
     
     /* Not right 
     rtp->hrt_rtpts = session_ts2hrt(mem->session, rtp_packet_timestamp(rtp));
     xrtp_lrtime_t lrt;
     
     session_ts2rt(mem->session, rtp_packet_timestamp(rtp), &lrt, &rtp->hrt_rtpts);
     */
     uint32 rtpts_arrival = media_realtime_to_rtpts(mem->session->media, rtp->lrt_arrival, rtp->hrt_arrival);
     uint32 transit = rtpts_arrival - rtp->hrt_rtpts;

     /* Adjust clock skew */
     int32 adj_skew = 0;
     xrtp_hrtime_t skew_threshold = session_rtp_period(mem->session);
     
     if(!mem->received){

        mem->delay_estimate = transit;
        mem->active_delay = transit;
        mem->last_transit = mem->last_last_transit = transit;

     }else{

        mem->delay_estimate = (31 * mem->delay_estimate + transit)/32;
     }

     if(mem->active_delay - mem->delay_estimate > skew_threshold){

        adj_skew = skew_threshold;
        mem->active_delay = mem->delay_estimate;
     }

     if(mem->active_delay - mem->delay_estimate < -skew_threshold){

        adj_skew = -skew_threshold;
        mem->active_delay = mem->delay_estimate;
     }

     /*
      * The minimum offset is base on the window of difference since
      * the last compensation for the skew adjustment.
      */
     if(adj_skew){

        mem->min_transit = transit;
        
     }else{

        mem->min_transit = mem->min_transit > transit ? transit : mem->min_transit;
     }

     base_playout_time = rtp->hrt_arrival + mem->min_transit;
     
     /* Adjust due to jitter */
     int delta_transit = transit - mem->last_transit;
     if (delta_transit < 0) delta_transit = -delta_transit;     /* abs(x) */
     
     mem->last_last_transit = mem->last_transit;
     mem->last_transit = transit;

     if(delta_transit > SPIKE_THRESHOLD){     /* FIXME: Thershold should be adjustable */

        /* A new delay spike started */
        mem->play_mode = PLAYMODE_SPIKE;
        mem->spike_var = 0;
        mem->playout_adapt = 0;
        mem->in_spike = IN_SPIKE;
        
     }else{

        if(mem->play_mode == PLAYMODE_SPIKE){

           mem->spike_var /= 2;
           delta_var = (abs(transit - mem->last_transit) + abs(transit - mem->last_last_transit))/8;
           mem->spike_var += delta_var;
           
           if(mem->spike_var < mem->spike_end){
              /* Slope is flat, return to normal operation */
              mem->play_mode = PLAYMODE_NORMAL;
              
              /* Not a spike, continue update jitter */
              mem->jitter += (1./16.) * ((double)delta_transit - mem->jitter);
           }
           mem->playout_adapt = 0;

        }else{

           /* Maintain spike end */
           if(!mem->in_spike){
              mem->spike_end = (abs(transit - mem->last_transit) + abs(transit - mem->last_last_transit))/8;
           }else{
              mem->in_spike--;
           }
           
           if(mem->consecutive_dropped > CONSECUTIVE_DROP_THRESHOLD){
              /* Too many consecutive packet dropped, need adaption */
              mem->playout_adapt = 1;
           }
           
           lrts_now = lrtime_now(mem->session->clock);
           if((lrts_now - mem->lrts_last_heard) > INACTIVE_THRESHOLD){
              /* Silent source restartted, network condition maybe changed */
              mem->playout_adapt = 1 ;
           }
           mem->lrts_last_heard = lrts_now;

           /* Not a spike, continue update jitter */
           mem->jitter += (1./16.) * ((double)delta_transit - mem->jitter);
        }
     }
     
     desired_playout_offset = 3 * mem->jitter;   /* FIXME: fact 3 should consider to be adjustable */
     if(mem->playout_adapt){

        mem->last_playout_offset = playout_offset;
        playout_offset = desired_playout_offset;
        
     }else{

        playout_offset = mem->last_playout_offset;
     }

     rtp->hrt_local = base_playout_time + adj_skew + playout_offset;
     xthr_unlock(mem->lock);

     rtp->local_mapped = 1;

     session_log(("session_member_mapto_local_time: skew adjust is hrt[%d], play offset is htr[%d]\n", adj_skew, playout_offset));
     session_log(("session_member_mapto_local_time: local base play time is hrt[%d], adjust to htr[%d]\n", base_playout_time, rtp->hrt_local));

     return rtp->hrt_local;
 }

 int session_make_ssrc(xrtp_session_t * ses){

    md5_state_t sign;
    uint32 salt;
    uint32 digest[4];

    uint32 ssrc = 0;

    md5_init(&sign);

    salt = session_signature(ses);
    md5_append(&sign, (md5_byte_t*)(&salt), sizeof(salt));

    salt = ses->media->sign(ses->media);
    md5_append(&sign, (md5_byte_t*)(&salt), sizeof(salt));

    md5_finish(&sign, (md5_byte_t *)digest);

    int i;
    for (i = 0; i < 3; i++) {
       ssrc ^= digest[i];
    }

    ses->self->ssrc = ssrc;
    /* FIXME: Maybe reset self member state */

    return XRTP_OK;
 }
 
 uint32 session_signature(xrtp_session_t * ses){

    if(ses->$callbacks.need_signature){
        return ses->$callbacks.need_signature(ses->$callbacks.need_signature_user, ses);
    }

    srand(rand());
    
    return rand();
 }

 int session_solve_collision(member_state_t * mem, uint32 ssrc){

     xrtp_session_t * ses = mem->session;
     
     if(ses->self == mem){

        /* The session is collided */
        
        if(ses->$callbacks.before_reset)
            ses->$callbacks.before_reset(ses->$callbacks.before_reset_user, ses);
            
        /* Silently changed, old ssrc will timeout by other participants
         * May implements bye mode later.
         */
        session_make_ssrc(ses);
        ses->self->n_rtp_send = ses->self->n_payload_oct_send = 0;
        
        if(ses->$callbacks.after_reset)
            ses->$callbacks.after_reset(ses->$callbacks.after_reset_user, ses);
     }

     if(ses->self != mem){

        /* A third party collision detected 
         * Nothing to do, just keep receiving the media for the exist member, ignore the
         * new coming participant, the situation will be solved soon by the collided parties.
         */
     }

     return XRTP_OK;
 }

 member_state_t * session_update_member_by_rtcp(xrtp_session_t * ses, xrtp_rtcp_compound_t * rtcp){

    uint32 src = rtcp_sender(rtcp);

    char * cname = NULL;
    int cname_len;
    cname_len = rtcp_sdes(rtcp, src, RTCP_SDES_CNAME, &cname);

    /* Get connect of the participant */
    session_connect_t *rtp_conn = NULL;
    session_connect_t *rtcp_conn = rtcp->connect;
    rtcp->connect = NULL; /* Dump the connect from rtcp packet */

    session_log(("session_update_member_by_rtcp: my ssrc is %d, rtcp_connect_io=%d\n", ses->self->ssrc, connect_io(rtcp_conn)));

    member_state_t * mem = session_member_state(ses, src);
    if(!mem){

        session_log(("session_update_member: New petential member coming\n"));
        
        /* Need Authorization */
        void * user_info = NULL;
        if(ses->$callbacks.member_apply){

           if(ses->$callbacks.member_apply(ses->$callbacks.member_apply_user, src, cname, cname_len, &user_info) < XRTP_OK){

               /* Member application fail */
               if(rtp_conn) connect_done(rtp_conn);
               if(rtcp_conn) connect_done(rtcp_conn);
               
               session_log(("< session_update_member: SSRC[%d]'s member application refused >\n", src));
               return NULL;
           }
        }   

        if(!cname_len && !ses->$state.receive_from_anonymous){

           /* Anonymous intended */
           if(rtp_conn) connect_done(rtp_conn);
           if(rtcp_conn) connect_done(rtcp_conn);

           session_log(("< session_update_member: Anonymous refused >\n"));
           return NULL;
           
        }

        mem = session_new_member(ses, src, user_info);
        if(!mem){

           if(rtp_conn) connect_done(rtp_conn);
           if(rtcp_conn) connect_done(rtcp_conn);

           return NULL;
        }

    }else if(mem->valid || cname_len == 0){

        /* Free as member has connects already, Can't be validated yet */
        if(rtp_conn) connect_done(rtp_conn);
        if(rtcp_conn) connect_done(rtcp_conn);
        
        if(mem->valid)
            session_log(("session_update_member: Existed validated Member[%d]\n", src));
        else
            session_log(("session_update_member: Can't validate Member[%d] yet\n", src));
    }

    if(!mem->valid && cname_len){    /* Validate a petential member */

        session_member_set_connects(mem, rtp_conn, rtcp_conn);

        mem->cname = (char *)malloc(cname_len);
        if(!mem->cname){
              
            /* Free as member has connects already */
            if(rtp_conn) connect_done(rtp_conn);
            if(rtcp_conn) connect_done(rtcp_conn);
        }

        memcpy(mem->cname, cname, cname_len + 1);
        mem->cname[cname_len] = '\0';
        mem->cname_len = cname_len;
                
        mem->valid = XRTP_YES;
        ses->n_member++;

        if(ses->join_to_rtcp_port && connect_from_teleport(rtcp_conn, ses->join_to_rtcp_port)){

            session_log(("session_update_member: Session[%u] has joined to [%s]:%d successfully\n", session_id(ses), teleport_name(ses->join_to_rtcp_port), teleport_portno(ses->join_to_rtcp_port)));

            /* participant joined, clear the join desire */
            teleport_done(ses->join_to_rtp_port);
            ses->join_to_rtp_port = NULL;
            teleport_done(ses->join_to_rtcp_port);
            ses->join_to_rtcp_port = NULL;
        }
            
        /* callback when the member is valid */
        if(ses->$callbacks.member_update)
           ses->$callbacks.member_update(ses->$callbacks.member_update_user, src, cname, cname_len);

        session_log(("session_update_member: Member['%s'] validated and accepted\n", mem->cname));
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
    xrtp_session_t * ses = sender->session;
    
    /* Map rtp ts to local time :
     * - member's hrts_local and rtp_ts from the most recent received rtp packet
     * + the rtp_ts from this rtcp sr
     * = the hrts_local from the rtp_ts
     */
    if(ses->$callbacks.resync_timestamp){

       /* Implement consideration:
        * Insert a sync rtp packet with empty data to playout buffer
        */
       xrtp_rtp_packet_t * sync = rtp_new_packet(ses, ses->$state.profile_no, RTP_RECEIVE, NULL, 0, 0);
       if(sync){

          sync->is_sync = 1;
          sync->hi_ntp = hi_ntp;
          sync->lo_ntp = lo_ntp;
          sync->local_mapped = 1;
          rtp_packet_set_timestamp(sync, rtp_ts);    /* Not used, as converted to hrt */

          session_member_hold_rtp(sender, sync);
       }
    }

    /* For report lsr: middle 32 bits of NTP */
    sender->lsr_ts = (hi_ntp << 16) + (lo_ntp >> 16);
    
    /* What to do with packet_sent, octet_sent */

    return XRTP_OK;
 }

 int session_member_check_report(member_state_t * mem, uint8 frac_lost, uint32 intv_lost,
                          uint32 full_seqno, uint32 jitter,
                          uint32 lsr_stamp, uint32 lsr_delay)
 {
    /* Calc network Round-trip Time(RTT) */
    xrtp_session_t * ses = mem->session;
    uint32 rtt = 0;
    
    if(mem == mem->session->self){

       /* ntp_now - lsr_stamp - lsr_delay */
       
       uint32 hi_ntp = 0, lo_ntp = 0, ntpts_now;
       if(ses->$callbacks.ntp_timestamp){

          ses->$callbacks.ntp_timestamp(ses->$callbacks.ntp_timestamp_user, &hi_ntp, &lo_ntp);
       }

       ntpts_now = (hi_ntp << 16) + (lo_ntp >> 16);
       rtt = ntpts_now - lsr_stamp - lsr_delay;
    }

    /* Filter network condition and report to user:
     * Check packet lost and Network Jitter
     * if result over the limitation, report to user
     */
    if(ses->$callbacks.member_report)
       ses->$callbacks.member_report(ses->$callbacks.member_report_user, mem->user_info, frac_lost, intv_lost, jitter, rtt);

    return XRTP_OK;                        
 }

 int session_member_make_report(member_state_t * mem, uint8 *frac_lost,
                                uint32 *total_lost, uint32 *exseqno)
 {

    /**
     * Calc packet lost, copied from RFC 3550 A.3
     */
    uint32 expected, expected_interval;
    uint32 received_interval;
    
    *exseqno = session_member_max_exseqno(mem);
    expected = *exseqno - mem->base_seq + 1;

    expected_interval = expected - mem->expected_prior;
    mem->expected_prior = expected;
    
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
int session_set_buffer(xrtp_session_t *ses, int size, xrtp_hrtime_t timesize){

   if(!ses->packets_in_sched){

      ses->packets_in_sched = queue_new(size);
      if(!ses->packets_in_sched){

         session_log(("< session_set_buffer: No packets queue created ! >\n"));
         return XRTP_EMEM;
      }
   }

   ses->timesize = timesize;

   return XRTP_OK;
}

int session_set_rtp_rate(xrtp_session_t *ses, int rate){

   ses->period = HRTIME_SECOND_DIVISOR / rate;

   if(ses->bandwidth != 0){

      /* bandwidth per clock period */
      ses->bandwidth /= rate;
      ses->rtp_bw /= rate;
      ses->rtcp_bw = ses->bandwidth - ses->rtp_bw;

      session_log(("session_set_rtp_rate: total bandwidth %d bytes of %d nsec period\n", ses->bandwidth, ses->period));
      session_log(("session_set_rtp_rate: rtp bandwidth %d bytes of %d nsec period\n", ses->rtp_bw, ses->period));
   }
         
   session_set_buffer(ses, RTP_MAX_PACKET_DELAY, ses->period * RTP_DELAY_FACTOR);

   return XRTP_OK;
}

 rtime_t session_rtp_period(xrtp_session_t *ses){

    return ses->period;
 }
 
 xrtp_hrtime_t session_rtp_delay(xrtp_session_t *ses){

    return ses->period * RTP_DELAY_FACTOR;
 }

 xrtp_lrtime_t session_rtcp_interval(xrtp_session_t *ses){

    xrtp_lrtime_t t = session_interval(ses->n_member, ses->n_sender,
                            ses->rtcp_bw, ses->self->we_sent,
                            ses->rtcp_avg_size, ses->rtcp_init);

    /*
    session_log("session_rtcp_interval: ses->n_member = %d\n", ses->n_member);
    session_log("session_rtcp_interval: ses->n_sender = %d\n", ses->n_sender);
    session_log("session_rtcp_interval: ses->rtcp_bw = %d\n", ses->rtcp_bw);
    session_log("session_rtcp_interval: ses->self->we_sent = %d\n", ses->self->we_sent);
    session_log("session_rtcp_interval: ses->rtcp_avg_size = %d\n", ses->rtcp_avg_size);
    session_log("session_rtcp_interval: ses->rtcp_init = %d\n", ses->rtcp_init);
    session_log("session_rtcp_interval: is %d csec\n", t);
    */

    ses->rtcp_interval = t;
    
    return t;
 }

 xrtp_lrtime_t session_rtcp_delay(xrtp_session_t *ses){

    return RTCP_MIN_INTERVAL / 2;
 }

 int session_count_rtcp(xrtp_session_t * ses, xrtp_rtcp_compound_t * rtcp){

    if(!ses->n_rtcp_recv){

       ses->rtcp_avg_size = rtcp->bytes_received;
       
    }else{

       ses->rtcp_avg_size = (31 * ses->rtcp_avg_size + rtcp->bytes_received)/32;
    }
    
    ses->n_rtcp_recv++;

    return XRTP_OK;
 }
 
 int session_notify_delay(xrtp_session_t * ses, xrtp_hrtime_t howlate){

    if(ses->$callbacks.notify_delay)
       ses->$callbacks.notify_delay(ses->$callbacks.notify_delay_user, ses, howlate);

    return XRTP_OK;
 }

 /**
  * Set mode of the Session, which could be
  *
  * 'SESSION_SEND', 'SESSION_RECV' or 'SESSION_DUPLEX' etc.
  */
 int session_set_mode(xrtp_session_t * ses, int mode){

    switch(mode){

       case(SESSION_SEND):
          ses->mode_trans = mode;
          if(!ses->rtp_send_pipe){
             ses->rtp_send_pipe = pipe_new(ses, XRTP_RTP, XRTP_SEND);
             ses->rtcp_send_pipe = pipe_new(ses, XRTP_RTCP, XRTP_SEND);
          }
          break;
          
       case(SESSION_RECV):
          ses->mode_trans = mode;
          if(!ses->rtp_recv_pipe){
             ses->rtp_recv_pipe = pipe_new(ses, XRTP_RTP, XRTP_RECEIVE);
             ses->rtcp_recv_pipe = pipe_new(ses, XRTP_RTCP, XRTP_RECEIVE);
          }
          break;
          
       case(SESSION_DUPLEX):
          ses->mode_trans = mode;
          if(!ses->rtp_send_pipe){
             ses->rtp_send_pipe = pipe_new(ses, XRTP_RTP, XRTP_SEND);
             ses->rtcp_send_pipe = pipe_new(ses, XRTP_RTCP, XRTP_SEND);
             ses->rtp_recv_pipe = pipe_new(ses, XRTP_RTP, XRTP_RECEIVE);
             ses->rtcp_recv_pipe = pipe_new(ses, XRTP_RTCP, XRTP_RECEIVE);
          }
          break;

       default:   return XRTP_EUNSUP;
    }

    return XRTP_OK;
 }
 
int session_done_module(void *gen) {

   profile_class_t * mod = (profile_class_t *)gen;

   mod->done(mod);

   return OS_OK;
}

xrtp_media_t * session_new_media(xrtp_session_t * ses, char * id, uint8 payload_type){

   char * mid = NULL;
    
   if(ses->media){

      mid = ses->media->mime(ses->media);
       
      if(!id || !strcmp(mid, id)){  /* Same module or new media mime is none */

         ses->$state.profile_no = payload_type;
          
         return ses->media;

      }else{

         /* restore bandwidth per second */
         ses->bandwidth *= ses->media->clockrate;
         ses->rtp_bw *= ses->media->clockrate;
          
         session_log(("session_new_media: total bandwidth %d bytes/second before set new media\n", ses->bandwidth));
         session_log(("session_new_media: total rtp bandwidth %d bytes/second before set new media\n", ses->rtp_bw));
      }
   }

   profile_class_t * modu = NULL;
    
   /* Search a suitable media handling module */
   xrtp_list_user_t $lu; 
   xrtp_list_t *list = xrtp_list_new();
   
   if (catalog_create_modules(ses->module_catalog, "media", list) <= 0) {

      xrtp_list_free(list, NULL);
      session_log(("session_new_media: No media module found\n"));
      
      return NULL;
   }

   modu = (profile_class_t *)xrtp_list_first(list, &$lu);
   while (modu) {

      if (!strcmp(modu->id(modu), id)) {

         xrtp_list_remove_item(list, modu);
         break;
      }
      
      modu = (profile_class_t *)xrtp_list_next(list, &$lu);
   }

   xrtp_list_free(list, session_done_module);
   
   if(!modu){

      session_log(("session_new_media: No '%s' module found\n", id));
      return NULL;
   }
    
   profile_handler_t * med = modu->new_handler(modu, ses);
   if(!med){

      modu->done(modu);
      return NULL;
   }

   if(ses->mode_trans == SESSION_SEND || ses->mode_trans == SESSION_DUPLEX){

      if(!ses->media){
         
         pipe_add(ses->rtp_send_pipe, med, XRTP_ENABLE);
         pipe_add(ses->rtcp_send_pipe, med, XRTP_ENABLE);
          
      }else if(!pipe_replace(ses->rtp_send_pipe, mid, med)){
         
         modu->done_handler(med);
         modu->done(modu);
         return NULL;
      }
       
   }else if(ses->mode_trans == SESSION_RECV || ses->mode_trans == SESSION_DUPLEX){
       
      if(ses->media){
         
         pipe_add(ses->rtp_recv_pipe, med, XRTP_ENABLE);
         pipe_add(ses->rtcp_recv_pipe, med, XRTP_ENABLE);

      }else if(!pipe_replace(ses->rtp_recv_pipe, mid, med)){
         
         return NULL;
      }
   }

   if(ses->media) ses->media->done(ses->media);
    
   ses->media = med->media(med);
   ses->period = HRTIME_SECOND_DIVISOR / ses->media->clockrate;

   if(ses->bandwidth != 0){

      /* bandwidth per clock period */
      ses->bandwidth /= ses->media->clockrate;
      ses->rtp_bw /= ses->media->clockrate;
      ses->rtcp_bw = ses->bandwidth - ses->rtp_bw;

      session_log(("session_new_media: bandwidth %d bytes / %d nsec after set new media\n", ses->bandwidth, ses->period));
      session_log(("session_new_media: rtp bandwidth %d bytes / %d nsec after set new media\n", ses->rtp_bw, ses->period));
   }
    
   if(!ses->self->ssrc) session_make_ssrc(ses);
    
   ses->$state.profile_no = payload_type;

   return ses->media;
}
  
/**
 * The middle process b/w import and export, order in addition
 * Can be compress module and crypo module, number of module less than MAX_PIPE_STEP
 * and be can enabled/disabled.
 */
profile_handler_t * session_add_handler(xrtp_session_t * ses, char * id){

   profile_class_t * modu = NULL;
   
   /* Search a suitable media handling module */
   xrtp_list_user_t $lu;
   xrtp_list_t *list = xrtp_list_new();

   if (catalog_create_modules(ses->module_catalog, "rtp_profile", list) <= 0) {

      xrtp_list_free(list, NULL);
      session_log(("session_add_handler: No media module found\n"));

      return NULL;
   }

   modu = (profile_class_t *)xrtp_list_first(list, &$lu);
   while (modu) {

      if (!strcmp(modu->id(modu), id)) {

         xrtp_list_remove_item(list, modu);
         break;
      }

      modu = (profile_class_t *)xrtp_list_next(list, &$lu);
   }

   xrtp_list_free(list, session_done_module);

   if(!modu){

      session_log(("session_add_handler: No '%s' module found\n", id));
      return NULL;
   }

   session_log(("session_add_handler: '%s' module created.\n", id));
   
   int mtype = modu->type(modu);
   if(mtype == XRTP_HANDLER_TYPE_MEDIA){

      if(ses->media){
         
         modu->done(modu);
         return ses->media_handler;
      }
   }

   profile_handler_t * hand = modu->new_handler(modu, ses);
   if(!hand){

      session_log(("< session_add_handler: Fail to create '%s' handler. >\n", id));
      return NULL;
   }

   session_log(("session_add_handler: '%s' handler created.\n", id));
   if(mtype == XRTP_HANDLER_TYPE_MEDIA){

      if(!ses->media){

         session_log(("session_add_handler: create media handler\n"));

         ses->media_handler = hand;

         ses->media = hand->media(hand);
      }
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

       case(CALLBACK_SESSION_SYNC_TIMESTAMP):
           ses->$callbacks.sync_timestamp = call;
           ses->$callbacks.sync_timestamp_user = user;
           session_log(("session_set_callback: 'sync_timestamp' callback added\n"));
           break;

       case(CALLBACK_SESSION_RESYNC_TIMESTAMP):
           ses->$callbacks.resync_timestamp = call;
           ses->$callbacks.resync_timestamp_user = user;
           session_log(("session_set_callback: 'resync_timestamp' callback added\n"));
           break;

       case(CALLBACK_SESSION_RTP_TIMESTAMP):
           ses->$callbacks.rtp_timestamp = call;
           ses->$callbacks.rtp_timestamp_user = user;
           session_log(("session_set_callback: 'rtp_timestamp' callback added\n"));
           break;

       case(CALLBACK_SESSION_NTP_TIMESTAMP):
           ses->$callbacks.ntp_timestamp = call;
           ses->$callbacks.ntp_timestamp_user = user;
           session_log(("session_set_callback: 'ntp_timestamp' callback added\n"));
           break;

       case(CALLBACK_SESSION_HRT2MT):
           ses->$callbacks.hrtime_to_mediatime = call;
           ses->$callbacks.hrtime_to_mediatime_user = user;
           session_log(("session_set_callback: 'hrtime_to_mediatime' callback added\n"));
           break;

       case(CALLBACK_SESSION_MT2HRT):
           ses->$callbacks.mediatime_to_hrtime = call;
           ses->$callbacks.mediatime_to_hrtime_user = user;
           session_log(("session_set_callback: 'mediatime_to_hrtime' callback added\n"));
           break;

       default:
           session_log(("< session_set_callback: callback[%d] unsupported >\n", type));
           return XRTP_EUNSUP;
    }

    return XRTP_OK;
 }

 /**
  * Send number of empty rtp packet to out/in pipe for containing media unit.
  */
 int session_rtp_to_send(xrtp_session_t *ses, xrtp_hrtime_t hrt_allow, int last_packet){

    xrtp_rtp_packet_t * rtp;
    xrtp_hrtime_t hrts_start;
    
    session_log(("session_rtp_to_send: Session[%u] gets to produce last rtp frame packet within %d nsec for scheduling.\n", session_id(ses), hrt_allow));

    /* Packet must be made within certain hrtime 
    if(hrt_allow == 0) hrt_allow = MAX_PRODUCE_TIME;
     */
     
    /* <debug> */
    static int mc;
    if(last_packet){
      
        session_log(("session_rtp_to_send: Session[%u] gets to produce last rtp frame packet within %d nsec for scheduling.\n", session_id(ses), hrt_allow));
        mc = 0;
        
    }else{
      
        session_log(("session_rtp_to_send: Session[%u] gets to produce %d frame packet within %d nsec for scheduling.\n", session_id(ses), mc++, hrt_allow));
    }
    /* </debug> */
    

    if(queue_is_full(ses->packets_in_sched)){

       session_log(("< session_rtp_to_send: Too many packets waiting for sending >\n"));
       /* FIXME: May callback to notify the applicaton */
       return XRTP_EBUSY;
    }

    /* FIXME: May consider time buffer full condition */

    /* FIXME: May race condition with rtcp, need sychronization */
    if(ses->self->ssrc == 0){

       if(ses->$callbacks.need_signature){

          ses->self->ssrc = ses->$callbacks.need_signature(ses->$callbacks.need_signature_user, ses);

       }else{

          ses->self->ssrc = rand();
       }
    }

    session_log(("session_rtp_to_send: Session[%d] ssrc is %d\n", session_id(ses), ses->self->ssrc));

    /* Check time */
    hrts_start = hrtime_now(ses->clock);
    if(ses->$state.n_pack_sent == 0){

       ses->$state.hrts_start_send = hrts_start;
       ses->$state.lrts_start_send = lrtime_now(ses->clock);
    }

    /* Test Time Atom
    ts_pass = time_passed(ses->clock, ts_start);
    session_log("session_rtp_to_send: time atom is %d\n", ts_pass);
     */
     
    rtp = rtp_new_packet(ses, ses->$state.profile_no, RTP_SEND, NULL, 0, 0);
    if(!rtp){

       session_log(("< session_rtp_to_send: Can't create rtp packet for sending >\n"));
       return XRTP_EMEM;
    }

    rtp_packet_set_head(rtp, ses->self->ssrc, ses->self->max_seq, ses->timestamp);

    /* set the time to send, delay one period */
    session_log(("session_rtp_to_send: rtp packet will be produced\n"));
    
    int packet_bytes;
    
    int pump_ret = pipe_pump(ses->rtp_send_pipe, hrt_allow, rtp, &packet_bytes);
    if(pump_ret == XRTP_ETIMEOUT){

       session_log(("< session_rtp_to_send: Can't made packet on time, cancelled producing >\n"));
       rtp_packet_done(rtp);

       return pump_ret;
    }                             

    if(pump_ret < XRTP_OK){

       session_log(("< session_rtp_to_send: Fail to produce packet >\n"));
       rtp_packet_done(rtp);

       return pump_ret;
    }

    if(pump_ret >= XRTP_OK){

       xrtp_hrtime_t hrt_pass = hrtime_passed(ses->clock, hrts_start);  /* Get the making time */
       session_log(("session_rtp_to_send: rtp packet production spent %d nanoseconds, while allowed processing time is %d nsecs\n", hrt_pass, hrt_allow));

       rtp_packet_set_info(rtp, packet_bytes);
       queue_wait(ses->packets_in_sched, rtp);
       
       if(last_packet){

          session_log(("session_rtp_to_send: Session[%d]'s scheduler is sched[@%u]\n", session_id(ses), (int)(ses->sched)));
          ses->sched->rtp_out(ses->sched, ses, hrt_allow - hrt_pass, 0);
          ses->timestamp += ses->media->sampling_instance;
       }
    }

    return XRTP_OK;
 }
  
 int session_cancel_rtp_sending(xrtp_session_t *ses, xrtp_hrtime_t ts){

    pipe_discard(ses->rtp_send_pipe, ts);

    ses->sched->rtp_out(ses->sched, ses, ts, 1);
    
    return XRTP_OK;
 }
 
 int session_rtp_outgoing(xrtp_session_t * ses, xrtp_rtp_packet_t * rtp){

    session_connect_t * conn = NULL;

    session_log(("session_rtp_outgoing: send rtp right now\n"));
    
    xrtp_buffer_t * buf= rtp_packet_buffer(rtp);
    char * data = buffer_data(buf);
    uint datalen = buffer_datalen(buf);

    if(port_is_multicast(ses->rtp_port)){

        session_log(("session_rtp_outgoing: Multicasting is not implemented yet\n"));
        
    }else{
      
        struct xrtp_list_user_s ul;
        
        session_log(("session_rtp_outgoing: Unicasting start ...\n"));
        member_state_t * mem = xrtp_list_first(ses->members, &ul);

        int nsent = 0;
        while(mem){

            if(mem != ses->self){
              
                conn = mem->rtp_connect;

                session_log(("session_rtp_outgoing: sent to Member[%u], mem->rtp_connect = %u\n", mem->ssrc, (int)(mem->rtp_connect)));
                connect_send(conn, data, datalen);
                nsent++;
            }

            mem = xrtp_list_next(ses->members, &ul);
        }

        session_log(("session_rtp_outgoing: sent to %d members\n", nsent));

        if(ses->join_to_rtp_port){

            session_log(("session_rtp_outgoing: Session[%d] has a join desire[%u] pending\n", session_id(ses), (int)(ses->join_to_rtp_port)));
            conn = connect_new(ses->rtcp_port, ses->join_to_rtp_port);
            if(conn){

                session_log(("session_rtp_outgoing: send to joining connect\n"));
                connect_send(conn, data, datalen);
                connect_done(conn);

            }else{

                session_log(("session_rtp_outgoing: Fail to conect\n"));
            }
        }
    }

    ses->self->max_seq++;
    session_log(("session_rtp_outgoing: Broadcasted ...\n"));
    return XRTP_OK;
 }

 int32 session_receiver_rtp_bw(xrtp_session_t * ses){

    session_log(("session_receiver_rtp_bw: %d valid members in the session\n", ses->n_member));
    if(port_is_multicast(ses->rtp_port) || ses->n_member <= 1)
        return ses->rtp_bw;

    else
        return ses->rtp_bw / (ses->n_member-1);   /* minus self */
 }

 int32 session_receiver_rtp_bw_left(xrtp_session_t * ses){

    session_log(("session_receiver_rtp_bw_left: %d valid members in the session\n", ses->n_member));
    if(port_is_multicast(ses->rtp_port) || ses->n_member-1 == 0)
        return ses->rtp_bw_left;

    else
        return ses->rtp_bw_left / (ses->n_member-1);   /* minus self */
 }

 int session_set_rtp_bw_left(xrtp_session_t * ses, int32 left){

    if(port_is_multicast(ses->rtp_port) || ses->n_member-1 == 0)
        ses->rtp_bw_left = left;

    else
        ses->rtp_bw_left = left * (ses->n_member-1);   /* minus self */

    session_log(("session_set_rtp_bw_left: %d members in the session, %d bytes bw in total\n", ses->n_member, ses->rtp_bw_left));
    return XRTP_OK;
 }

 /* Return n bytes sent */
 int session_rtp_send_now(xrtp_session_t *ses){

    int nbytes = 0;

    int bw_per_recvr = 0;   /* bandwidth per member to receive media per period */
    int packet_bytes = 0;

    xrtp_lrtime_t lrt_now = lrtime_now(ses->clock);
    xrtp_hrtime_t hrt_now = hrtime_now(ses->clock);

    xrtp_hrtime_t hrt_passed = 0;
    xrtp_lrtime_t lrt_passed = 0;

    xrtp_rtp_packet_t *rtp = (xrtp_rtp_packet_t *)queue_head(ses->packets_in_sched);

    /* Get next packet info */
    rtp_packet_info(rtp, &packet_bytes);

    /* Calculate current bandwidth */
    if(ses->$state.n_pack_sent){

        int rtp_bw = session_receiver_rtp_bw(ses);
        
        time_passed(ses->clock, ses->self->lrts_last_rtp_sent, ses->self->hrts_last_rtp_sent, &lrt_passed, &hrt_passed);
        
        bw_per_recvr += rtp_bw * lrt_passed / ((double)ses->period / LRT_HRT_DIVISOR);
        bw_per_recvr += rtp_bw * hrt_passed / ses->period;
        
        /* Use some bandwidth left */
        bw_per_recvr += session_receiver_rtp_bw_left(ses);

        session_log(("session_rtp_send_now: lrtime[%d]:hrtime[%d] passed, current rtp bandwidth available is %d bytes\n", lrt_passed, hrt_passed, bw_per_recvr));

    }else{

        time_passed(ses->clock, ses->$state.lrts_start_send, ses->$state.hrts_start_send, &lrt_passed, &hrt_passed);

        bw_per_recvr = session_receiver_rtp_bw(ses);    /* minus self */
        
        bw_per_recvr = bw_per_recvr + session_receiver_rtp_bw(ses) * (1 + hrt_passed / ses->period);

        session_log(("session_rtp_send_now: initial rtp bandwidth is %d bytes\n", bw_per_recvr));
    }

    while( rtp && bw_per_recvr >= packet_bytes){

       queue_serve(ses->packets_in_sched);

       session_rtp_outgoing(ses, rtp);
       rtp_packet_done(rtp);
       nbytes += packet_bytes;
       ses->$state.n_pack_sent++;
       bw_per_recvr -= packet_bytes;
       session_log(("session_rtp_send_now: rtp bandwidth %d bytes available\n", bw_per_recvr));

       rtp = (xrtp_rtp_packet_t *)queue_head(ses->packets_in_sched);
       if(!rtp)
          break;

       rtp_packet_info(rtp, &packet_bytes);

       session_log(("session_rtp_send_now: rtp bandwidth %d bytes available for next packet with %d bytes\n", bw_per_recvr, packet_bytes));
    }
    
    session_set_rtp_bw_left(ses, bw_per_recvr);
    
    if(rtp && bw_per_recvr < packet_bytes){

       /* FIXME: Consider delay resynchronization */
       ses->sched->rtp_out(ses->sched, ses, ses->period, 0); 
    }
    
    ses->self->lrts_last_rtp_sent = lrt_now;
    ses->self->hrts_last_rtp_sent = hrt_now;
    
    ses->$state.oct_sent += nbytes;
    
    return nbytes;
 }

 int session_rtp_to_receive(xrtp_session_t *ses){

    xrtp_rtp_packet_t * rtp;

    char * pdata = NULL;
    int datalen;

    xrtp_buffer_t * buf;
    
    xrtp_hrtime_t hrts_arrival;
    xrtp_lrtime_t lrts_arrival;

    datalen = connect_receive(ses->rtp_incoming, &pdata, 0, &hrts_arrival, &lrts_arrival);

    session_log(("session_rtp_to_receive: Prepare to receive RTP Packet\n"));

    /* New RTP Packet */
    rtp = rtp_new_packet(ses, ses->$state.profile_no, RTP_RECEIVE, ses->rtp_incoming, lrts_arrival, hrts_arrival);
    if(!rtp){

       session_log(("< session_rtp_to_receive: Can't create rtp packet for receiving >\n"));
       return XRTP_EMEM;
    }
    ses->rtp_incoming = NULL;

    buf = buffer_new(0, NET_BYTEORDER);
    buffer_mount(buf, pdata, datalen);
    
    rtp_packet_set_buffer(rtp, buf);
    
    int packet_bytes;
    if(pipe_pump(ses->rtp_recv_pipe, 0, rtp, &packet_bytes) != XRTP_CONSUMED){

        session_log(("session_rtp_to_receive: rtp is not freed yet, free now\n"));
        rtp_packet_done(rtp);
        
    }else{
      
        session_log(("session_rtp_to_receive: rtp is consumed already\n"));
    }
    
    return XRTP_OK;
 }
 
 /* Seperate thread for rtp receiving */
 int session_rtp_recv_loop(void * gen){

    xrtp_session_t * ses = (xrtp_session_t *)gen;

    ses->thread_run = 1;
    do{

        if(port_poll(ses->rtp_port, ses->period) <= 0){   /* Should be ONE actually if got incoming */

            /* FIXME: Maybe a little condition racing */
            if(ses->thread_run == 0){

                session_log(("session_rtp_recv_loop: quit loop.\n"));
                break;
            }

            continue;
        }

        xthr_lock(ses->rtp_recv_lock);

        if(ses->rtp_incoming){

            session_log(("session_rtp_recv_loop: Session[%d] is receiving rtp packet\n", session_id(ses)));
            session_rtp_to_receive(ses);
        }

        xthr_unlock(ses->rtp_recv_lock);

    }while(1);

    return XRTP_OK;
 }

 int session_start_receipt(xrtp_session_t * ses){

    ses->rtp_recv_lock = xthr_new_lock();
    if(ses->rtp_recv_lock == NULL){

        session_log(("< session_start: fail to create lock >\n"));
        return XRTP_FAIL;
    }

    ses->thr_rtp_recv = xthr_new(session_rtp_recv_loop, ses, XTHREAD_NONEFLAGS);
    if(ses->thr_rtp_recv == NULL){

        session_log(("< session_start: fail to create thread >\n"));
        return XRTP_FAIL;
    }

    session_log(("session_start: Session[%d] ready for receiving\n", session_id(ses)));
    return XRTP_OK;
 }
 
 int session_stop_receipt(xrtp_session_t * ses){

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

 int session_member_timeout(member_state_t * mem){

    xrtp_lrtime_t rtcp_tpass, rtp_tpass;
    xrtp_session_t * ses = mem->session;

    rtcp_tpass = ses->tc - mem->lrt_last_rtcp_sent;

    if(mem->we_sent){

        rtp_tpass = ses->tc - mem->lrts_last_rtp_sent;

        if(rtp_tpass > ses->rtcp_interval * RTCP_SENDER_TIMEOUT_MULTIPLIER){

            mem->we_sent = 0;
            session_delete_sender(ses, mem);
            ses->n_sender--;
        }
    }

    if(mem == ses->self)  return XRTP_NO;
    
    if(rtcp_tpass > ses->rtcp_interval * RTCP_MEMBER_TIMEOUT_MULTIPLIER){
      
        return XRTP_YES;
    }

    return XRTP_NO;
 }

 /* In cyclic fashion: If reash the last, the next one is the first one */
 member_state_t * session_next_valid_member_of(xrtp_session_t * ses, uint32 ssrc){

    int next_report_ssrc_stall = 0;
    
    member_state_t * mem = xrtp_list_find(ses->members, &ssrc, session_match_member_src, &(ses->$member_list_block));
    while(mem){

       uint32 ssrc2 = mem->ssrc;
       mem = xrtp_list_next(ses->members, &(ses->$member_list_block));
       if(!mem){
         
          mem = xrtp_list_first(ses->members, &(ses->$member_list_block));
          session_log(("session_next_member_of: Member[%u] is the last member! Return first Member[%u]\n", ssrc2, mem->ssrc));
       }

       if(!mem->valid){

          session_log(("< session_next_member_of: Member[%u] is not valid yet, skipped! >\n", ssrc2));
          continue;
       }

       if(session_member_timeout(mem)){

            session_log(("session_next_valid_member_of: Member['%s'] timeout, to delete\n", mem->cname));
            
            if(ses->next_report_ssrc == mem->ssrc){

                next_report_ssrc_stall = 1;
            }

            session_delete_member(ses, mem);
            ses->n_member--;
            continue;
       }

       if(next_report_ssrc_stall){
            
          ses->next_report_ssrc = mem->ssrc;
          next_report_ssrc_stall = 0;
       }
          
       session_log(("session_next_member_of: Next member of Member[%u] is Member[%u]\n", ssrc2, mem->ssrc));
       break;
    }
    
    return mem;
 }

 int session_need_rtcp(xrtp_session_t *ses){

    if(ses->n_member > 1 || ses->join_to_rtcp_port){
      
      session_log(("session_need_rtcp: Session[%d] has %d members, join_to_rtcp_port = %u, need rtcp.\n", session_id(ses), ses->n_member, (int)(ses->join_to_rtcp_port)));
      return 1;
    }
    
    session_log(("session_need_rtcp: Session[%d] has %d members, join_to_rtcp_port = %u, need not rtcp !\n", session_id(ses), ses->n_member, (int)(ses->join_to_rtcp_port)));
    return 0;
 }

 int session_rtcp_send_now(xrtp_session_t * ses, xrtp_rtcp_compound_t * rtcp){

    session_log(("session_rtcp_send_now: send rtcp right now\n"));
    xrtp_buffer_t * buf= rtcp_buffer(rtcp);
    char * data = buffer_data(buf);
    uint datalen = buffer_datalen(buf);
    session_connect_t * conn = NULL;

    int i;
    for(i=0; i<ses->n_rtcp_out; i++){

       conn = ses->outgoing_rtcp[i];

       session_log(("session_rtcp_send_now: sent to outgoing Connect.%d[@%u] of %d connects.\n", i, (int)(ses->outgoing_rtcp[i]), ses->n_rtcp_out));
       connect_send(conn, data, datalen);
    }

    if(ses->join_to_rtcp_port){

       session_log(("session_rtcp_send_now: Session[%d] has a join desire[%u] pending\n", session_id(ses), (int)(ses->join_to_rtcp_port)));
       conn = connect_new(ses->rtcp_port, ses->join_to_rtcp_port);
       if(conn){

          session_log(("session_rtcp_send_now: send to a petential member\n"));
          connect_send(conn, data, datalen);
          connect_done(conn);

       }else{

          session_log(("session_rtcp_send_now: Fail to conect\n"));
       }
    }

    session_log(("session_rtcp_send_now: sent to %d members\n", ses->n_rtcp_out));
    return XRTP_OK;
 }

 int session_rtcp_to_send(xrtp_session_t *ses){

    uint32 hi_ntp = 0, lo_ntp = 0;

    xrtp_hrtime_t hrt_local = 0;

    uint8 frac_lost; 
    uint32 total_lost;
    uint32 exseqno;
    
    uint32 lsr_delay = 0;

    /* Here is the implementation */
    member_state_t * self = ses->self;
    member_state_t * mem = NULL;

    int max_report;

    xrtp_rtcp_compound_t * rtcp;

    int rtcp_end = 0;
    
    xrtp_lrtime_t lrt_now;
    xrtp_hrtime_t hrt_now;
    time_now(ses->clock, &lrt_now, &hrt_now);

    while(self->ssrc == 0){

       if(ses->$callbacks.need_signature){

          self->ssrc = ses->$callbacks.need_signature(ses->$callbacks.need_signature_user, ses);

       }else{

          self->ssrc = rand();
       }
    }

    session_log(("session_rtcp_to_send: Session[%d] ssrc is %d\n", session_id(ses), self->ssrc));
    
    xrtp_lrtime_t lrt_pass = lrt_now - self->lrts_last_rtp_sent;
    if(lrt_pass > ses->rtcp_interval * RTCP_SENDER_TIMEOUT_MULTIPLIER || ses->$state.n_pack_sent == 0){

        session_log(("session_rtcp_to_send: Session[%d] hasn't sent rtp within %d csec, we are receiver\n", session_id(ses), lrt_pass));

        self->we_sent = 0;
        ses->rtp_bw_left = 0;
        
    }else{

        session_log(("session_rtcp_to_send: Session[%d] sent rtp within %d csec, we are sender\n", session_id(ses), lrt_pass));

        self->we_sent = 1;
    }

    if(self->we_sent){

        /* SR Sender Info */
        rtcp = rtcp_sender_report_new(ses, SESSION_DEFAULT_RTCP_NPACKET, XRTP_YES); /* need padding */
        if(!rtcp){

           session_log(("< session_rtcp_to_send: Can't create rtp packet for receiving ! >\n"));
           return XRTP_EMEM;
        }

        if(ses->$callbacks.sync_timestamp)
           ses->$callbacks.sync_timestamp(ses->$callbacks.sync_timestamp_user, &hi_ntp, &lo_ntp, &hrt_local);

        else if(ses->$callbacks.rtp_timestamp)
           ses->$callbacks.rtp_timestamp(ses->$callbacks.rtp_timestamp_user, &hrt_local);
       
        rtcp_set_sender_info(rtcp, self->ssrc, hi_ntp, lo_ntp,
                            session_hrt2ts(ses, hrt_local), self->n_rtp_send, self->n_payload_oct_send);

        /* Calculate last lsr_delay(sender received delay) */
        lsr_delay = ((hrt_now - self->lsr_hrt) % HRTIME_SECOND_DIVISOR) & 0xFF;
        lsr_delay += ((lrt_now - self->lsr_lrt) / LRTIME_SECOND_DIVISOR) << 16;
        
    }else{

        rtcp = rtcp_receiver_report_new(ses, self->ssrc, SESSION_DEFAULT_RTCP_NPACKET, XRTP_YES);
    }

    /* Bye not implemented */

    max_report = XRTP_MAX_REPORTS > ses->n_member ? ses->n_member : XRTP_MAX_REPORTS;
    /* report self */
    session_member_make_report(self, &frac_lost, &total_lost, &exseqno);

    /*
    session_log("session_rtcp_to_send: self->ssrc = %u\n", self->ssrc);
    session_log("session_rtcp_to_send: frac_lost = %u\n", frac_lost);
    session_log("session_rtcp_to_send: total_lost = %u\n", total_lost);
    session_log("session_rtcp_to_send: exseqno = %u\n", exseqno);
    session_log("session_rtcp_to_send: self->jitter = %f\n", self->jitter);
    session_log("session_rtcp_to_send: self->lsr_ts = %u\n", self->lsr_ts);
    session_log("session_rtcp_to_send: lsr_delay = %u\n", lsr_delay);
     */
     
    rtcp_add_report(rtcp, self->ssrc, frac_lost, total_lost,
                    exseqno, self->jitter, self->lsr_ts, lsr_delay);

    max_report--;
                            
    /* Add SDES packet */
    if(self->cname){

       session_log(("session_rtcp_to_send: Add CNAME '%s'\n", self->cname));
       rtcp_add_sdes(rtcp, self->ssrc, RTCP_SDES_CNAME, self->cname_len, self->cname);
       rtcp_end_sdes(rtcp, self->ssrc);
    }
    
    /* Bye not implemented */

    /* report of members */
    ses->n_rtcp_out = 0;

    if(ses->n_member > 1){
      
        session_log(("session_rtcp_to_send: %d member\n", ses->n_member));
        
        /* Add more report if more valid members other than self */
        uint32 start_report_ssrc = 0;

        if(ses->next_report_ssrc == 0){

            mem = session_next_valid_member_of(ses, self->ssrc);
            ses->next_report_ssrc = mem->ssrc;
            
        }else{

            /* !mem MUST NOT happen if member is left, this should avoid
             * FIXME: always update ses->next_report_ssrc whenever member left
             */
            mem = session_member_state(ses, ses->next_report_ssrc);
            if(mem == self){    

                /* Actually, this never happen, as next_report_ssrc never record self */
                session_log(("session_rtcp_to_send: skip Self[%u]\n", self->ssrc));
                mem = session_next_valid_member_of(ses, self->ssrc);
            }
        }
        
        start_report_ssrc = ses->next_report_ssrc;
        session_log(("session_rtcp_to_send: report Member[%u]\n", mem->ssrc));
    
        int report_end = 0;

        while(!report_end && max_report > 0){

          /* report other participants */
          session_member_make_report(mem, &frac_lost, &total_lost, &exseqno);

          /* Calculate last lsr_delay(sender received delay) */
          if(mem->we_sent){
         
              lsr_delay = ((hrt_now - mem->lsr_hrt) % HRTIME_SECOND_DIVISOR) & 0xFF;
              lsr_delay += ((lrt_now - mem->lsr_lrt) / LRTIME_SECOND_DIVISOR) << 16;

          }else{
         
              lsr_delay = 0;
          }
       
          if(rtcp_add_report(rtcp, mem->ssrc, frac_lost, total_lost,
                          exseqno, mem->jitter, mem->lsr_ts, lsr_delay) == XRTP_EREFUSE){

              report_end = 1;
          
          }else{

              max_report--;
          }
       
          /* Which connect send to for this member */
          ses->outgoing_rtcp[ses->n_rtcp_out++] = mem->rtcp_connect;
          session_log(("session_rtcp_to_send: rtcp send to connect_io[%d]\n", connect_io(mem->rtcp_connect)));
          
          mem = session_next_valid_member_of(ses, mem->ssrc);
          if(mem == self){

             session_log(("session_rtcp_to_send: skip Self[%u]\n", self->ssrc));
             mem = session_next_valid_member_of(ses, self->ssrc);
          }

          session_log(("session_rtcp_to_send: report Member[%u]\n", mem->ssrc));
          
          if(mem->ssrc == start_report_ssrc){

              session_log(("session_rtcp_to_send: reports send to %d connects\n", ses->n_rtcp_out));
              break;
          }
       }
       
        /* Record next report member */
        ses->next_report_ssrc = mem->ssrc;
    }

    /* Send SDES of member */
    if(!rtcp_end){
       /* Add SDES packet */
    }
    
    session_log(("session_rtcp_to_send: pump ses->rtcp_send_pipe[@%u]\n", (int)(ses->rtcp_send_pipe)));
    int rtcp_bytes;
    if(pipe_pump(ses->rtcp_send_pipe, 0, rtcp, &rtcp_bytes) >= XRTP_OK){

        session_log(("session_rtcp_to_send: produce successfully, the final rtcp packet is %d bytes\n", rtcp_bytes));
        session_rtcp_send_now(ses, rtcp);
        ses->rtcp_init = 0;
        rtcp_compound_done(rtcp);
        
    }else{

        session_log(("< session_rtcp_to_send: Fail to produce the rtcp packet >\n"));
        rtcp_compound_done(rtcp);
        
        return XRTP_FAIL;
    }

    return XRTP_OK;
 }

 int session_rtcp_to_receive(xrtp_session_t *ses){

    xrtp_rtcp_compound_t * rtcp;

    char *pdata = NULL;
    int datalen;

    xrtp_buffer_t * buf;

    xrtp_hrtime_t hrts_arrival;

    xrtp_lrtime_t lrts_arrival;

    datalen = connect_receive(ses->rtcp_incoming, &pdata, 0, &hrts_arrival, &lrts_arrival);

    session_log(("session_rtcp_to_receive: Prepare to receive RTCP Packet from connect_io[%d]\n", connect_io(ses->rtcp_incoming)));

    /* New RTCP Packet, 0 packet means default implementation depends value */
    rtcp = rtcp_new_compound(ses, 0, RTP_RECEIVE, ses->rtcp_incoming, hrts_arrival, lrts_arrival);
    if(!rtcp){

       session_log(("< session_rtcp_to_receive: Can't create rtp packet for receiving >\n\n"));
       return XRTP_EMEM;
    }

    buf = buffer_new(0, NET_BYTEORDER);
    buffer_mount(buf, pdata, datalen);

    rtcp_set_buffer(rtcp, buf);
    rtcp->bytes_received = datalen;

    int rtcp_bytes;
    if(pipe_pump(ses->rtcp_recv_pipe, 0, rtcp, &rtcp_bytes) != XRTP_CONSUMED){

       rtcp_compound_done(rtcp);
    }

    return XRTP_OK;
 }


 /**
  * Need a scheduler to schedule the packet piping according timer
  */
 int session_set_scheduler(xrtp_session_t *ses, session_sched_t *sched){

    if(ses->sched == sched)
       return XRTP_OK;
       
    if(ses->sched){

       sched->remove(ses->sched, ses);
    }

    ses->sched = sched;
    
    if (sched) sched->add(sched, ses);
    
    return XRTP_OK;
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
 int session_rtp_arrived(xrtp_session_t * ses, session_connect_t * conn){

    ses->rtp_incoming = conn;

    return XRTP_OK;
 }

 /* Notify the session a rtcp arrival and schedule the process */
 int session_rtcp_arrival_notified(xrtp_session_t * ses, session_connect_t * conn, xrtp_lrtime_t lrts_arrival){

    ses->rtcp_incoming = conn;

    /*
    session_log("session_rtcp_arrival: rtcp arrived at lrtime[%u]\n", lrts_arrival);
    */
    ses->sched->rtcp_in(ses->sched, ses, lrts_arrival);

    return XRTP_OK;
 }
