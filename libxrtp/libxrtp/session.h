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
#include "xrtp.h"

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

#define MAX_IPADDR_BYTES 64
 
typedef enum
{
    SESSION_SEND,
    SESSION_RECV,
    SESSION_DUPLEX

} session_mode_t;

#define MIN_SEQUENTIAL  3

typedef struct member_state_s
{
    xthr_lock_t * lock;

    xrtp_session_t * session;

    uint32 ssrc;  /* Note: value 0 is NOT a valid ssrc! */
    int active;
    int valid;
    int in_collision;

    session_connect_t *rtp_connect;
    session_connect_t *rtcp_connect;

	/* to verify the incoming packet, if member setup by sip or other external protocol */
    xrtp_teleport_t *rtp_port;
    xrtp_teleport_t *rtcp_port;

    uint16 max_seq;
    uint32 cycles;
    uint32 base_seq;
    uint32 bad_seq;
    uint32 probation;
    
    uint n_payload_oct_received;  /* payload octet counter */
    uint n_rtp_received;  /* number of rtp received by now */
    uint n_payload_oct_sent;
    uint n_rtp_sent;

    uint n_rtp_received_prior;
    uint32 n_rtp_expected_prior;


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

    rtime_t last_playout_offset;

	/* latest heart beat */
    rtime_t msec_last_heard;

    double jitter;

    int we_sent;    /* we are a sender */

    /* rtcp report elements: */
    uint32 lsr_ts; 
    rtime_t lsr_usec;   /* time of last sr received */
    rtime_t lsr_msec;   /* time of last sr received */
    
    void * user_info;    /* Extenal info given by the session user */

    rtime_t lrt_last_rtcp_sent;
    
    rtime_t msec_last_rtp_sent;
    rtime_t usec_last_rtp_sent;

    char * cname;
    int cname_len;

    /* media info block */
	int		media_playable;  /* eg: after setup by 3 vorbis header packet */
	uint32  rtpts_minfo;
	int		minfo_signum;

	void*	mediainfo;
	void*	media_player;

    /* Playout buffer */
	xlist_t *delivery_buffer;

	xthr_lock_t *delivery_lock;
	xthr_cond_t *wait_media_available;
	xthr_cond_t *wait_seqno;

	int stop_delivery;

	uint16 expect_seqno;
	int misorder;

	uint32 timestamp_playing;
	rtime_t msec_playing;
	rtime_t usec_playing;
	rtime_t nsec_playing;

	xthr_lock_t *sync_lock;
	uint32 timestamp_remote_sync;
	uint32 timestamp_sync;
	rtime_t msec_sync;
	rtime_t usec_sync;
	rtime_t nsec_sync;

	uint32 timestamp_last_sync;
	rtime_t msec_last_sync;
	rtime_t usec_last_sync;
	rtime_t nsec_last_sync;

	int64 packetno;
	uint32 rtpts_last_payload;
	int64 samples;

	xthread_t *deliver_thread;

} member_state_t;

typedef struct session_state_s
{
	/* aka payload_type */
	int profile_no;
	char *profile_type;
   
	int receive_from_anonymous;
       
	int n_pack_recvd; 
	int n_pack_sent;  /* reset when changed ssrc */

	rtime_t usec_start_send;
	xrtp_lrtime_t lrts_start_send;
    
	int oct_recvd;  
	int oct_sent;  /* payload octet counter, reset when changed ssrc */
    
	int jitter;

} session_state_t;

struct session_lock_s
{
	void* send_waiting_lock;
	void* recv_waiting_lock;
};

struct session_cond_s
{
   void* send_waiting_cond;
   void* recv_waiting_cond;
};

typedef struct appinfo_s
{
   uint32 name;
   uint32 len;
   char * info;
    
} xrtp_appinfo_t;
 
typedef struct session_call_s
{
   int id;
   void * fn;
   void * user;
   
} session_call_t;

struct session_callbacks_s
{
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
    #define CALLBACK_SESSION_SYNC			  0xC
    #define CALLBACK_SESSION_NTP			  0xD
    
    void * default_user;

    /* Callout type: CALLBACK_SESSION_MEDIA_SENT */
    void *media_sent_user;
    int (*media_sent)(void* user, xrtp_media_t *media);

    /* Callout type: CALLBACK_SESSION_MEDIA_RECVD */
    void *media_recieved_user;
    int (*media_recieved)(void* user, char *data, int dlen, uint32 ssrc, media_time_t mts, media_time_t dur);

    /* Callout type: CALLBACK_SESSION_APPINFO_RECVD */
    void *appinfo_recieved_user;
    int (*appinfo_recieved)(void*user, xrtp_appinfo_t *appinfo);
    
    /**
     * Callout type: CALLBACK_SESSION_MEMBER_APPLY
     * This leave application to deside add a member or not
     */
    void *member_apply_user;
    int (*member_apply)(void* user, uint32 member_src, char * cname, int cname_len, void ** user_info);

    /**
     * Callout type: CALLBACK_SESSION_MEMBER_UPDATE
     * Update member identification
     */
    void *member_update_user;
    int (*member_update)(void* user, uint32 member_src, char * cname, int cname_len);

    /**
     * Callout type: CALLBACK_SESSION_MEMBER_CONNECTS
     * Ask the connects to the member ports, which is preset by SIP protocol
     * If not set, use OLD RTP static ports allocation assumption, which is rtcp is ONE up the rtp port.
     */
    void *member_connects_user;
    int (*member_connects)(void* user, uint32 member_src, session_connect_t **rtp_conn, session_connect_t **rtcp_conn);

    /**
     * Callout type: CALLBACK_SESSION_MEMBER_DELETED
     * The extra info return to the user.
     */
    void *member_deleted_user;
    int (*member_deleted)(void* user, uint32 member_src, void * user_info);

    /**
     * Callout type: CALLBACK_SESSION_MEMBER_REPORT
     * Report the transission condition of the member
     * may be judged to adapt better media data
     */
    void *member_report_user;
    int (*member_report)(void* user, void* member_info, 
						 uint32 ssrc, char *cname, int cnlen,
						 uint npack_recv, uint bytes_recv, 
						 uint8 frac_lost, uint32 intv_lost, 
						 uint32 jitter, uint32 rtt);

    /**
     * Callout type: CALLBACK_SESSION_BEFORE_RESET
     * Notify right before the session reset
     */
    void *before_reset_user;
    int (*before_reset)(void* user, xrtp_session_t * session);

    /**
     * Callout type: CALLBACK_SESSION_AFTER_RESET
     * Notify right after the session reset
     */
    void *after_reset_user;
    int (*after_reset)(void* user, xrtp_session_t * session);

    /**
     * Callout type: CALLBACK_SESSION_NEED_SIGNATURE
     * Recreate a signature of the session
     */
    void *need_signature_user;
    uint32 (*need_signature)(void* user, xrtp_session_t * session);

    /**
     * Callout type: CALLBACK_SESSION_NOTIFY_DELAY
     * Tell the user how late we are
     */
    void *notify_delay_user;
    int (*notify_delay)(void* user, xrtp_session_t * session, rtime_t late);

    /**
     * Callout type: CALLBACK_SESSION_NTP
     * Get NTP timestamp
     */
    void *ntp_user;
    int (*ntp)(void* user, uint32 * hi_ntp, uint32 * lo_ntp);
    /**
     * Callout type: CALLBACK_SESSION_SYNC
     * Get NTP timestamp
     */
    void *synchronise_user;
    int (*synchronise)(void* user, uint32 timestamp, uint32 hi_ntp, uint32 lo_ntp);
};

typedef struct param_members_s
{
    uint32 * srcs;
    uint32 n_src;

} param_member_t;

struct xrtp_session_s
{
   int id;

   module_catalog_t * module_catalog;
	void *media_control;

   /* NOTE: The Redundancy is not implemented yet */
   xrtp_media_t * media;
   profile_handler_t* media_handler;

   session_mode_t mode_trans;

   packet_pipe_t *rtp_send_pipe;
	packet_pipe_t *rtcp_send_pipe;
    
   uint n_rtcp_recv;

   int rtcp_avg_size;

   packet_pipe_t *rtp_recv_pipe;
	packet_pipe_t *rtcp_recv_pipe;
	uint32 recv_seq;

	xrtp_clock_t * clock;

   int  bandwidth;			/* Total bandwidth */
   int  bandwidth_rtp;     /* RTP bandwidth */

	int  bandwidth_budget;
   int	 bandwidth_rtp_budget;  /* unused bandwidth each sending */
   int  bandwidth_rtcp_budget;	/* RTCP bandwidth */

	xthr_lock_t * bandwidth_lock;

   rtime_t hrts_last_rtp_sent;

   rtime_t usec_period;
   rtime_t msec_rtcp_interval;
    
   session_sched_t *sched;

   int bye_mode; /* if in BYE Reconsideration */

   member_state_t * self;

   uint n_sender;
   xlist_t *senders;
	xthr_lock_t * senders_lock;
    
   uint n_member; /* the number of validated member */
   xlist_t *members;
	xthr_lock_t *members_lock;

	int renew_minfo_level;
	xthr_lock_t *renew_level_lock;

   struct xrtp_list_user_s $member_list_block;

   int  rtcp_init;   /* if rtcp is sent ever */

   rtime_t tp, tc, tn; /* RTCP transition time (RFC3550 6.3) */

   uint pn_member;    /* pmember */
    
   char ip[MAX_IPADDR_BYTES];
	int default_rtp_portno;
	int default_rtcp_portno;

	xrtp_port_t *rtp_port;
   xrtp_port_t *rtcp_port;

   xthr_lock_t *rtp_incoming_lock;
	//session_connect_t * rtp_incoming;
   session_connect_t * rtcp_incoming;

   session_connect_t * outgoing_rtcp[XRTP_MAX_REPORTS]; /* Only send to member on the report once a time */
   int n_rtcp_out;     /* How many rtcp send once */
    
   struct param_members_s param_members;
    
   uint32 next_report_ssrc;

	/* should be maintained by profile for different meaning
    uint32 timestamp;
	*/

   rtime_t rtp_cancel_ts;
   int rtp_cancelled;

   sched_schedinfo_t * schedinfo;

   xrtp_teleport_t * join_to_rtp_port;
   xrtp_teleport_t * join_to_rtcp_port;

	struct session_state_s $state;
	struct session_callbacks_s $callbacks;
	struct session_lock_s $locks;
	struct session_cond_s $condvars;

   spin_queue_t * packets_in_sched;
   rtime_t timesize;

   xrtp_thread_t * thr_rtp_recv;
   xthr_lock_t * rtp_recv_lock;
   int thread_run;

	xrtp_set_t* set;
   
   void* rtp_capable;  /* rtp capable */
};

/* Interface of the Session class */

/**
 * Create a new Session
 */
extern DECLSPEC
xrtp_session_t * 
session_new(xrtp_set_t* set, char * cname, int clen, char *ip, uint16 rtp_portno, uint16 rtcp_portno, module_catalog_t * cata, void *media_control, int mode);

/**
 * Release the Session
 */
extern DECLSPEC
int
session_done(xrtp_session_t * session);

extern DECLSPEC
int
session_set_id(xrtp_session_t * session, int id);

extern DECLSPEC
int 
session_id(xrtp_session_t * session);

extern DECLSPEC
int
session_match(xrtp_session_t * session, char *cn, int cnlen, char *ip, uint16 rtp_pno, uint16 rtcp_pno, uint8 profno, char *prof_type);

int session_copy_cname(xrtp_session_t* session, char* cname, int clen);

/* set if an anonymous participant allowed in the session */
extern DECLSPEC
int 
session_allow_anonymous(xrtp_session_t * session, int allow);

/**
 *  From now on, session can receive incoming data
 */
extern DECLSPEC
int
session_start_reception(xrtp_session_t * session);
 
/**
 *  From now on, session stop receiving incoming data
 */
int session_stop_reception(xrtp_session_t * session);

extern DECLSPEC
char* 
session_cname(xrtp_session_t * ses);

/**
 * Get session rtp and rtcp ports
 */
int session_ports(xrtp_session_t * session, xrtp_port_t **r_rtp_port, xrtp_port_t **r_rtcp_port);

/**
 * Get session number of rtp and rtcp ports
 */
extern DECLSPEC
int 
session_portno(xrtp_session_t * session, int *rtp_portno, int *rtcp_portno);

/**
 * Get session net address
 */
extern DECLSPEC
char* 
session_address(xrtp_session_t * session);

/**


 * Set session number of rtp and rtcp ports
 */
extern DECLSPEC
int 
session_set_portno(xrtp_session_t * session, int rtp_portno, int rtcp_portno);

/**
 * Get the actual payload type when it is dynamic.
 */
extern DECLSPEC
int 
session_payload_type(xrtp_session_t *session);

/**
 * Get the owner's media info of the session
 */
extern DECLSPEC
void* 
session_mediainfo(xrtp_session_t *session);

/**
 * Set mode of the Session, which could be
 *
 * 'SESSION_SEND', 'SESSION_RECV' or 'SESSION_DUPLEX'
 */


int session_set_mode(xrtp_session_t * session, int mode);
uint session_mode(xrtp_session_t * session);
 
extern DECLSPEC
int 
session_new_sdp(module_catalog_t* cata, 
				char* nettype, char* addrtype, char* netaddr, 
				int* default_rtp_portno, int* default_rtcp_portno, 
				int pt, char* mime, int clockrate, int coding_param, 
				int bw_budget, void* control, void* sdp_info);

extern DECLSPEC
int
session_mediainfo_sdp(xrtp_session_t* ses,
                        char* nettype, char* addrtype, char* netaddr,
                        int* default_rtp_portno, int* default_rtcp_portno,
                        char* mime, int bw_budget, void* control,
                        void* sdp_info, void* mediainfo);

/**
 * Retrieve a Media handler associated with the session and allocate a payload type to
 * this session to packet transfer. so send/receive packets with this pt.
 */
extern DECLSPEC
xrtp_media_t *
session_new_media(xrtp_session_t * ses, uint8 profile_no, char *profile_type, int clockrate, int coding_param);

extern DECLSPEC
xrtp_media_t*
session_media(xrtp_session_t *ses);

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
 * Get ssrc of session owner
 */
extern DECLSPEC
uint32 
session_ssrc(xrtp_session_t *session);

extern DECLSPEC
xclock_t*
session_clock(xrtp_session_t *session);

/**
 * Set bandwidth for this session, REMEMBER: multiuser need share the bandwidth!!!
 */
extern DECLSPEC
int 
session_set_bandwidth(xrtp_session_t * session, int32 total_bw, int32 rtp_bw);

/**
 * Set bandwidth for this session, REMEMBER: multiuser need share the bandwidth!!!
 */
extern DECLSPEC
int 
session_bandwidth(xrtp_session_t * session);

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
member_state_t * 
session_owner(xrtp_session_t * session);

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

/**
 * Add a cname member to session by external protocol, such as SIP
 * return number of member 
 */
extern DECLSPEC
int 
session_add_cname(xrtp_session_t * ses, char *cn, int cnlen, char *ipaddr, uint16 rtp_portno, uint16 rtcp_portno, void* rtp_capable, void* userinfo);

/**
 * Remove a cname member to session by external protocol, such as SIP
 * return number of member 
 */

extern DECLSPEC
int 
session_delete_cname(xrtp_session_t * ses, char *cname, int cnlen);

/**
 * Move member from one rtp session to other session.
 */
extern DECLSPEC
member_state_t *
session_move_member_by_cname(xrtp_session_t *from_session, xrtp_session_t *to_session, char *cname);

/**
 * Move all members except session owner from one rtp session to other session.
 */
extern DECLSPEC
int
session_move_all_guests(xrtp_session_t *ses, xrtp_session_t *to_ses);

int
session_add_member(xrtp_session_t *ses, member_state_t* member);

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
session_member_update_by_rtp(member_state_t * mem, xrtp_rtp_packet_t * rtp);

extern DECLSPEC
int 
session_member_seqno_stall(member_state_t * mem, int32 seqno);

extern DECLSPEC
int 
session_member_hold_media(member_state_t * member, void *media, int bytes, uint16 seqno, uint32 rtpts, rtime_t us_ref, rtime_t us, int last, void *memblock);
 
extern DECLSPEC
int
member_deliver_media_loop(void *gen);

extern DECLSPEC
int
session_member_deliver(member_state_t * mem, uint16 seqno, int64 packetno);
 
extern DECLSPEC
int
session_member_media_playing(member_state_t *mem, uint32 *rtpts_playing, rtime_t *ms_playing, rtime_t *us_playing, rtime_t *ns_playing);

/**
 * return rtpts_sync - rtpts_remote_sync;
 */
extern DECLSPEC
uint32
session_member_sync_point(member_state_t *mem, uint32 *rtpts_sync, rtime_t *ms_sync, rtime_t *us_sync, rtime_t *ns_sync);

extern DECLSPEC
int64
session_member_samples(member_state_t *mem, uint32 rtpts_payload); 

media_time_t session_hrt2mt(xrtp_session_t *session, rtime_t hrt);
rtime_t session_mt2hrt(xrtp_session_t *session, media_time_t mt);

extern DECLSPEC
uint32 
session_member_mapto_local_time(member_state_t *member, uint32 ts_packet, uint32 ts_local, int level);

extern DECLSPEC
int 
session_member_synchronise(member_state_t *member, uint32 ts_remote, uint32 hi_ntp, uint32 lo_ntp, int clockrate);

/* retrieve media info of the member */
extern DECLSPEC
void*
session_member_mediainfo(member_state_t *member, uint32 *rtpts, int *signum);

/* new media info for the member */
/* return previous media info */
extern DECLSPEC
void *
session_member_update_mediainfo(member_state_t *member, void *minfo, uint32 rtpts, int signum);

/* check if need to renew the media info */
extern DECLSPEC
int
session_member_renew_mediainfo(member_state_t *member, void *minfo, uint32 rtpts, int signum);

/* session is asked to distribute media info to members */
extern DECLSPEC
int 
session_issue_mediainfo(xrtp_session_t *ses, void *minfo, int signum);

/* check if need to send media info to member */
extern DECLSPEC
int
session_distribute_mediainfo(xrtp_session_t *ses, uint32 *rtpts, int *signum, void **minfo_ret);

extern DECLSPEC
void*
session_member_player(member_state_t *member);

/* return previous player */
extern DECLSPEC
void*
session_member_set_player(member_state_t *member, void *player);

extern DECLSPEC
void*
session_media_control(xrtp_session_t *session);

uint32
session_signature(xrtp_session_t *session);

extern DECLSPEC
int
session_solve_collision(member_state_t * member, uint32 ssrc);

/* Notify the session a rtp arrival and schedule the process 
int session_rtp_arrived(xrtp_session_t * session, session_connect_t * connect);
*/

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

int session_member_number(xrtp_session_t * session);

int session_sender_number(xrtp_session_t * session);

extern DECLSPEC
int 
session_set_rtp_rate(xrtp_session_t *session, int rate);

rtime_t session_rtp_period(xrtp_session_t *session);
/* How long is allow to delay the rtp sending, used as a sched window */
rtime_t session_rtp_delay(xrtp_session_t *session);
 
xrtp_lrtime_t session_rtcp_interval(xrtp_session_t *session);
/* How long is allow to delay the rtcp sending, used as a sched window */
xrtp_lrtime_t session_rtcp_delay(xrtp_session_t *session);
 
extern DECLSPEC
int 
session_count_rtcp(xrtp_session_t * ses, xrtp_rtcp_compound_t * rtcp);
 
int session_notify_delay(xrtp_session_t * session, rtime_t howlate);
 
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
session_rtp_to_send(xrtp_session_t *session, rtime_t ts, int last);
int session_cancel_rtp_sending(xrtp_session_t *session, rtime_t ts);
 
/**
 * Reached schedule time, send data immidiately, called by scheduler
 * budget: The bandwidth available for sending
 * nts: The completed frame number(not just packets) sent is filled in.
 *
 * return the byte sent, to adjust the current budget.
 */
int session_rtp_send_now(xrtp_session_t *session);
 
int session_receive_rtp(xrtp_session_t *ses, session_connect_t* incoming);

/**
 * Check if the rtp packet is a cancelled one
 */
int 
session_rtp_receiving_cancelled(xrtp_session_t * session, rtime_t ts);

/**
 * send rtcp packet to pipeline
 * return the packet origional size
 */
int 
session_need_rtcp(xrtp_session_t *session);
 
/**
 * Make RTCP report, rtp timestamp is set if it's a SR.
 */
extern DECLSPEC
int 
session_report(xrtp_session_t *ses, xrtp_rtcp_compound_t * rtcp, uint32 timestamp, rtime_t ms, rtime_t us, rtime_t ns, uint32 hi_ntp, uint32 lo_ntp);

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
