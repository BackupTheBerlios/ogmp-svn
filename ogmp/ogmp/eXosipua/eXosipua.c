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
#include <timedia/xmalloc.h>

#define JUA_LOG

#ifdef JUA_LOG
 #define jua_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define jua_log(fmtargs)
#endif

#define JUA_INTERVAL_MSEC 1000

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
	  
			/*josua_printf(buf);*/

			jcall_new(jua, je);
		}
		else if (je->type==EXOSIP_CALL_ANSWERED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/

			jcall_answered(jua, je);
		}
		else if (je->type==EXOSIP_CALL_PROCEEDING)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
			josua_printf(buf);
			*/
			jcall_proceeding(jua, je);
		}
		else if (je->type==EXOSIP_CALL_RINGING)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/

			jcall_ringing(jua, je);
		}
		else if (je->type==EXOSIP_CALL_REDIRECTED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/

			jcall_redirected(jua, je);
		}
		else if (je->type==EXOSIP_CALL_REQUESTFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/

			jcall_requestfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_SERVERFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/

			jcall_serverfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_GLOBALFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/

			jcall_globalfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_CLOSED)
		{
			snprintf(buf, 99, "<- (%i %i) BYE from: %s",
					je->cid, je->did, je->remote_uri);
	  
			/*josua_printf(buf);*/
	
			jcall_closed(jua, je);
		}
		else if (je->type==EXOSIP_CALL_HOLD)
		{
			snprintf(buf, 99, "<- (%i %i) INVITE (On Hold) from: %s",
					je->cid, je->did, je->remote_uri);
	  
			/*josua_printf(buf);*/

			jcall_onhold(jua, je);
		}
		else if (je->type==EXOSIP_CALL_OFFHOLD)
		{
			snprintf(buf, 99, "<- (%i %i) INVITE (Off Hold) from: %s",
					je->cid, je->did, je->remote_uri);
	  
			/*josua_printf(buf);*/

			jcall_offhold(jua, je);
		}
		else if (je->type==EXOSIP_REGISTRATION_SUCCESS)
		{
			sipua_event_t sip_e;
			sip_e.call_info = (sipua_set_t*)je->external_reference;
			sip_e.type = SIPUA_EVENT_REGISTRATION_SUCCEEDED;

			snprintf(buf, 99, "<- (%i) [%i %s] %s for REGISTER %s",
					je->rid, je->status_code, je->reason_phrase,
					je->remote_uri, je->req_uri);
	  
			/*josua_printf(buf);*/

			jua->registration_status = je->status_code;
			
			snprintf(jua->registration_server, 100, "%s", je->req_uri);
	  
			if (je->reason_phrase!='\0')
				snprintf(jua->registration_reason_phrase, 100, "%s", je->reason_phrase);
			else 
				jua->registration_reason_phrase[0] = '\0';
	
			sip_e.from = jua->registration_server;
			sip_e.content = NULL;

			/* event back to sipuac */
			jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);
		}
		else if (je->type==EXOSIP_REGISTRATION_FAILURE)
		{
			sipua_event_t sip_e;
			sip_e.call_info = (sipua_set_t*)je->external_reference;
			sip_e.type = SIPUA_EVENT_REGISTRATION_FAILURE;

			snprintf(buf, 99, "<- (%i) [%i %s] %s for REGISTER %s",
					je->rid, je->status_code, je->reason_phrase,
					je->remote_uri, je->req_uri);
	  
			/*josua_printf(buf);*/

			jua->registration_status = je->status_code;
	  
			snprintf(jua->registration_server, 100, "%s", je->req_uri);
	  
			if (je->reason_phrase!='\0')
				snprintf(jua->registration_reason_phrase, 100, "%s", je->reason_phrase);
			else 
				jua->registration_reason_phrase[0] = '\0';
	  
			sip_e.from = jua->registration_server;
			sip_e.content = NULL;

			/* event back to sipuac */
			jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);
		}
		else if (je->type==EXOSIP_OPTIONS_NEW)
		{
			int k;
	  
			snprintf(buf, 99, "<- (%i %i) OPTIONS from: %s",
					je->cid, je->did, je->remote_uri);
	  
			/*josua_printf(buf);*/

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
		}
		else if (je->type==EXOSIP_OPTIONS_ANSWERED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_OPTIONS_PROCEEDING)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_OPTIONS_REDIRECTED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_OPTIONS_REQUESTFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_OPTIONS_SERVERFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_OPTIONS_GLOBALFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
			
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_INFO_NEW)
		{
			snprintf(buf, 99, "<- (%i %i) INFO from: %s",
					je->cid, je->did, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_INFO_ANSWERED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_INFO_PROCEEDING)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_INFO_REDIRECTED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_INFO_REQUESTFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_INFO_SERVERFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_INFO_GLOBALFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_ANSWERED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
			snprintf(buf, 99, "<- (%i %i) online=%i [status: %i reason:%i]",
					je->sid, je->did, je->online_status,
					je->ss_status, je->ss_reason);
	  
			/*josua_printf(buf);*/
			jsubscription_answered(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_PROCEEDING)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
			jsubscription_proceeding(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_REDIRECTED)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
			jsubscription_redirected(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_REQUESTFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
			jsubscription_requestfailure(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_SERVERFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
			jsubscription_serverfailure(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_GLOBALFAILURE)
		{
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
			/*josua_printf(buf);*/
			jsubscription_globalfailure(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_NOTIFY)
		{
			snprintf(buf, 99, "<- (%i %i) NOTIFY from: %s",
					je->sid, je->did, je->remote_uri);
	  
			/*josua_printf(buf);*/

			snprintf(buf, 99, "<- (%i %i) online=%i [status: %i reason:%i]",
					je->sid, je->did, je->online_status,
					je->ss_status, je->ss_reason);
	  
			/*josua_printf(buf);*/

			jsubscription_notify(jua, je);
		}
		else if (je->type==EXOSIP_IN_SUBSCRIPTION_NEW)
		{
			snprintf(buf, 99, "<- (%i %i) SUBSCRIBE from: %s",
					je->nid, je->did, je->remote_uri);
	  
			/*josua_printf(buf);*/

			/* search for the user to see if he has been
				previously accepted or not! */

			eXosip_notify(je->did, EXOSIP_SUBCRSTATE_PENDING, EXOSIP_NOTIFY_AWAY);
			jinsubscription_new(jua, je);
		}
		else if (je->textinfo[0]!='\0')
		{
			snprintf(buf, 99, "(%i %i %i %i) %s", je->cid, je->sid, je->nid, je->did, je->textinfo);
	  
			/*josua_printf(buf);*/
		}
	
		eXosip_event_free(je);
	}
  
	return(counter);
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

int uas_start(sipua_uas_t *sipuas)
{
	eXosipua_t *jua = (eXosipua_t*)sipuas;

	jua->thread = xthr_new(jua_loop, jua, XTHREAD_NONEFLAGS);

	if(jua->thread == NULL)
		return UA_FAIL;

	return UA_OK;
}

int uas_shutdown(sipua_uas_t *sipuas)
{
	int th_ret;

	eXosipua_t *jua = (eXosipua_t*)sipuas;

	jua->run = 0;

	if(jua->thread != NULL)
	{
		xthr_wait(jua->thread, &th_ret);

		jua->thread = NULL;
	}

	return UA_OK;
}

int uas_done(sipua_uas_t *sipuas)
{
	eXosipua_t *jua = (eXosipua_t*)sipuas;

	time_end(jua->clock);

	free(jua);

	return UA_OK;
}

int uas_address(sipua_uas_t* sipuas, char* *nettype, char* *addrtype, char* *netaddr)
{
	sipua_uas_t* uas = (sipua_uas_t*)sipuas;

	*nettype = uas->nettype;
	*addrtype = uas->addrtype;
	*netaddr = uas->netaddr;

	return UA_OK;
}

int uas_set_lisener(sipua_uas_t* sipuas, void* lisener, int(*notify_event)(void*, sipua_event_t*))
{
	sipua_uas_t* uas = (sipua_uas_t*)sipuas;

	uas->lisener = lisener;

	uas->notify_event = notify_event;
	
	return UA_OK;
}

int uas_add_coding(sipua_uas_t* sipuas, int pt, int rtp_portno, int rtcp_portno, char* mime, int clockrate, int param)
{
	eXosipua_t* jua = (eXosipua_t*)sipuas;

	char pt_a[4];
	char rtp_a[8], rtcp_a[8];
	char rtpmap_a[32];
	char nport_a[2];

	char coding[16], *p, *q;

	snprintf(pt_a, 4, "%i", pt);
	snprintf(rtp_a, 8, "%i", rtp_portno);
	snprintf(rtcp_a, 8, "%i", rtcp_portno);

	p = strchr(mime, '/');
	p++;

	q = p;
	while(*q && *q != '/')
		q++;

	strncpy(coding, p, q-p);
	coding[q-p] = '\0';

	snprintf(rtpmap_a, 32, "%i %s/%i/%i", pt, coding, clockrate, param);

	if(rtp_portno - rtcp_portno == 1)
		snprintf(nport_a, 2, "%i", 2);
	else
		snprintf(nport_a, 2, "%i",1);
	
	eXosip_sdp_negotiation_add_codec(osip_strdup(pt_a),
				   osip_strdup(nport_a),
				   osip_strdup("RTP/AVP"),
				   osip_strdup(jua->sipuas.nettype), osip_strdup(jua->sipuas.addrtype), osip_strdup(jua->sipuas.netaddr),
				   NULL, NULL,
				   osip_strdup(rtpmap_a));

	return UA_OK;
}

int uas_clear_coding(sipua_uas_t* sipuas)
{
	eXosip_sdp_negotiation_remove_audio_payloads();

	return UA_OK;
}

int uas_regist(sipua_uas_t *sipuas, char *registrar, char *id, int seconds)
{
	int ret;
	int regno = -1;

	eXosipua_t *jua = (eXosipua_t*)sipuas;

	jua_log(("uas_regist: %s on %s within %ds\n", id, registrar, seconds));

	eXosip_lock();

	if (jua->current_id[0] != '\0' && strcmp(jua->current_id, id) == 0)
    {
		eXosip_unlock();
		return UA_OK;
    }

	if(jua->owner || jua->owner[0] != '\0')
		regno = eXosip_register_init(id, registrar, jua->owner);
	else
		regno = eXosip_register_init(id, registrar, NULL);

	if (regno < 0)
	{
		eXosip_unlock();
		return UA_FAIL;
	}

	ret = eXosip_register(regno, seconds);

	jua_log(("uas_regist: ret=%d\n", ret));

	if(ret == 0)
		strcpy(jua->current_id, id);

	jua_log(("uas_regist: 6\n"));

	eXosip_unlock();

	jua_log(("uas_regist: 7\n"));

	if(ret != 0)
		return UA_FAIL;
	
	jua_log(("uas_regist: 8\n"));

	return UA_OK;
}

int uas_unregist(sipua_uas_t *sipuas, char *registrar, char *id)
{
	int ret;
	int regno = -1;

	eXosipua_t *jua = (eXosipua_t*)sipuas;
	
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
		strcpy(jua->current_id, "");
	}

	eXosip_unlock();

	if(ret != 0)
		return UA_FAIL;
	
	return UA_OK;
}

int uas_call(sipua_uas_t *sipuas, char *to, sipua_set_t* call_info, char* sdp_body, int sdp_bytes)
{
	eXosipua_t *jua = (eXosipua_t*)sipuas;
	osip_message_t *invite;
	char sdp_size[8];
	char* proxy;

	int ret;
	/*
	OSIP_TRACE (osip_trace(__FILE__, __LINE__, OSIP_INFO2, NULL, "To: |%s|\n", to));
	*/
	if(jua->current_id[0] == '\0')
	{
		jua_log(("uas_call: sipua is not register yet!\n"));
		return UA_FAIL;
	}

	if (0!=jua_check_url(jua->current_id))
	{
		jua_log(("uas_call: illigal sip id!\n"));
		return UA_FAIL;
	}

	if (0!=jua_check_url(to))
	{
		jua_log(("uas_call: illigal sip destination\n"));
		return UA_FAIL;
	}

	if(jua->sipuas.proxy[0]=='\0')
		proxy = NULL;
	else
		proxy = jua->sipuas.proxy;

	if (eXosip_build_initial_invite(&invite, to, jua->current_id, proxy, call_info->subject) != 0)
		return UA_FAIL;

	jua_log(("uas_call: 4\n"));

	/* sdp content of the call */
	sprintf(sdp_size,"%i", sdp_bytes);

	osip_message_set_content_type(invite, "application/sdp");
	osip_message_set_content_length(invite, sdp_size);
	osip_message_set_body(invite, sdp_body);

	eXosip_lock();

	ret = eXosip_initiate_call(invite, call_info, NULL/*negotiation_reference*/, NULL/*local_audio_port*/);

	eXosip_unlock();  

	return ret;
}

int sipuas_hangup(sipua_t *sipua, int i)
{
	eXosipua_t *jua = (eXosipua_t*)sipua;
	
	jcall_t *call = &jua->jcalls[i];

	if(call && eXosip_terminate_call(call->cid, call->did) == 0)
		jcall_remove(jua, call);

	return UA_OK;
}

sipua_uas_t* sipua_uas(int sip_port, char* nettype, char* addrtype, char* firewall, char* proxy)
{
	eXosipua_t *jua;

	sipua_uas_t *uas;

	int ip_family;

	if(strcmp(nettype, "IN") != 0)
	{
		jua_log(("sipua_uas: Current, Only IP networking supported\n"));
		return NULL;
	}

	if(strcmp(addrtype, "IP4") != 0)
	{
		jua_log(("sipua_uas: Current, Only IPv4 networking supported\n"));
		return NULL;
	}
		
	ip_family = AF_INET;

	if (eXosip_init(stdin, stdout, sip_port) != 0)
    {
		jua_log(("sipua_uas: could not initialize eXosip\n"));
		return NULL;
    }
  
	jua = xmalloc(sizeof(eXosipua_t));
	if(!jua)
	{
		jua_log(("sipua_new: No memory\n"));
		return NULL;
	}

	memset(jua, 0, sizeof(eXosipua_t));

	uas = (sipua_uas_t*)jua;

	uas->start = uas_start;
	uas->shutdown = uas_shutdown;

	uas->address = uas_address;

	uas->add_coding = uas_add_coding;
	uas->clear_coding = uas_clear_coding;

	uas->set_listener = uas_set_lisener;

	uas->regist = uas_regist;
	uas->unregist = uas_unregist;

	uas->call = uas_call;

	/* detect local address */
	eXosip_guess_ip_for_via(ip_family, uas->netaddr, 63);
	if (uas->netaddr[0]=='\0')
    {
		jua_log(("sipua_uas: No ethernet interface found!\n"));
		jua_log(("sipua_uas: using ip[127.0.0.1] (debug mode)!\n"));

		strncpy(uas->netaddr, "127.0.0.1", MAX_IP_BYTES);
    }
	else
	{
		jua_log(("sipua_uas: local address[%s]\n", uas->netaddr));
	}

	strcpy(uas->nettype, nettype);
	strcpy(uas->addrtype, addrtype);

	if(firewall)
		strcpy(uas->firewall, firewall);

	if(proxy)
		strcpy(uas->proxy, proxy);

	uas->portno = sip_port;
	
	jua_log(("sipua_uas: uas ready\n"));

	return uas;
}
