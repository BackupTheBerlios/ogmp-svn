/*
 * josua - Jack's open sip User Agent
 *
 * Copyright (C) 2002,2003   Aymeric Moizard <jack@atosc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sipua.h>

#include <eXosip/eXosip.h>

typedef struct eXosipua_s eXosipua_t;

/*
#include "jcalls.h"
*/

struct jcall 
{
  int cid;
  int did;

  char reason_phrase[50];
  int  status_code;

  char textinfo[256];

  char req_uri[256];
  char local_uri[256];
  char remote_uri[256];

  char subject[256];

  char remote_sdp_audio_ip[50];
  int  remote_sdp_audio_port;

  int  payload;
  char payload_name[50];
  
#if defined(WIN32) && defined(ORTP_SUPPORT)

  RtpSession *rtp_session;
  int enable_audio; /* 0 started, -1 stopped */
  struct osip_thread *audio_thread;

#elif defined(ORTP_SUPPORT)

  RtpSession *rtp_session;
  int enable_audio; /* 0 started, -1 stopped */
  struct osip_thread *audio_thread;
  struct osip_thread *out_audio_thread;

#elif defined(XRTP_SUPPORT)

#define MAX_CAPABLES 32 /* maximum 256 due to some media format limitation, set to 32 currently */

  capable_descript_t *capables[MAX_CAPABLES];
  int ncap;

#endif

#define NOT_USED      0
  int state;
};

typedef struct jcall jcall_t;

#if 0
int    os_sound_init();
int    os_sound_start(jcall_t *ca);
void  *os_sound_start_thread(void *_ca);
void  *os_sound_start_out_thread(void *_ca);
void   os_sound_close(jcall_t *ca);
#endif

#if defined(ORTP_SUPPORT)
void rcv_telephone_event(RtpSession *rtp_session, jcall_t *ca);
#endif

int jcall_init(eXosipua_t *jua);
jcall_t *jcall_find_call(eXosipua_t *jua, int pos);
int jcall_get_number_of_pending_calls(eXosipua_t *jua);

int jcall_new(eXosipua_t *jua, eXosip_event_t *je);
int jcall_answered(eXosipua_t *jua, eXosip_event_t *je);
int jcall_proceeding(eXosipua_t *jua, eXosip_event_t *je);
int jcall_ringing(eXosipua_t *jua, eXosip_event_t *je);
int jcall_redirected(eXosipua_t *jua, eXosip_event_t *je);
int jcall_requestfailure(eXosipua_t *jua, eXosip_event_t *je);
int jcall_serverfailure(eXosipua_t *jua, eXosip_event_t *je);
int jcall_globalfailure(eXosipua_t *jua, eXosip_event_t *je);

int jcall_closed(eXosipua_t *jua, eXosip_event_t *je);
int jcall_onhold(eXosipua_t *jua, eXosip_event_t *je);
int jcall_offhold(eXosipua_t *jua, eXosip_event_t *je);

int jcall_remove(eXosipua_t *jua, jcall_t *ca);
/* End of jcall.h */

/*
#include "jsubscriptions.h"
*/
struct jsubscription {
  int sid;
  int did;

  char reason_phrase[50];
  int  status_code;

  char textinfo[256];
  char req_uri[256];
  char local_uri[256];
  char remote_uri[256];

  int online_status;
  int ss_status;
  int ss_reason;

#define NOT_USED      0
  int state;

};

typedef struct jsubscription jsubscription_t;

jsubscription_t *jsubscription_find_subscription(eXosipua_t *jua, int pos);
int jsubscription_get_number_of_pending_subscriptions(eXosipua_t *jua);

int jsubscription_new(eXosipua_t *jua, eXosip_event_t *je);
int jsubscription_answered(eXosipua_t *jua, eXosip_event_t *je);
int jsubscription_proceeding(eXosipua_t *jua, eXosip_event_t *je);
int jsubscription_redirected(eXosipua_t *jua, eXosip_event_t *je);
int jsubscription_requestfailure(eXosipua_t *jua, eXosip_event_t *je);
int jsubscription_serverfailure(eXosipua_t *jua, eXosip_event_t *je);
int jsubscription_globalfailure(eXosipua_t *jua, eXosip_event_t *je);
int jsubscription_notify(eXosipua_t *jua, eXosip_event_t *je);

int jsubscription_closed(eXosipua_t *jua, eXosip_event_t *je);

int jsubscription_remove(eXosipua_t *jua, jsubscription_t *ca);
/* End of jsubscriptions.h */

/*
#include "jinsubscriptions.h"
*/
struct jinsubscription {
  int nid;
  int did;

  char reason_phrase[50];
  int  status_code;

  char textinfo[256];
  char req_uri[256];
  char local_uri[256];
  char remote_uri[256];

  int online_status;
  int ss_status;
  int ss_reason;

#define NOT_USED      0
  int state;

};

typedef struct jinsubscription jinsubscription_t;

jinsubscription_t *jinsubscription_find_insubscription(eXosipua_t *jua, int pos);
int jinsubscription_get_number_of_pending_insubscriptions(eXosipua_t *jua);

int jinsubscription_new(eXosipua_t *jua, eXosip_event_t *je);
int jinsubscription_answered(eXosipua_t *jua, eXosip_event_t *je);
int jinsubscription_proceeding(eXosipua_t *jua, eXosip_event_t *je);
int jinsubscription_redirected(eXosipua_t *jua, eXosip_event_t *je);
int jinsubscription_requestfailure(eXosipua_t *jua, eXosip_event_t *je);
int jinsubscription_serverfailure(eXosipua_t *jua, eXosip_event_t *je);
int jinsubscription_globalfailure(eXosipua_t *jua, eXosip_event_t *je);

int jinsubscription_closed(eXosipua_t *jua, eXosip_event_t *je);

int jinsubscription_remove(eXosipua_t *jua, jinsubscription_t *ca);
/* End of jinsubscriptions.h*/

#include <timedia/xthread.h>
#include <timedia/timer.h>

#ifndef MAX_NUMBER_OF_CALLS
#define MAX_NUMBER_OF_CALLS 10
#endif

#ifndef MAX_NUMBER_OF_SUBSCRIPTIONS
#define MAX_NUMBER_OF_SUBSCRIPTIONS 100
#endif

#ifndef MAX_NUMBER_OF_INSUBSCRIPTIONS
#define MAX_NUMBER_OF_INSUBSCRIPTIONS 100
#endif

#define MAXBYTES_ID 256

struct eXosipua_s
{
	struct sipua_s sipua;

	int online_status;
	int registration_status;

	char localip[30];

	char registration_server[100];
	char registration_reason_phrase[100];

	char owner[MAXBYTES_ID];
	int  owner_bytes;
	
	char current_id[MAXBYTES_ID];
	int  current_idbytes;

	sipua_action_t *action;

	jcall_t jcalls[MAX_NUMBER_OF_CALLS];

	jsubscription_t jsubscriptions[MAX_NUMBER_OF_SUBSCRIPTIONS];

	jinsubscription_t jinsubscriptions[MAX_NUMBER_OF_INSUBSCRIPTIONS];

	int run;
	xthread_t *thread;

	xclock_t *clock;
};

/**
 * Processing the events, return the number of event happened.
 */
int sipua_process_event(sipua_t *ua);

/**
 * Suppose in "a=rtpmap:96 G.729a/8000/1", rtpmap string would be "96 G.729a/8000/1"
 * parse it into rtpmapno=96; coding_type="G.729a"; clockrate=8000; coding_param=1
 */
int sdp_parse_rtpmap(char *rtpmap, int *rtpmapno, char *coding_type, int *clockrate, int *coding_param);

/**
 * Parse IPv4 string "a.b.c.d/n" into "a.b.c.d" and n (maskbits)
 */
int jua_parse_ipv4(char *addr, char *ip, int ipbytes, int *maskbits);
/**
 * Parse sdp rtcp attr (a=rtcp:)
 * port: "53020"
 * ipv4: "53020 IN IP4 126.16.64.4"
 * ipv6: "53020 IN IP6 2001:2345:6789:ABCD:EF01:2345:6789:ABCD"
 */
int jua_parse_rtcp(char *rtcp, char *control_ip, int buflen, uint *control_port);
