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

#include "eXosipua.h"
#include "../rtp_cap.h"

#include <timedia/xstring.h>

#define JUA_LOG

#ifdef JUA_LOG
 #define jua_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define jua_log(fmtargs)
#endif

#define JUA_INTERVAL_MSEC 1000

/**
 * Suppose in "a=rtpmap:96 G.729a/8000/1", rtpmap string would be "96 G.729a/8000/1"
 * parse it into rtpmapno=96; coding_type="G.729a"; clockrate=8000; coding_param=1
 */
int sdp_parse_rtpmap(char *rtpmap, int *rtpmapno, char *coding_type, int *clockrate, int *coding_param)
{
	char *end;
	char *token = rtpmap;

	*rtpmapno = (int)strtol(token, NULL, 10);

	while(*token != ' ' || *token != '\t')
		token++;
	while(*token == ' ' || *token == '\t')
		token++;

	end = strchr(token, '/');

	strncpy(coding_type, token, end-token);
	coding_type[end-token] = '\0';

	token = ++end;

	*clockrate = (int)strtol(token, NULL, 10);

	end = strchr(token, '/');
	token = ++end;

	*coding_param = (int)strtol(token, NULL, 10);
	
	return UA_OK;
}

/**
 * Parse IPv4 string "a.b.c.d/n" into "a.b.c.d" and n (maskbits)
 */
int jua_parse_ipv4(char *addr, char *ip, int ipbytes, int *maskbits)
{
	char *end = strchr(addr, '/');

	if(end != NULL)
	{
		*maskbits = atoi(++end);
	}

	strncpy(ip, addr, end - addr);
	ip[end-addr] = '\0';

	return UA_OK;
}

/**
 * Parse sdp rtcp attr (a=rtcp:)
 * port: "53020"
 * ipv4: "53020 IN IP4 126.16.64.4"
 * ipv6: "53020 IN IP6 2001:2345:6789:ABCD:EF01:2345:6789:ABCD"
 */
int jua_parse_rtcp(char *rtcp, char *ip, int buflen, uint *port)
{
	char *token;
	*port = strtol(rtcp, NULL, 10);

	token = strstr(rtcp, "IP");

	while(*token != ' ' || *token != '\t')
		token++;
	while(*token == ' ' || *token == '\t')
		token++;

	strcpy(ip, token);

	return UA_OK;
}

static int jua_check_url(char *url)
{
	int i;
	osip_from_t *to;

	i = osip_from_init(&to);
	if (i!=0)
		return -1;

	i = osip_from_parse(to, url);
	if (i!=0)
		return -1;

	return 0;
}

/**
 * Processing the events, return the number of event happened.
 */
int jua_process_event(eXosipua_t *jua)
{	
	int counter =0;

	/* use events to print some info */
	eXosip_event_t *je;

	for (;;)
    {
		char buf[100];
		je = eXosip_event_wait(0,50);

		if (je==NULL)
			break;
      
		counter++;
		if (je->type==EXOSIP_CALL_NEW)
		{
			snprintf(buf, 99, "<- (%i %i) INVITE from: %s",
					je->cid, je->did, je->remote_uri);
	  
			jua_log((buf));
#if 0
			if (je->remote_sdp_audio_ip[0]!='\0')
			{
				snprintf(buf, 99, "<- Remote sdp info: %s:%i",
						je->remote_sdp_audio_ip, je->remote_sdp_audio_port);
	      
				jua_log((buf));
			}
#endif
			jcall_new(jua, je);
		}
		else if (je->type==EXOSIP_CALL_ANSWERED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
#if 0
			if (je->remote_sdp_audio_ip[0]!='\0')
			{
				snprintf(buf, 99, "<- Remote sdp info: %s:%i",
						je->remote_sdp_audio_ip, je->remote_sdp_audio_port);
	      
				jua_log((buf));
			}
#endif
			jcall_answered(jua, je);
		}
		else if (je->type==EXOSIP_CALL_PROCEEDING)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
#if 0
			if (je->remote_sdp_audio_ip[0]!='\0')
			{
				snprintf(buf, 99, "<- Remote sdp info: %s:%i",
						je->remote_sdp_audio_ip,
						je->remote_sdp_audio_port);
	      
				jua_log((buf));
			}
#endif
			jcall_proceeding(jua, je);
		}
		else if (je->type==EXOSIP_CALL_RINGING)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
#if 0
			if (je->remote_sdp_audio_ip[0]!='\0')
			{
				snprintf(buf, 99, "<- Remote sdp info: %s:%i",
						je->remote_sdp_audio_ip, je->remote_sdp_audio_port);
	      
				jua_log((buf));
			}
#endif
			jcall_ringing(jua, je);
		}
		else if (je->type==EXOSIP_CALL_REDIRECTED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			jcall_redirected(jua, je);
		}
		else if (je->type==EXOSIP_CALL_REQUESTFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			jcall_requestfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_SERVERFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			jcall_serverfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_GLOBALFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			jcall_globalfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_CLOSED)
		{
			snprintf(buf, 99, "<- (%i %i) BYE from: %s",
					je->cid, je->did, je->remote_uri);
	  
			jua_log((buf));
	
			jcall_closed(jua, je);
		}
		else if (je->type==EXOSIP_CALL_HOLD)
		{
			snprintf(buf, 99, "<- (%i %i) INVITE (On Hold) from: %s",
					je->cid, je->did, je->remote_uri);
	  
			jua_log((buf));

			jcall_onhold(jua, je);
		}
		else if (je->type==EXOSIP_CALL_OFFHOLD)
		{
			snprintf(buf, 99, "<- (%i %i) INVITE (Off Hold) from: %s",
					je->cid, je->did, je->remote_uri);
	  
			jua_log((buf));

			jcall_offhold(jua, je);
		}
		else if (je->type==EXOSIP_REGISTRATION_SUCCESS)
		{
			snprintf(buf, 99, "<- (%i) [%i %s] %s for REGISTER %s",
					je->rid, je->status_code, je->reason_phrase,
					je->remote_uri, je->req_uri);
	  
			jua_log((buf));

			jua->registration_status = je->status_code;
			
			snprintf(jua->registration_server, 100, "%s", je->req_uri);
	  
			if (je->reason_phrase!='\0')
				snprintf(jua->registration_reason_phrase, 100, "%s", je->reason_phrase);
			else 
				jua->registration_reason_phrase[0] = '\0';
		}
		else if (je->type==EXOSIP_REGISTRATION_FAILURE)
		{
			snprintf(buf, 99, "<- (%i) [%i %s] %s for REGISTER %s",
					je->rid, je->status_code, je->reason_phrase,
					je->remote_uri, je->req_uri);
	  
			jua_log((buf));

			jua->registration_status = je->status_code;
	  
			snprintf(jua->registration_server, 100, "%s", je->req_uri);
	  
			if (je->reason_phrase!='\0')
				snprintf(jua->registration_reason_phrase, 100, "%s", je->reason_phrase);
			else 
				jua->registration_reason_phrase[0] = '\0';
	  
		}
		else if (je->type==EXOSIP_OPTIONS_NEW)
		{
			int k;
	  
			snprintf(buf, 99, "<- (%i %i) OPTIONS from: %s",
					je->cid, je->did, je->remote_uri);
	  
			jua_log((buf));

			/* answer the OPTIONS method */
			/* 1: search for an existing call */
			for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
			{
				if (jua->jcalls[k].state != NOT_USED || jua->jcalls[k].cid==je->cid)
					break;
			}
	  
			eXosip_lock();
	  
			if (jua->jcalls[k].cid==je->cid)
			{
				/* already answered! */
			}
			else if (k==MAX_NUMBER_OF_CALLS)
			{
				/* answer 200 ok */
				eXosip_answer_options(je->cid, je->did, 200);
			}
			else
			{
				/* answer 486 ok */
				eXosip_answer_options(je->cid, je->did, 486);
			}
	  
			eXosip_unlock();
#if 0
			if (je->remote_sdp_audio_ip[0]!='\0')
			{
				snprintf(buf, 99, "<- Remote sdp info: %s:%i",
						je->remote_sdp_audio_ip, je->remote_sdp_audio_port);
	      
				jua_log((buf));
			}
#endif
		}
		else if (je->type==EXOSIP_OPTIONS_ANSWERED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
#if 0
			if (je->remote_sdp_audio_ip[0]!='\0')
			{
				snprintf(buf, 99, "<- Remote sdp info: %s:%i",
						je->remote_sdp_audio_ip, je->remote_sdp_audio_port);
	      
				jua_log((buf));
			}
#endif
		}
		else if (je->type==EXOSIP_OPTIONS_PROCEEDING)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
#if 0
			if (je->remote_sdp_audio_ip[0]!='\0')
			{
				snprintf(buf, 99, "<- Remote sdp info: %s:%i",
						je->remote_sdp_audio_ip, je->remote_sdp_audio_port);
	      
				jua_log((buf));
			}
#endif
		}
		else if (je->type==EXOSIP_OPTIONS_REDIRECTED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_OPTIONS_REQUESTFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_OPTIONS_SERVERFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_OPTIONS_GLOBALFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
			
			jua_log((buf));
		}
		else if (je->type==EXOSIP_INFO_NEW)
		{
			snprintf(buf, 99, "<- (%i %i) INFO from: %s",
					je->cid, je->did, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_INFO_ANSWERED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_INFO_PROCEEDING)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_INFO_REDIRECTED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_INFO_REQUESTFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_INFO_SERVERFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_INFO_GLOBALFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_ANSWERED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			snprintf(buf, 99, "<- (%i %i) online=%i [status: %i reason:%i]",
					je->sid, je->did, je->online_status,
					je->ss_status, je->ss_reason);
	  
			jua_log((buf));

			jsubscription_answered(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_PROCEEDING)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			jsubscription_proceeding(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_REDIRECTED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			jsubscription_redirected(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_REQUESTFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			jsubscription_requestfailure(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_SERVERFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			jsubscription_serverfailure(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_GLOBALFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			jua_log((buf));

			jsubscription_globalfailure(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_NOTIFY)
		{
			snprintf(buf, 99, "<- (%i %i) NOTIFY from: %s",
					je->sid, je->did, je->remote_uri);
	  
			jua_log((buf));

			snprintf(buf, 99, "<- (%i %i) online=%i [status: %i reason:%i]",
					je->sid, je->did, je->online_status,
					je->ss_status, je->ss_reason);
	  
			jua_log((buf));

			jsubscription_notify(jua, je);
		}
		else if (je->type==EXOSIP_IN_SUBSCRIPTION_NEW)
		{
			snprintf(buf, 99, "<- (%i %i) SUBSCRIBE from: %s",
					je->nid, je->did, je->remote_uri);
	  
			jua_log((buf));

			/* search for the user to see if he has been
				previously accepted or not! */

			eXosip_notify(je->did, EXOSIP_SUBCRSTATE_PENDING, EXOSIP_NOTIFY_AWAY);

			jinsubscription_new(jua, je);
		}
		else if (je->textinfo[0]!='\0')
		{
			snprintf(buf, 99, "(%i %i %i %i) %s", je->cid, je->sid, je->nid, je->did, je->textinfo);
	  
			jua_log((buf));
		}
	
		eXosip_event_free(je);
	}
  
	return counter;
}

int jua_loop(void *gen)
{
	eXosipua_t *jua = (eXosipua_t *)gen;

	rtime_t ms_remain;

	jua->run = 1;

	while(1)
	{
		if(jua->run == 0)
			break;

		jua_process_event(jua);

		time_msec_sleep(jua->clock, JUA_INTERVAL_MSEC, &ms_remain);
	}

	return UA_OK;
}

int sipua_start(sipua_t *sipua)
{
	eXosipua_t *jua = (eXosipua_t*)sipua;

	jua->thread = xthr_new(jua_loop, jua, XTHREAD_NONEFLAGS);

	if(jua->thread == NULL)
		return UA_FAIL;

	return UA_OK;
}

int sipua_stop(sipua_t *sipua)
{
	int th_ret;

	eXosipua_t *jua = (eXosipua_t*)sipua;

	jua->run = 0;

	if(jua->thread != NULL)
	{
		xthr_wait(jua->thread, &th_ret);

		jua->thread = NULL;
	}

	return UA_OK;
}

int sipua_done(sipua_t *sipua)
{
	eXosipua_t *jua = (eXosipua_t*)sipua;

	time_end(jua->clock);

	free(jua);

	return UA_OK;
}

int sipua_regist(sipua_t *sipua, char *registrar, char *id, int idbytes, int seconds, sipua_action_t *action)
{
	int ret;
	int regno = -1;

	eXosipua_t *jua = (eXosipua_t*)sipua;

	if(!action)
		return UA_IGNORE;
	
	eXosip_lock();

	if (jua->current_id[0] != '\0' && strncmp(jua->current_id, id, jua->current_idbytes) == 0)
    {
		eXosip_unlock();
		return UA_OK;
    }

	if(jua->owner != NULL && jua->owner[0] != '\0')
		regno = eXosip_register_init(id, registrar, jua->owner);
	else
		regno = eXosip_register_init(id, registrar, NULL);

	if (regno < 0)
	{
		eXosip_unlock();
		return UA_FAIL;
	}

	ret = eXosip_register(regno, seconds);

	if(ret == 0)
	{
		strncpy(jua->current_id, id, idbytes);
		jua->action = action;
	}

	eXosip_unlock();

	if(ret != 0)
		return UA_FAIL;
	
	return UA_OK;
}

int sipua_unregist(sipua_t *sipua, char *registrar, char *id, int idbytes)
{
	int ret;
	int regno = -1;

	eXosipua_t *jua = (eXosipua_t*)sipua;
	
	eXosip_lock();

	if (jua->current_id[0] == '\0' || jua->registration_server[0] == '\0')
    {
		eXosip_unlock();
		return UA_OK;
    }

	if(jua->owner[0] != '\0')
		regno = eXosip_register_init(jua->current_id, jua->registration_server, jua->owner);
	else
		regno = eXosip_register_init(jua->current_id, jua->registration_server, NULL);

	if (regno < 0)
	{
		eXosip_unlock();
		return UA_FAIL;
	}

	ret = eXosip_register(regno, 0);

	if(ret == 0)
	{
		strncpy(jua->current_id, 0, jua->current_idbytes);
		jua->action->done(jua->action);
		jua->action = NULL;
	}

	eXosip_unlock();

	if(ret != 0)
		return UA_FAIL;
	
	return UA_OK;
}

int sipua_connect(sipua_t *ua,
				char *from, int from_bytes,
				char *to, int to_bytes, 
				char *subject, int subject_bytes,
				char *route, int route_bytes)
{
	osip_message_t *invite;
	int ret;

	OSIP_TRACE (osip_trace(__FILE__, __LINE__, OSIP_INFO2, NULL, "To: |%s|\n", to));
	
	if (0!=jua_check_url(from))
		return UA_FAIL;

	if (0!=jua_check_url(to))
		return UA_FAIL;

#if 0
	if (0!=check_sipurl(route))
		return UA_FAIL;
#endif

	if (eXosip_build_initial_invite(&invite, to, from, route, subject) != 0)
    {
		return UA_FAIL;
    }

	eXosip_lock();

	ret = eXosip_initiate_call(invite, NULL, NULL, NULL);

	eXosip_unlock();  

	return ret;
}

jcall_t *jua_find_call(eXosipua_t *jua,
					 char *local, int local_bytes, 
					 char *remote, int remote_bytes,
					 char *subject, int subject_bytes)
{
	int i;

	for (i=0; i<MAX_NUMBER_OF_CALLS; i++)
	{
		jcall_t *call = &jua->jcalls[i];

		if (call->state != NOT_USED)
		{
			if(strcmp(call->local_uri, local)
				&& strcmp(call->remote_uri, remote)
				&& strncmp(call->subject, subject, subject_bytes))
				return call;
		}
	}
  
	return NULL;
}

int sipua_disconnect(sipua_t *sipua,
					 char *from, int from_bytes, 
					 char *to, int to_bytes,
					 char *subject, int subject_bytes)
{
	eXosipua_t *jua = (eXosipua_t*)sipua;
	
	jcall_t *call = jua_find_call(jua, from, from_bytes, to, to_bytes, subject, subject_bytes);

	if(call && eXosip_terminate_call(call->cid, call->did) == 0)
		jcall_remove(jua, call);

	return UA_OK;
}

sipua_t *sipua_new(uint16 sip_port, char *firewall, void *config)
{
	sipua_t *sipua;

	eXosipua_t *jua = malloc(sizeof(struct eXosipua_s));
	if(!jua)
	{
		jua_log(("sipua_new: No memory\n"));
		return NULL;
	}

	memset(jua, 0, sizeof(struct eXosipua_s));

	if (eXosip_init(stdin, stdout, sip_port) != 0)
    {
		jua_log(("sipua_new: could not initialize eXosip\n"));
		return NULL;
    }
  
	eXosip_set_mode(EVENT_MODE);

	if(firewall != NULL && firewall[0] != '\0')
		eXosip_set_firewallip(firewall);

	jua->clock = time_start();

	sipua = (sipua_t *)jua;

	jcall_init(jua);
	sipua->regist = sipua_regist;
	sipua->unregist = sipua_unregist;

	sipua->connect = sipua_connect;
	sipua->disconnect = sipua_disconnect;

	sipua->done = sipua_done;

	sipua_start(sipua);

	return sipua;
}

