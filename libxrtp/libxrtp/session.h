/*
 * Copyright (C) 2002-2003 the Timedia project
 *
 * This file is part of xrtp, a extendable rtp stack.
 *
 * xrtp is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xrtp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: session.h,v 0.1 15/12/2002 15:47:39 heming$
 *
 */

 #ifndef XRTP_SESSION_H

 #define XRTP_SESSION_H
 
 #define MEMBER_MAXACTIVE 2
 #define XRTP_MAX_MTU_BYTES 65536
 
 typedef struct xrtp_session_s xrtp_session_t;

#include <timedia/timer.h>
#include <timedia/spinqueue.h>
#include <timedia/xthread.h>
#include <timedia/list.h>
#include <timedia/socket.h>
#include <timedia/catalog.h>
 
#include "pipeline.h"
#include "psched.h"
#include "const.h"

#define RTCP_SENDER_TIMEOUT_MULTIPLIER  2
#define RTCP_MEMBER_TIMEOUT_MULTIPLIER  5

#define RTCP_MAX_SEQNO_DIST  4096    /* Very rough guess */

#define PLAYMODE_NORMAL  0
#define PLAYMODE_SPIKE  1

#define IN_SPIKE  3  /* Make sure we are stay away from a spike */

#define CONSECUTIVE_DROP_THRESHOLD  3
 
#define SPIKE_THRESHOLD  375   /* Refer to RTP(Collin Perkins) Page 189 */

#define INACTIVE_THRESHOLD  200  /* 2000 millisecond */

#define RTCP_MIN_INTERVAL  500

#define RTP_DELAY_FACTOR  3  /* FIXME: 3 times period, need more consider */
#define RTP_MAX_PACKET_DELAY 5
 
typedef enum{
   
    SESSION_SEND,
    SESSION_RECV,
    SESSION_DUPLEX

} session_mode_t;

#define MIN_SEQUENTIAL  3

typedef struct member_state_s{

    xthr_lock_t * lock;

    xrtp_session_t * session;

    uint32 ssrc;  /* Note: value 0 is NOT a valid ssrc! */
    int active;
    int valid;
    int in_collision;

    session_connect_t * rtp_connect;
    session_connect_t * rtcp_connect;

    uint16 max_seq;
    uint32 cycles;
    uint32 base_seq;
    uint32 bad_seq;
    uint32 probation;
    
    uint32 received;  /* number of rtp received by now */
    
    uint32 expected_prior;
    uint32 received_prior;
    
    xrtp_hrtime_t last_hrt_local;
    xrtp_hrtime_t last_hrt_rtpts;

    uint32 last_transit;       /* For playout time computing, session_local_time() */
    uint32 last_last_transit;
    uint32 min_transit;

    int32 delay_estimate;
    int32 active_delay;

    int in_spike;
    double spike_var;
    double spike_end;

    int consecutive_dropped;
    
    int playout_adapt;
    int play_mode;

    xrtp_hrtime_t last_playout_offset;
    xrtp_lrtime_t lrts_last_heard;

    double jitter;

    int we_sent;    /* we are a sender */

    uint n_rtp_received;
    uint n_rtp_received_prior;
    uint n_payload_oct_received;  /* payload octet counter */

    uint n_rtp_send;          /* reset when changed ssrc */
    uint n_payload_oct_send;  /* payload octet counter, reset when changed ssrc */

    /* rtcp report elements: */
    
    /* uint8 frac_lost;    calc on_fly
     * uint32 total_lost;  calc on_fly
     */
    uint32 lsr_ts; 
    xrtp_hrtime_t lsr_hrt;   /* time of last sr received */
    xrtp_lrtime_t lsr_lrt;   /* time of last sr received */
    
    xrtp_list_t * rtp_buffer; /* Playout buffer */

    void * user_info;    /* Extenal info given by the session user */

    xrtp_lrtime_t lrt_last_rtcp_sent;
    
    xrtp_lrtime_t lrts_last_rtp_sent;
    xrtp_hrtime_t hrts_last_rtp_sent;

    char * cname;
    int cname_len;

} member_state_t;

typedef struct session_state_s{

	 /* aka payload_type */
   int profile_no;
   
   int receive_from_anonymous;
       
	 int n_pack_recvd;
	 int n_pack_sent;

   xrtp_hrtime_t hrts_start_send;
   xrtp_lrtime_t lrts_start_send;
    
	 int oct_recvd;
	 int oct_sent;
    
	 int jitter;

} session_state_t;

struct session_lock_s{

   void* send_waiting_lock;
	 void* recv_waiting_lock;
};

struct session_cond_s{

   void* send_waiting_cond;
	 void* recv_waiting_cond;
};

typedef struct appinfo_s{
    
   uint32 name;
   uint32 len;
   char * info;
    
} xrtp_appinfo_t;
 
typedef struct session_call_s{

   int id;
   void * fn;
   void * user;
   
} session_call_t;

struct session_callbacks_s{

    #define CALLBACK_SESSION_MEDIA_SENT       0x0
    #define CALLBACK_SESSION_MEDIA_RECVD      0x1
    #define CALLBACK_SESSION_APPINFO_RECVD    0x2
    #define CALLBACK_SESSION_MEMBER_APPLY     0x3
    #define CALLBACK_SESSION_MEMBER_UPDATE    0x4
    #define CALLBACK_SESSION_MEMBER_CONNECTS  0x5
    #define CALLBACK_SESSION_MEMBER_DELETED   0x6
    #define CALLBACK_SESSION_MEMBER_REPORT    0x7
    #define CALLBACK_SESSION_BEFORE_RESET     0x8
    #define CALLBACK_SESSION_AFTER_RESET      0x9
    #define CALLBACK_SESSION_NEED_SIGNATURE   0xA
    #define CALLBACK_SESSION_NOTIFY_DELAY     0xB
    #define CALLBACK_SESSION_SYNC_TIMESTAMP   0xC
    #define CALLBACK_SESSION_RESYNC_TIMESTAMP 0xD
    #define CALLBACK_SESSION_RTP_TIMESTAMP    0xE
    #define CALLBACK_SESSION_NTP_TIMESTAMP    0xF
    #define CALLBACK_SESSION_HRT2MT           0x10
    #define CALLBACK_SESSION_MT2HRT           0x11
    
    void * default_user;

    /* Callout type: CALLBACK_SESSION_MEDIA_SENT */
    int (*media_sent)(void* user, xrtp_media_t *media);
    void * media_sent_user;

    /* Callout type: CALLBACK_SESSION_MEDIA_RECVD */
    int (*media_recieved)(void* user, char *data, int dlen, uint32 ssrc, media_time_t mts, media_time_t dur);
    void * media_recieved_user;

    /* Callout type: CALLBACK_SESSION_APPINFO_RECVD */
    int (*appinfo_recieved)(void*user, xrtp_appinfo_t *appinfo);
    void * appinfo_recieved_user;
    
    /**
     * Callout type: CALLBACK_SESSION_MEMBER_APPLY
     * This leave application to deside add a member or not
     */
    int (*member_apply)(void* user, uint32 member_src, char * cname, int cname_len, void ** user_info);
    void * member_apply_user;

    /**
     * Callout type: CALLBACK_SESSION_MEMBER_UPDATE
     * Update member identification
     */
    int (*member_update)(void* user, uint32 member_src, char * cname, int cname_len);
    void * member_update_user;

    /**
     * Callout type: CALLBACK_SESSION_MEMBER_CONNECTS
     * Ask the connects to the member ports, which is preset by SIP protocol
     * If not set, use OLD RTP static ports allocation assumption, which is rtcp is ONE up the rtp port.
     */
    int (*member_connects)(void* user, uint32 member_src, session_connect_t **rtp_conn, session_connect_t **rtcp_conn);
    void * member_connects_user;

    /**
     * Callout type: CALLBACK_SESSION_MEMBER_DELETED
     * The extra info return to the user.
     */
    int (*member_deleted)(void* user, uint32 member_src, void * user_info);
    void * member_deleted_user;

    /**
     * Callout type: CALLBACK_SESSION_MEMBER_REPORT
     * Report the transission condition of the member
     * may be judged to adapt better media data
     */
    int (*member_report)(void* user, void* member_info, uint8 frac_lost, uint32 intv_lost, uint32 jitter, uint32 rtt);
    void * member_report_user;

    /**
     * Callout type: CALLBACK_SESSION_RECEIVE_RTP
     * Preempty current sending by a receive event
    int (*receive_rtp)(void* user, xrtp_hrtime_t ts);
    void * receive_rtp_user;
     */

    /**
     * Callout type: CALLBACK_SESSION_RESUME_SEND_RTP
     * After receiving, resume the sending
    int (*resume_send_rtp)(void* user, xrtp_hrtime_t now);
    void * resume_send_rtp_user;
     */
    
    /**
     * Callout type: CALLBACK_SESSION_BEFORE_RESET
     * Notify right before the session reset
     */
    int (*before_reset)(void* user, xrtp_session_t * session);
    void * before_reset_user;

    /**
     * Callout type: CALLBACK_SESSION_AFTER_RESET
     * Notify right after the session reset
     */
    int (*after_reset)(void* user, xrtp_session_t * session);
    void * after_reset_user;

    /**
     * Callout type: CALLBACK_SESSION_NEED_SIGNATURE
     * Recreate a signature of the session
     */
    uint32 (*need_signature)(void* user, xrtp_session_t * session);
    void * need_signature_user;

    /**
     * Callout type: CALLBACK_SESSION_NOTIFY_DELAY
     * Tell the user how late we are
     */
    int (*notify_delay)(void* user, xrtp_session_t * session, xrtp_hrtime_t late);
    void * notify_delay_user;

    /**
     * Callout type: CALLBACK_SESSION_SYNC_TIMESTAMP
     * Get NTP and Local timestamp
     */
    int (*sync_timestamp)(void* user, uint32 * hi_ntp, uint32 * lo_ntp, xrtp_hrtime_t * hrts_local);
    void * sync_timestamp_user;

    /**
     * Callout type: CALLBACK_SESSION_RESYNC_TIMESTAMP
     * Sync Remote NTP and Local timestamp
     */
    int (*resync_timestamp)(void* user, xrtp_session_t * session, uint32 hi_ntp, uint32 lo_ntp, xrtp_hrtime_t hrts_local);
    void * resync_timestamp_user;

    /**
     * Callout type: CALLBACK_SESSION_RTP_TIMESTAMP
     * Get RTP timestamp
     */
    int (*rtp_timestamp)(void* user, xrtp_hrtime_t * rtp_ts);
    void * rtp_timestamp_user;

    /**
     * Callout type: CALLBACK_SESSION_NTP_TIMESTAMP
     * Get NTP timestamp
     */
    int (*ntp_timestamp)(void* user, uint32 * hi_ntp, uint32 * lo_ntp);
    void * ntp_timestamp_user;

    /**
     * Callout type: CALLBACK_SESSION_HRT2MT
     */
    media_time_t (*hrtime_to_mediatime)(void* user, xrtp_hrtime_t hrt);
    void * hrtime_to_mediatime_user;

    /**
     * Callout type: CALLBACK_SESSION_MT2HRT
     */
    xrtp_hrtime_t (*mediatime_to_hrtime)(void* user, media_time_t mt);
    void * mediatime_to_hrtime_user;
};

typedef struct param_members_s{

    uint32 * srcs;
    uint32 n_src;

} param_member_t;

struct xrtp_session_s {

    int id;

    module_catalog_t * module_catalog;

    /* NOTE: The Redundancy is not implemented yet */
    xrtp_media_t * media;
    profile_handler_t* media_handler;

    session_mode_t mode_trans;

    packet_pipe_t *rtp_send_pipe;
	  packet_pipe_t *rtcp_send_pipe;
    
    uint n_rtcp_recv;

    uint rtcp_avg_size;

    packet_pipe_t *rtp_recv_pipe;
	  packet_pipe_t *rtcp_recv_pipe;
	  uint32 recv_seq;

	  xrtp_clock_t * clock;

    uint  bandwidth;
    uint  rtp_bw;     /* RTP bandwidth */
    uint  rtcp_bw;    /* RTCP bandwidth */
    int rtp_bw_left;  /* unused bandwidth each sending */

    xrtp_hrtime_t hrts_last_rtp_sent;

    rtime_t period;
    rtime_t rtcp_interval;
    
    session_sched_t *sched;

    int bye_mode; /* if in BYE Reconsideration */

    member_state_t * self;

    uint n_sender;
    xrtp_list_t * senders;
    
    uint n_member;
    xrtp_list_t * members;
    struct xrtp_list_user_s $member_list_block;

    int  rtcp_init;   /* if rtcp is sent ever */

    xrtp_hrtime_t tp, tc, tn; /* RTCP transition time (RFC3550 6.3) */

    uint pn_member;    /* pmember */
    
    xrtp_port_t * rtp_port;
    xrtp_port_t * rtcp_port;

    session_connect_t * rtp_incoming;
    session_connect_t * rtcp_incoming;

    session_connect_t * outgoing_rtcp[XRTP_MAX_REPORTS]; /* Only send to member on the report once a time */
    int n_rtcp_out;     /* How many rtcp send once */
    
    struct param_members_s param_members;
    
    uint32 next_report_ssrc;

    uint32 timestamp;

    xrtp_hrtime_t rtp_cancel_ts;
    int rtp_cancelled;

    sched_schedinfo_t * schedinfo;

    xrtp_teleport_t * join_to_rtp_port;
    xrtp_teleport_t * join_to_rtcp_port;

	  struct session_state_s $state;
	  struct session_callbacks_s $callbacks;
	  struct session_lock_s $locks;
	  struct session_cond_s $condvars;

    spin_queue_t * packets_in_sched;
    xrtp_hrtime_t timesize;

    xrtp_thread_t * thr_rtp_recv;
    xthr_lock_t * rtp_recv_lock;
    int thread_run;
};

/* Interface of the Session class */

/**
 * Create a new Session
 */
extern DECLSPEC
xrtp_session_t * 
session_new(xrtp_port_t *rtp_port, xrtp_port_t *rtcp_port, char * cname, int clen, module_catalog_t * cata);

/**
 * Release the Session
 */
extern DECLSPEC
int
session_done(xrtp_session_t * session);

extern DECLSPEC
int
session_set_id(xrtp_session_t * session, int id);

int 
session_id(xrtp_session_t * session);

int session_cname(xrtp_session_t * session, char * cname, int clen);

/* set if an anonymous participant allowed in the session */
extern DECLSPEC
int 
session_allow_anonymous(xrtp_session_t * session, int allow);

/**
 *  From now on, session can receive incoming data
 */
extern DECLSPEC
int
session_start_receipt(xrtp_session_t * session);
 
/**
 *  From now on, session stop receiving incoming data
 */
int session_stop_receipt(xrtp_session_t * session);

/**
 * Get session rtp and rtcp ports
 */
int session_ports(xrtp_session_t * session, xrtp_port_t **r_rtp_port, xrtp_port_t **r_rtcp_port);

/**
 * Set mode of the Session, which could be
 *
 * 'SESSION_SEND', 'SESSION_RECV' or 'SESSION_DUPLEX'
 */
int session_set_mode(xrtp_session_t * session, int mode);
uint session_mode(xrtp_session_t * session);
 
/**
 * Retrieve a Media handler associated with the session and allocate a payload type to
 * this session to packet transfer. so send/receive packets with this pt.
 */
extern DECLSPEC
xrtp_media_t * 
session_new_media(xrtp_session_t * ses, char * id, uint8 payload_type);

/**
 * The middle process b/w import and export, order in addition
 * Can be compress module and crypo module, number of module less than MAX_PIPE_STEP
 */
extern DECLSPEC
profile_handler_t * 
session_add_handler(xrtp_session_t * session, char * id);

/**
 * Get the packet process of the session
 * param type:
 *   RTP_SEND
 *   RTCP_SEND
 *   RTP_RECEIVE
 *   RTCP_RECEIVE
 */
#define RTP_SEND 0
#define RTCP_SEND 1
#define RTP_RECEIVE 2
#define RTCP_RECEIVE 3
packet_pipe_t * session_process(xrtp_session_t * session, int type);
 
/**
 * Reserved function
 int session_enable_process(xrtp_session_t * session, char * id, int enable); 
 */
  
uint32 session_ssrc(xrtp_session_t * session);

/**
 * Set bandwidth for this session, REMEMBER: multiuser need share the bandwidth!!!
 */
extern DECLSPEC
int 
session_set_bandwidth(xrtp_session_t * session, int32 total_bw, int32 rtp_bw);

uint32 session_rtp_bandwidth(xrtp_session_t * session);
 
/* Depend on the number of member; Multicast or Unicast; Media period 
uint32 session_rtp_bandwidth_budget(xrtp_session_t * session);
 */
  
/**
 * To join to a remote connection destinate, this is a chance to
 * contact a new participant of the session.
 */
extern DECLSPEC
int 
session_join(xrtp_session_t * session, xrtp_teleport_t * rtp_port, xrtp_teleport_t * rtcp_port);

/**
 * Session memeber management:
 * session_add_member for adding new coming member
 * session_set_member for setting all member, some left member can be removed in this way.
 */
extern DECLSPEC
member_state_t * 
session_new_member(xrtp_session_t * session, uint32 src, void * extra_info);

int session_active_members(xrtp_session_t * session, uint32 srcs[], uint n_src);

extern DECLSPEC
member_state_t * 
session_member_state(xrtp_session_t * session, uint32 member);

extern DECLSPEC
int 
session_member_check_senderinfo(member_state_t * member,
                                uint32 hi_ntp, uint32 lo_ntp, uint32 rtp_ts,
                                uint32 packet_sent, uint32 octet_sent);

extern DECLSPEC
int 
session_member_check_report(member_state_t * member, uint8 frac_lost, uint32 total_lost,
                                 uint32 full_seqno, uint32 jitter,
                                 uint32 lsr_stamp, uint32 lsr_delay);

extern DECLSPEC
member_state_t * 
session_update_member_by_rtcp(xrtp_session_t * session, xrtp_rtcp_compound_t * rtcp);

extern DECLSPEC
int 
session_member_set_connects(member_state_t * member, session_connect_t * rtp_conn, session_connect_t * rtcp_conn);
int session_member_connects(member_state_t * member, session_connect_t **rtp_conn, session_connect_t **rtcp_conn);

int session_quit(xrtp_session_t * session, int silently);

extern DECLSPEC
int 
session_member_update_rtp(member_state_t * mem, xrtp_rtp_packet_t * rtp);

extern DECLSPEC
int 
session_member_hold_rtp(member_state_t * member, xrtp_rtp_packet_t * rtp);
 
extern DECLSPEC
xrtp_rtp_packet_t*
session_member_next_rtp_withhold(member_state_t * member);
 
media_time_t session_hrt2mt(xrtp_session_t * session, xrtp_hrtime_t hrt);
xrtp_hrtime_t session_mt2hrt(xrtp_session_t * session, media_time_t mt);

extern DECLSPEC
xrtp_hrtime_t 
session_member_mapto_local_time(member_state_t * member, xrtp_rtp_packet_t * rtp);

uint32
session_signature(xrtp_session_t * session);

extern DECLSPEC
int
session_solve_collision(member_state_t * member, uint32 ssrc);

/* Notify the session a rtp arrival and schedule the process */
int session_rtp_arrived(xrtp_session_t * session, session_connect_t * connect);

/* Notify the session a rtcp arrival and schedule the process */
int session_rtcp_arrival_notified(xrtp_session_t * session, session_connect_t * connect, xrtp_lrtime_t ts_arrival);

/**
 * Update seqno according the seq provided.
 *
 * return value: 1 if the seqno is a valid number, otherwise return 0.
 */
extern DECLSPEC
int 
session_update_seqno(member_state_t * mem, uint16 seqno);

uint32 session_member_max_exseqno(member_state_t * mem);

int session_nmember(xrtp_session_t * session);
int session_nsender(xrtp_session_t * session);

extern DECLSPEC
int 
session_set_rtp_rate(xrtp_session_t *session, int rate);

rtime_t session_rtp_period(xrtp_session_t *session);
/* How long is allow to delay the rtp sending, used as a sched window */
xrtp_hrtime_t session_rtp_delay(xrtp_session_t *session);
 
xrtp_lrtime_t session_rtcp_interval(xrtp_session_t *session);
/* How long is allow to delay the rtcp sending, used as a sched window */
xrtp_lrtime_t session_rtcp_delay(xrtp_session_t *session);
 
extern DECLSPEC
int 
session_count_rtcp(xrtp_session_t * ses, xrtp_rtcp_compound_t * rtcp);
 
int session_notify_delay(xrtp_session_t * session, xrtp_hrtime_t howlate);
 
/**
 * Set callback of some event arised.
 */
extern DECLSPEC
int
session_set_callback(xrtp_session_t *session, int type, void* callback, void* user);

int session_set_callbacks(xrtp_session_t *session, session_call_t cbs[], int n);

/**
 * send rtp packet to send pipeline
 * return the packet size after the pipeline
 */
extern DECLSPEC
int 
session_rtp_to_send(xrtp_session_t *session, xrtp_hrtime_t ts, int last);
int session_cancel_rtp_sending(xrtp_session_t *session, xrtp_hrtime_t ts);
 
/**
 * Reached schedule time, send data immidiately, called by scheduler
 * budget: The bandwidth available for sending
 * nts: The completed frame number(not just packets) sent is filled in.
 *
 * return the byte sent, to adjust the current budget.
 */
int session_rtp_send_now(xrtp_session_t *session);
 
int session_rtp_to_receive(xrtp_session_t *ses);
/**
 * Check if the rtp packet is a cancelled one
 */
int session_rtp_receiving_cancelled(xrtp_session_t * session, xrtp_hrtime_t ts);

/**
 * send rtcp packet to pipeline
 * return the packet origional size
 */
int session_need_rtcp(xrtp_session_t *session);
 
int session_rtcp_to_send(xrtp_session_t *session);

int session_rtcp_to_receive(xrtp_session_t *session);

/**
 * Need a scheduler to schedule the packet piping according timer
 */
extern DECLSPEC
int 
session_set_scheduler(xrtp_session_t *session, session_sched_t *sched);
 
int session_set_schedinfo(xrtp_session_t *session, sched_schedinfo_t * si);
sched_schedinfo_t * session_schedinfo(xrtp_session_t * session);

#endif
