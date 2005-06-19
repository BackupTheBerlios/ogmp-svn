/***************************************************************************
                          sipua.c  -  user agent
                             -------------------
    begin                : Wed Jul 14 2004
    copyright            : (C) 2004 by Heming Ling
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

#ifndef SIPUA_H
#define SIPUA_H

#include <timedia/os.h>
#include <timedia/xstring.h>
#include <xrtp/xrtp.h>

#include "rtp_format.h"
#include "dev_rtp.h"
#include "eXutils.h"

#define MAX_CN_BYTES 256 /* max value in rtcp */
#define MAX_IP_LEN   64  /* may enough for ipv6 ? */

#define MAX_PAYLOAD_TYPE 128
#define MAX_CALLID_LEN		32

#define MAX_SIPUA_LINES		4
#define MAX_SIPUA_MEDIA		4

#define SIPUA_EVENT_SUCCEED						   0
#define SIPUA_EVENT_REGISTRATION_SUCCEEDED		1
#define SIPUA_EVENT_UNREGISTRATION_SUCCEEDED	   2

#define SIPUA_EVENT_NEW_CALL					3
#define SIPUA_EVENT_PROCEEDING				4
#define SIPUA_EVENT_RINGING					5
#define SIPUA_EVENT_ANSWERED					6
#define SIPUA_EVENT_REDIRECTED				7
#define SIPUA_EVENT_CALL_CLOSED				8
#define SIPUA_EVENT_ONHOLD						9
#define SIPUA_EVENT_OFFHOLD					10
#define SIPUA_EVENT_ACK							11

#define SIPUA_EVENT_REGISTRATION_FAILURE		-1
#define SIPUA_EVENT_UNREGISTRATION_FAILURE	-2
#define SIPUA_EVENT_REQUESTFAILURE				-3
#define SIPUA_EVENT_SERVERFAILURE				-4
#define SIPUA_EVENT_GLOBALFAILURE				-5

#define SIPUA_STATUS_UNKNOWN				-1
#define SIPUA_STATUS_NORMAL				0
#define SIPUA_STATUS_REG_OK				1
#define SIPUA_STATUS_REG_FAIL				2
#define SIPUA_STATUS_UNREG_FAIL			3
#define SIPUA_STATUS_REG_DOING			4
#define SIPUA_STATUS_UNREG_DOING			5
#define SIPUA_STATUS_QUEUE			      6
#define SIPUA_STATUS_SERVE    			7
#define SIPUA_STATUS_DECLINE  			8
#define SIPUA_STATUS_INCOMING    		9
#define SIPUA_STATUS_ACCEPT   		   10
#define SIPUA_STATUS_INCOMING    			9
#define SIPUA_STATUS_ACCEPT    			10

#define SIP_STATUS_CODE_TRYING	            100
#define SIP_STATUS_CODE_DIALOGESTABLISHMENT	101
#define SIP_STATUS_CODE_RINGING	            180
#define SIP_STATUS_CODE_FORAWRD	            181
#define SIP_STATUS_CODE_QUEUE	               182
#define SIP_STATUS_CODE_SESSIONPROGRESS	   183

#define SIP_STATUS_CODE_OK    	            200

#define SIP_STATUS_CODE_MULTIPLECHOICES         300
#define SIP_STATUS_CODE_MOVEDPERMANENTLY        301
#define SIP_STATUS_CODE_MOVEDTEMPORARILY        302
#define SIP_STATUS_CODE_USEPROXY                305
#define SIP_STATUS_CODE_ALTERNATIVESERVICE      380

#define SIP_STATUS_CODE_BADREQUEST                    400
#define SIP_STATUS_CODE_UNAUTHORIZED                  401
#define SIP_STATUS_CODE_PAYMENTREQUIRED               402
#define SIP_STATUS_CODE_FORBIDDEN                     403
#define SIP_STATUS_CODE_NOTFOUND                      404
#define SIP_STATUS_CODE_METHODNOTALLOWED              405
#define SIP_STATUS_CODE_NOTACCEPTABLE                 406
#define SIP_STATUS_CODE_PROXYAUTHENTICATIONREQUIRED   407
#define SIP_STATUS_CODE_REQUESTTIMEOUT                408
#define SIP_STATUS_CODE_CONFLICT                      409
#define SIP_STATUS_CODE_GONE                          410
#define SIP_STATUS_CODE_LENGTHREQUIRED                411
#define SIP_STATUS_CODE_REQUESTENTITYTOOLARGE         413
#define SIP_STATUS_CODE_REQUESTURITOOLONG             414
#define SIP_STATUS_CODE_UNSUPPORTEDMEDIATYPE          415
#define SIP_STATUS_CODE_BADEXTENSION                  420
#define SIP_STATUS_CODE_TEMPORARILYUNAVAILABLE        480
#define SIP_STATUS_CODE_TRANSACTIONDOESNOTEXIST       481
#define SIP_STATUS_CODE_LOOPDETECTED                  482
#define SIP_STATUS_CODE_TOOMANYHOPS                   483
#define SIP_STATUS_CODE_ADDRESSINCOMPLETE             484
#define SIP_STATUS_CODE_AMBIGUOUS                     485
#define SIP_STATUS_CODE_BUSYHERE	                  486
#define SIP_STATUS_CODE_REQUESTTERMINATED             487
#define SIP_STATUS_CODE_NOTACCEPTABLEHERE	            488
#define SIP_STATUS_CODE_REQUESTPENDING	               491
#define SIP_STATUS_CODE_UNDECIPHERABLE	               493

#define SIP_STATUS_CODE_SERVERINTERNALERROR	   500
#define SIP_STATUS_CODE_NOTIMPLEMENTED	         501
#define SIP_STATUS_CODE_BADGATEWAY	            502
#define SIP_STATUS_CODE_SERVICEUNAVAILABLE      503
#define SIP_STATUS_CODE_GATEWAYTIMEOUT	         504
#define SIP_STATUS_CODE_VERSIONNOTSUPPORTED	   505

#define SIP_STATUS_CODE_BUSYEVERYWHERE	               600
#define SIP_STATUS_CODE_DECLINE	                     603
#define SIP_STATUS_CODE_DOESNOTEXISTANTWHERE	         604
#define SIP_STATUS_CODE_GLOBALNOTACCEPTABLE	         606

#include "phonebook.h"

typedef struct sipua_setting_s sipua_setting_t;
struct sipua_setting_s
{
	char nettype[8];
	char addrtype[8];

	int default_rtp_portno;
	int default_rtcp_portno;

	int ncoding;
	rtp_coding_t codings[MAX_NPAYLOAD_PRESET];
};

/* Contact Proxy
 * Sender register its session info to proxy
 * recvr contact sender by interact with proxy
 */
typedef struct sipua_s sipua_t;
typedef struct sipua_uas_s sipua_uas_t;

typedef struct sipua_setid_s sipua_setid_t;
struct sipua_setid_s
{
	char* id;
	char* username;
	char* nettype;
	char* addrtype;
	char* netaddr;
};

typedef struct sipua_call_s sipua_call_t;
typedef struct sipua_auth_s sipua_auth_t;

struct sipua_auth_s
{
   char* username;
   char* realm;
   char* authid;    /* Due to libeXosip v1 cannot delete single auth */
   char* password;  /* I have to cache all of them for re-adding. */
};

struct sipua_call_s
{
	sipua_setid_t setid;

	/* cid, did is ONLY for eXosip to determine local call */
	int cid;
	int did;

	char* version;

	char* subject;
	int sbyte;

	char* info;
	int ibyte;

	int lineno;  /* on which line */

	user_profile_t* user_prof;

	char* proto; /* 'sip' or 'sips' */
   char* from;  /* name@domain, for outgoing call, from = NULL */

	int status;  /* line status */
   /*
	xlist_t *mediaset;
   */
	media_format_t* rtp_format;
   
	int bandwidth_need;
   int bandwidth_hold;

	char* sdp_body;
	rtpcap_set_t* rtpcapset;

	char* reply_body;
   char* renew_body;

	int ring_num;
};

typedef struct sipua_event_s sipua_event_t;
struct sipua_event_s
{
	int lineno;
	sipua_call_t *call_info;

	char* from;

	int type;

	void* content;
};

typedef struct sipua_reg_event_s sipua_reg_event_t;
struct sipua_reg_event_s
{
	sipua_event_t event;

	int status_code;

	char *server_info;
	char *server;

	int seconds_expires;
};

typedef struct sipua_call_event_s sipua_call_event_t;

struct sipua_call_event_s
{
	sipua_event_t event;

	int status_code;

	int cid;
	int did;

	char *subject;
   int sbytes;
	char *textinfo;
   int tbytes;
   
	char *reason_phrase;

	char *req_uri;
	char *remote_uri;
	char *local_uri;

   char *auth_realm;
};

#define MAX_NETTYPE_BYTES 8
#define MAX_IP_BYTES 64

typedef struct sipua_net_s sipua_net_t;
struct sipua_net_s
{
	int portno;

	char nettype[MAX_NETTYPE_BYTES];
	char addrtype[MAX_NETTYPE_BYTES];

	char netaddr[MAX_IP_BYTES];
	char firewall[MAX_IP_BYTES];
	char proxy[MAX_IP_BYTES];
};

struct sipua_uas_s
{
	int portno;

	char nettype[MAX_NETTYPE_BYTES];
	char addrtype[MAX_NETTYPE_BYTES];

	char netaddr[MAX_IP_BYTES];
	char firewall[MAX_IP_BYTES];
	char proxy[MAX_IP_BYTES];

   xlist_t *auth_list;

	void* lisener;

	int (*notify_event)(void* listener, sipua_event_t* e);

	int (*match_type)(sipua_uas_t* uas, char *type);

   int (*init)(sipua_uas_t* uas, int portno, const char* nettype, const char* addrtype, const char* firewall, const char* proxy);
	int (*done)(sipua_uas_t* uas);

   int (*start)(sipua_uas_t* uas);
	int (*shutdown)(sipua_uas_t* uas);

	int (*address)(sipua_uas_t* uas, char **nettype, char **addrtype, char **netaddr);

	int (*set_listener)(sipua_uas_t* uas, void* listener, int(*event_notify)(void*, sipua_event_t*));

	int (*add_coding)(sipua_uas_t* uas, int pt, int rtp_portno, int rtcp_portno, char* mime, int clockrate, int param);
	
	int (*clear_coding)(sipua_uas_t* uas);
	
   int (*set_authentication_info)(sipua_uas_t* uas, char *username, char *userid, char*passwd, char *realm);
   int (*clear_authentication_info)(sipua_uas_t* uas, char *username, char *realm);
   int (*has_authentication_info)(sipua_uas_t* uas, char *username, char *realm);

   /* regno is for re-register the previous one */
   int (*regist)(sipua_uas_t* uas, int *regno, char *userloc, char *registrar, char *regname, int seconds);
	
	int (*unregist)(sipua_uas_t* uas, char *userloc, char *registrar, char *regname);
 	
	int (*accept)(sipua_uas_t* uas, sipua_call_t *call);


	int (*release)(sipua_uas_t* uas);

	int (*invite)(sipua_uas_t* uas, const char *regname, sipua_call_t* call, char* sdp_body, int bytes);
	int (*answer)(sipua_uas_t* uas, sipua_call_t* call, int reply, char* reply_type, char* reply_body);
	int (*bye)(sipua_uas_t* uas, sipua_call_t* call);
   
	int (*retry)(sipua_uas_t* uas, sipua_call_t* call);
};

extern DECLSPEC
sipua_uas_t* sipua_uas(int portno, char* nettype, char* addrtype, char* firewall, char* proxy);

struct sipua_s
{
	sipua_uas_t* uas;
   char *proxy_realm;

   /* sipua_call_t* incall; The call in conversation */
   
   user_profile_t* user_profile;
   config_t* config;

	int (*done)(sipua_t *ua);

	char* (*userloc)(sipua_t* sipua, char* uid);
	int (*locate_user)(sipua_t* sipua, user_t* user);
   
	user_t* (*load_userdata)(sipua_t* sipua, const char* loc, const char* uid, const char* tok, const int tsz);

	int (*set_profile)(sipua_t* sipua, user_profile_t* prof);
	user_profile_t* (*profile)(sipua_t* sipua);

	int (*authenticate)(sipua_t *sipua, int profile_no, const char* realm, const char *auth_id, int authbytes, const char* passwd, int pwdbytes);
   
	/* registration */
	int (*regist) (sipua_t *sipua, user_profile_t *user, char *userloc);
	int (*unregist) (sipua_t *sipua, user_profile_t *user);
   
   int (*register_profile) (sipua_t *sipua, int profile_no);
   int (*unregister_profile) (sipua_t *sipua, int profile_no);

	/* call */
	sipua_call_t* (*make_call)(sipua_t* sipua, const char* subject, int sbytes, const char *desc, int dbytes);
	int (*done_call)(sipua_t *sipua, sipua_call_t* set);
 	
	/**
	 * conversation media
	 * return new bandwidth, <0 fail 
	int (*add)(sipua_t *sipua, sipua_call_t* set, xrtp_media_t* rtp_media, int bandwidth);
	int (*remove)(sipua_t *sipua, sipua_call_t* set, xrtp_media_t* rtp_media);
	 */

	/* lines management */
	int (*lock_lines)(sipua_t* sipua);
	int (*unlock_lines)(sipua_t* sipua);


	int (*line_range)(sipua_t* sipua, int *min, int *max);
	int (*lines)(sipua_t* sipua, int *nbusy);
	int (*busylines)(sipua_t* sipua, int *busylines, int nlines);

	/* current call session */
	sipua_call_t* (*session)(sipua_t* sipua);
	sipua_call_t* (*line)(sipua_t* sipua, int line);

	/* switch current call session */
	sipua_call_t* (*pick)(sipua_t* sipua, int line);
	int (*hold)(sipua_t* sipua);

	/**
	 * Set the default media playing when call in queue or on hold:
	 * @param sipua		The default media source
	 * @param subject	subject of SDP of background media
	 * @param info		info of SDP of background media
	 */
	media_source_t* (*set_background_source)(sipua_t* sipua, char* name, char* subject, char* info);
	
	sipua_setting_t* (*setting)(sipua_t* sipua);

	/* call media attachment */
	media_source_t* (*open_source)(sipua_t* sipua, char* name, char* mode, void* param);
	int (*close_source)(sipua_t* sipua, media_source_t* src);

	int (*attach_source)(sipua_t* sipua, sipua_call_t* call, transmit_source_t* src);
	int (*detach_source)(sipua_t* sipua, sipua_call_t* call, transmit_source_t* src);
	
	/* session description */
	int (*session_sdp)(sipua_t *sipua, sipua_call_t* set, char** sdp);

	int (*call)(sipua_t *ua, sipua_call_t* set, const char *regname, char* sdp_body);
	int (*answer)(sipua_t *ua, sipua_call_t* call, int reason, media_source_t* source);
   
	int (*queue)(sipua_t *ua, sipua_call_t* call);

	int (*options_call)(sipua_t *ua, sipua_call_t* call);
	int (*info_call)(sipua_t *ua, sipua_call_t* call, char *type, char *info);

	int (*bye)(sipua_t *ua, sipua_call_t* set);
	char* (*set_call_source)(sipua_t *sipua, sipua_call_t* set, media_source_t* source);

    /* Set callbacks */
    int (*set_register_callback)(sipua_t *sipua, int(*callback)(void*callback_user,int statuscode,char*reason,int isreg), void* callback_user);
    int (*set_authentication_callback)(sipua_t *sipua, int(*callback)(void*callback_user, char* realm, user_profile_t* profile, char** user_id, char** user_password), void* callback_user);
    int (*set_progress_callback)(sipua_t *sipua, int(*callback)(void*callback_user,sipua_call_t *call,int statuscode), void* callback_user);
    int (*set_newcall_callback)(sipua_t *sipua, int(*callback)(void*callback_user,int lineno,sipua_call_t *call), void* callback_user);
    int (*set_terminate_callback)(sipua_t *sipua, int(*callback)(void*callback_user,sipua_call_t *call,int statuscode), void* callback_user);
    int (*set_conversation_start_callback)(sipua_t *sipua, int(*callback)(void *callback_user, int lineno,sipua_call_t *call), void* callback_user);
    int (*set_conversation_end_callback)(sipua_t *sipua, int(*callback)(void *callback_user,int lineno,sipua_call_t *call), void* callback_user);
    int (*set_bye_callback)(sipua_t *sipua, int (*callback)(void *callback_user, int lineno, char *caller, char *reason), void* callback_user);

    int (*subscribe_bandwidth)(sipua_t *sipua, sipua_call_t *call);
    int (*unsubscribe_bandwidth)(sipua_t *sipua, sipua_call_t *call);
};

sipua_t* sipua_new(sipua_uas_t *uas, void* event_lisener, int(*lisen)(void*,sipua_event_t*), int bandwidth, media_control_t* control);
	
/* User location : user@host or user@address */
DECLSPEC
char* 
sipua_userloc(sipua_t* sipua, char* uid);

DECLSPEC
int 
sipua_locate_user(sipua_t* sipua, user_t* user);

/* registration */
DECLSPEC
int 
sipua_regist(sipua_t *sipua, user_profile_t *user, char *userloc);

DECLSPEC
int 
sipua_unregist(sipua_t *sipua, user_profile_t *user);

/* create call with rtp sessions when establish a call 
sipua_call_t* 
sipua_create_call(sipua_t *sipua, user_profile_t* user_prof, char* id, 
                  char* subject, int sbyte, char* info, int ibyte,
                  char* mediatypes[], int rtp_ports[], int rtcp_ports[], 
                  int nmedia, media_control_t* control, 
                  rtp_coding_t codings[], int ncoding, int pt_pool[]);
*/
DECLSPEC
sipua_call_t*
sipua_new_call(sipua_t *ua, user_profile_t* current_prof, char *from,
                              char *subject, int sbytes, char *info, int ibytes);

/* generate sdp message when call out, no rtp session created */
DECLSPEC
int 
sipua_make_call(sipua_t *sipua, sipua_call_t* call, char* id,
					char* mediatypes[], int rtp_ports[], int rtcp_ports[],
					int nmedia, media_control_t* control, int bw_budget,
					rtp_coding_t codings[], int ncoding, int pt_pool[]);

DECLSPEC
int
sipua_negotiate_call(sipua_t *sipua, sipua_call_t *call,
                     rtpcap_set_t* rtpcapset,
							char* mediatypes[], int rtp_ports[], int rtcp_ports[], 
							int nmedia, media_control_t* control, int call_max_bw);

DECLSPEC
int 
sipua_establish_call(sipua_t* sipua, sipua_call_t* call, char* mode, rtpcap_set_t* rtpcapset,
							   xlist_t* format_handlers, media_control_t* control, int pt_pool[]);

/* Create a SDP with new set of media info */
DECLSPEC
char*
sipua_call_sdp(sipua_t *sipua, sipua_call_t* call, int bw_budget, media_control_t* control,
                char* mediatypes[], int rtp_ports[], int rtcp_ports[], int nmedia,
                media_source_t* source, int pt_pool[]);
                
int sipua_done_sip_session(void* gen);

/*
conversation media
return new bandwidth, <0 fail 
*/
//int sipua_add(sipua_t *sipua, sipua_call_t* set, xrtp_media_t* rtp_media, int bandwidth);
int sipua_remove(sipua_t *sipua, sipua_call_t* set, xrtp_media_t* rtp_media);

/* session description */
int sipua_session_sdp(sipua_t *sipua, sipua_call_t* set, char** sdp);

DECLSPEC
int
sipua_receive(sipua_t *ua, sipua_call_event_t *call_e, char **from, char **subject, int *sbytes, char **info, int *ibytes);

DECLSPEC
int 
sipua_call(sipua_t *ua, sipua_call_t* set, const char *regname, char *sdp_body);
	
DECLSPEC
int
sipua_retry_call(sipua_t *ua, sipua_call_t* call);

DECLSPEC
int 
sipua_answer(sipua_t *ua, sipua_call_t* call, int reason, char* sdp_body);

DECLSPEC
int 
sipua_options_call(sipua_t *ua, sipua_call_t* call);

DECLSPEC
int 
sipua_info_call(sipua_t *ua, sipua_call_t* call, char *type, char *info);

DECLSPEC
int 
sipua_bye(sipua_t *ua, sipua_call_t* set);

DECLSPEC
int 
sipua_recall(sipua_t *sipua, sipua_call_t* set);


#endif
