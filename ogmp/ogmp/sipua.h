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

#include "media_format.h"
#include "devices/dev_rtp.h"
#include "eXosipua/eXutils.h"

#define UA_OK		0
#define UA_FAIL		-1
#define UA_REJECT	-2
#define UA_IGNORE	-3

#define MAX_CN_BYTES 256 /* max value in rtcp */
#define MAX_IP_LEN   64  /* may enough for ipv6 ? */

#define MAX_PAYLOAD_TYPE 128

#define SIPUA_EVENT_SUCCEED						0
#define SIPUA_EVENT_REGISTRATION_SUCCEEDED		1
#define SIPUA_EVENT_UNREGISTRATION_SUCCEEDED	2

#define SIPUA_EVENT_NEW_CALL					3
#define SIPUA_EVENT_PROCEEDING					4
#define SIPUA_EVENT_RINGING						5
#define SIPUA_EVENT_ANSWERED					6
#define SIPUA_EVENT_REDIRECTED					7
#define SIPUA_EVENT_CALL_CLOSED					8
#define SIPUA_EVENT_ONHOLD						9
#define SIPUA_EVENT_OFFHOLD						10

#define SIPUA_EVENT_REGISTRATION_FAILURE		-1
#define SIPUA_EVENT_UNREGISTRATION_FAILURE		-2
#define SIPUA_EVENT_REQUESTFAILURE				-3
#define SIPUA_EVENT_SERVERFAILURE				-4
#define SIPUA_EVENT_GLOBALFAILURE				-5

#include "phonebook.h"

/* Contact Proxy
 * Sender register its session info to proxy
 * recvr contact sender by interact with proxy
 */
typedef struct sipua_s sipua_t;
typedef struct sipua_uas_s sipua_uas_t;
typedef struct sipua_action_s sipua_action_t;

typedef struct sipua_setid_s sipua_setid_t;
struct sipua_setid_s
{
	char* id;
	char* username;
	char* nettype;
	char* addrtype;
	char* netaddr;
};

typedef struct sipua_set_s sipua_set_t;
struct sipua_set_s
{
	sipua_setid_t setid;

	char* version;

	char* subject;
	int sbyte;

	char* info;
	int ibyte;

	int lineno;  /* on which line */

	xlist_t *mediaset;
};

typedef struct sipua_event_s sipua_event_t;
struct sipua_event_s
{
	int lineno;
	sipua_set_t *call_info;

	char* from;

	int type;
	void* content;
};

#define MAX_NETTYPE_BYTES 8
#define MAX_IP_BYTES 64

struct sipua_uas_s
{
	int portno;

	char nettype[MAX_NETTYPE_BYTES];
	char addrtype[MAX_NETTYPE_BYTES];

	char netaddr[MAX_IP_BYTES];
	char firewall[MAX_IP_BYTES];
	char proxy[MAX_IP_BYTES];

	void* lisener;
	int (*notify_event)(void* listener, sipua_event_t* e);

	int(*start)(sipua_uas_t* uas);
	int(*shutdown)(sipua_uas_t* uas);

	int(*address)(sipua_uas_t* uas, char **nettype, char **addrtype, char **netaddr);

	int(*set_listener)(sipua_uas_t* uas, void* listener, int(*event_notify)(void*, sipua_event_t*));

	int (*add_coding)(sipua_uas_t* uas, int pt, int rtp_portno, int rtcp_portno, char* mime, int clockrate, int param);
	
	int (*clear_coding)(sipua_uas_t* uas);
	
	int (*regist)(sipua_uas_t* uas, char *registrar, char *regname, int seconds);
	
	int (*unregist)(sipua_uas_t* uas, char *registrar, char *regname);
 	
	int (*call)(sipua_uas_t* uas, char *regname, sipua_set_t* call, char* sdp_body, int bytes);

	sipua_set_t* (*pick_call)(sipua_uas_t* uas, int lineno);

	int (*set_call)(sipua_uas_t* uas, int lineno, sipua_set_t *call_info);
};

extern DECLSPEC
sipua_uas_t* sipua_uas(int portno, char* nettype, char* addrtype, char* firewall, char* proxy);

struct sipua_action_s
{
	int (*done)(sipua_action_t *rec);

	void *sip_user;
	//int (*oncall)(void *user, char *cname, int cnlen, capable_descript_t* oppo_caps[], int oppo_ncap, capable_descript_t* **my_caps);
	int (*onregister)(void *user, char *from, int status, void* info);
	int (*oncall)(void *user, char *from, void* info_in, void* info_out);
	int (*onconnect)(void *user, char *from, void* info_in);
	int (*onreset)(void *user, char *from, void* info_in);
	int (*onbye)(void *user, char *from);
};

sipua_action_t *sipua_new_action(void *sip_user, 
								int(*cb_onregister)(void*, char*, int, void*), 
								int(*cb_oncall)(void*, char*, void*, void*), 
								int(*cb_onconnect)(void*, char*, void*), 
								int(*cb_onreset)(void*, char*, void*),
								int(*cb_onbye)(void*,char*));

struct sipua_s
{
	sipua_uas_t* uas;
	
	int (*done)(sipua_t *ua);

	int (*set_user)(sipua_t *sipua, user_profile_t *user);
	user_profile_t* (*user)(sipua_t *sipua);

	int (*regist)(sipua_t *sipua, sipua_action_t *act);
	int (*unregist)(sipua_t *sipua);

 	sipua_set_t* (*new_conference)(sipua_t *sipua, char* id, 
					  char* subject, int sbyte, char* info, int ibyte, 
					  rtp_coding_t codings[], int ncoding);

	int (*done_conference)(sipua_t *sipua, sipua_set_t* set);
	
	sipua_set_t* (*pick_conference)(sipua_t *sipua, sipua_set_t* set);

	int (*add)(sipua_t *sipua, sipua_set_t* set, xrtp_media_t* rtp_media);
 	int (*remove)(sipua_t *sipua, sipua_set_t* set, xrtp_media_t* rtp_media);

 	int (*session_sdp)(sipua_t *sipua, sipua_set_t* set, char** sdp);

	int (*invite)(sipua_t *ua, sipua_set_t* set, char *regname);
	
	int (*bye)(sipua_t *ua, sipua_set_t* set);
	
	int (*reinvite)(sipua_t *sipua, sipua_set_t* set);
};

sipua_t* sipua_new(sipua_uas_t *uas, int bandwidth, media_control_t* control);

#endif