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

#include <timedia/xstring.h>
#include <timedia/xmalloc.h>
#include <timedia/ui.h>

#define JUA_LOG

#ifdef JUA_LOG
 #define jua_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
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
			/*
			snprintf(buf, 99, "<- (%i %i) INVITE from: %s",
					je->cid, je->did, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_new(jua, je);
		}
		else if (je->type==EXOSIP_CALL_ANSWERED)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_answered(jua, je);
		}
		else if (je->type==EXOSIP_CALL_ACK)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_ack(jua, je);
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
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_ringing(jua, je);
		}
		else if (je->type==EXOSIP_CALL_REDIRECTED)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_redirected(jua, je);
		}
		else if (je->type==EXOSIP_CALL_REQUESTFAILURE)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_requestfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_SERVERFAILURE)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_serverfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_GLOBALFAILURE)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_globalfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_CLOSED)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) BYE from: %s",
					je->cid, je->did, je->remote_uri);
	  
            josua_printf(buf);
            */
	
			jcall_closed(jua, je);
		}
		else if (je->type==EXOSIP_CALL_HOLD)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) INVITE (On Hold) from: %s",
					je->cid, je->did, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_onhold(jua, je);
		}
		else if (je->type==EXOSIP_CALL_OFFHOLD)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) INVITE (Off Hold) from: %s",
					je->cid, je->did, je->remote_uri);
	  
            josua_printf(buf);
            */

			jcall_offhold(jua, je);
		}
		else if (je->type==EXOSIP_REGISTRATION_SUCCESS)
		{
			sipua_reg_event_t reg_e;
		
			reg_e.event.call_info = NULL;

			reg_e.event.type = SIPUA_EVENT_REGISTRATION_SUCCEEDED;
			reg_e.status_code = je->status_code;
			reg_e.server_info = je->reason_phrase;
			reg_e.server = je->req_uri;
			/*
			snprintf(buf, 99, "<- (%i) [%i %s] %s for REGISTER %s",
					je->rid, je->status_code, je->reason_phrase,
					je->remote_uri, je->req_uri);
			printf("jua_process_event: reg ok! [%s]\n", buf);
			*/
			jua->registration_status = je->status_code;
			
			snprintf(jua->registration_server, 100, "%s", je->req_uri);
	  
			if (je->reason_phrase!='\0')
				snprintf(jua->registration_reason_phrase, 100, "%s", je->reason_phrase);
			else 
				jua->registration_reason_phrase[0] = '\0';
	
			reg_e.event.from = jua->registration_server;
			reg_e.event.content = NULL;

			/* event back to sipuac */
			jua->sipuas.notify_event(jua->sipuas.lisener, &reg_e.event);
		}
		else if (je->type==EXOSIP_REGISTRATION_FAILURE)
		{
			sipua_reg_event_t reg_e;

			reg_e.event.type = SIPUA_EVENT_REGISTRATION_FAILURE;
			reg_e.status_code = je->status_code;
			reg_e.server_info = je->reason_phrase;
			reg_e.server = je->req_uri;

			reg_e.event.from = je->req_uri;
			reg_e.event.content = NULL;
			reg_e.event.call_info = NULL;

			/* event back to sipuac */
			jua->sipuas.notify_event(jua->sipuas.lisener, &reg_e.event);
		}
		else if (je->type==EXOSIP_OPTIONS_NEW)
		{
			int k;
	  
			/*
			snprintf(buf, 99, "<- (%i %i) OPTIONS from: %s",
					je->cid, je->did, je->remote_uri);
	  
            josua_printf(buf);
            */

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
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_OPTIONS_PROCEEDING)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_OPTIONS_REDIRECTED)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_OPTIONS_REQUESTFAILURE)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_OPTIONS_SERVERFAILURE)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_OPTIONS_GLOBALFAILURE)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
			
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_INFO_NEW)
		{
			snprintf(buf, 99, "<- (%i %i) INFO from: %s",
					je->cid, je->did, je->remote_uri);
	  
			/*josua_printf(buf);*/
		}
		else if (je->type==EXOSIP_INFO_ANSWERED)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_INFO_PROCEEDING)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_INFO_REDIRECTED)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
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
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_INFO_GLOBALFAILURE)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s",
					je->cid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_ANSWERED)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);

            snprintf(buf, 99, "<- (%i %i) online=%i [status: %i reason:%i]",
					je->sid, je->did, je->online_status,
					je->ss_status, je->ss_reason);
	  
            josua_printf(buf);
            */
			jsubscription_answered(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_PROCEEDING)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
			jsubscription_proceeding(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_REDIRECTED)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
			jsubscription_redirected(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_REQUESTFAILURE)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
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
			/*
			snprintf(buf, 99, "<- (%i %i) [%i %s] %s for SUBSCRIBE",
					je->sid, je->did, je->status_code,
					je->reason_phrase, je->remote_uri);
	  
            josua_printf(buf);
            */
			jsubscription_globalfailure(jua, je);
		}
		else if (je->type==EXOSIP_SUBSCRIPTION_NOTIFY)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) NOTIFY from: %s",
					je->sid, je->did, je->remote_uri);
	  
            josua_printf(buf);

			snprintf(buf, 99, "<- (%i %i) online=%i [status: %i reason:%i]",
					je->sid, je->did, je->online_status,
					je->ss_status, je->ss_reason);
	  
            josua_printf(buf);
            */

			jsubscription_notify(jua, je);
		}
		else if (je->type==EXOSIP_IN_SUBSCRIPTION_NEW)
		{
			/*
			snprintf(buf, 99, "<- (%i %i) SUBSCRIBE from: %s",
					je->nid, je->did, je->remote_uri);
	  
            josua_printf(buf);
            */

			/* search for the user to see if he has been
				previously accepted or not! */

			eXosip_notify(je->did, EXOSIP_SUBCRSTATE_PENDING, EXOSIP_NOTIFY_AWAY);
			jinsubscription_new(jua, je);
		}
		else if (je->textinfo[0]!='\0')
		{
			/*
			snprintf(buf, 99, "(%i %i %i %i) %s", je->cid, je->sid, je->nid, je->did, je->textinfo);
	  
            josua_printf(buf);
            */
		}
	
		eXosip_event_free(je);
	}
  
	return(counter);
}

int jua_loop(void *gen)
{
	eXosipua_t *jua = (eXosipua_t *)gen;

	jua->run = 1;

	while(1)
	{
		if(jua->run == 0)
			break;

		jua_process_event(jua);
	}

	return UA_OK;
}

int uas_start(sipua_uas_t *sipuas)
{
	eXosipua_t *jua = (eXosipua_t*)sipuas;

	jua->clock = time_start();

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

	time_end(jua->clock);

	if(jua->thread != NULL)
	{
		xthr_wait(jua->thread, &th_ret);

		jua->thread = NULL;
	}

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

int uas_regist(sipua_uas_t *sipuas, char *loc, char *registrar, char *id, int seconds)
{
	int ret;
	int regno = -1;

	eXosipua_t *jua = (eXosipua_t*)sipuas;

	char* siploc, *p;

	p = siploc = xmalloc(4+strlen(loc)+1+10+1);
	if(!siploc)
		return UA_FAIL;

	strcpy(p, "sip:");
	p += 4;
	strcpy(p, loc);
    while(*p) p++;
    snprintf(p, 12, ":%d", sipuas->portno);

	jua_log(("uas_regist: %s on %s within %ds\n", id, registrar, seconds));

	eXosip_lock();

	regno = eXosip_register_init(id, registrar, siploc);

	if (regno < 0)
	{
		eXosip_unlock();
		return UA_FAIL;
	}

	ret = eXosip_register(regno, seconds);

	eXosip_unlock();

	xfree(siploc);

	if(ret != 0)
	{
		jua_log(("uas_regist: ret=%d\n", ret));

		return UA_FAIL;
	}
		
	if(!jua->thread)
	{
		jua->thread = xthr_new(jua_loop, jua, XTHREAD_NONEFLAGS);

		if(!jua->thread)
			return UA_FAIL;
	}

	return UA_OK;
}

int uas_unregist(sipua_uas_t *sipuas, char *userloc, char *registrar, char *id)
{
	int ret;
	int regno = -1;
    /*
	eXosipua_t *jua = (eXosipua_t*)sipuas;
	*/
	eXosip_lock();

	regno = eXosip_register_init(userloc, registrar, id);

	if (regno < 0)
	{
		eXosip_unlock();
		return UA_FAIL;
	}

	ret = eXosip_register(regno, 0);

	eXosip_unlock();

	if(ret != 0)
		return UA_FAIL;
	
	return UA_OK;
}

int uas_invite(sipua_uas_t *sipuas, char *to, sipua_set_t* call_info, char* sdp_body, int sdp_bytes)
{
	eXosipua_t *jua = (eXosipua_t*)sipuas;
    
	osip_message_t *invite;
	char sdp_size[8];
	char* proxy;

	int ret;

	char *from = call_info->user_prof->regname;

	/*
	OSIP_TRACE (osip_trace(__FILE__, __LINE__, OSIP_INFO2, NULL, "To: |%s|\n", to));
	*/
	if (0!=jua_check_url(from))
	{
		jua_log(("uas_call: illigal sip id!\n"));
		return UA_FAIL;
	}

	if (0!=jua_check_url(to))
	{
		jua_log(("uas_invite: illigal sip destination\n"));
		return UA_FAIL;
	}

	if(jua->sipuas.proxy[0]=='\0')
		proxy = NULL;
	else
		proxy = jua->sipuas.proxy;

	sprintf(sdp_size,"%i", sdp_bytes);

	/*
	printf("uas_invite: [%s] from %s to %s, proxy[%s]\n", call_info->subject, from, to, proxy);
	printf("\n-------Initiate SDP [%d bytes]--------\n", sdp_bytes);
	printf("Callid[%s]\n", call_info->setid.id);
	printf("----------------------------------------\n");
	printf("%s", sdp_body);
	printf("----------------------------------------\n");
	getchar();
	*/

	if (eXosip_build_initial_invite(&invite, to, from, proxy, call_info->subject) != 0)
		return UA_FAIL;

	/* sdp content of the call */
	osip_message_set_content_type(invite, "application/sdp");
	osip_message_set_content_length(invite, sdp_size);
	osip_message_set_body(invite, sdp_body);

	eXosip_lock();

	ret = eXosip_initiate_call(invite, call_info, NULL/*negotiation_reference*/, NULL/*local_audio_port*/);

	eXosip_unlock();
	
	/*When to free it ???
	osip_message_free(invite);
	*/

	return ret;
}

int uas_accept(sipua_uas_t* uas, int lineno)
{
	eXosipua_t *jua = (eXosipua_t*)uas;
	
	jua->ncall++;
	
	return UA_OK;
}

int uas_answer(sipua_uas_t* uas, sipua_set_t* call, int reply, char* reply_type, char* reply_body)
{
	if(reply_body)
	{
		if(eXosip_answer_call_with_body(call->did, reply, reply_type, reply_body) == 0)
			return UA_OK;
	}
	else
	{
		if(eXosip_answer_call(call->did, reply, NULL) == 0)
			return UA_OK;
	}

	return UA_FAIL;
}

int uas_bye(sipua_uas_t* uas, sipua_set_t* call)
{
    /*
    eXosipua_t *jua = (eXosipua_t*)uas;
	*/
	if(eXosip_terminate_call(call->cid, call->did) == 0)
		return UA_OK;

	return UA_FAIL;
}

int uas_match_type(sipua_uas_t* uas, char *type)
{
    if(strcmp(type, "eXosipua")==0)
        return 1;

	return 0;
}

int uas_init(sipua_uas_t* uas, int sip_port, char* nettype, char* addrtype, char* firewall, char* proxy)
{

	int ip_family;

	if(strcmp(nettype, "IN") != 0)
	{
		jua_log(("sipua_uas: Current, Only IP networking supported\n"));
		return UA_FAIL;
	}

	if(strcmp(addrtype, "IP4") != 0)
	{
		jua_log(("sipua_uas: Current, Only IPv4 networking supported\n"));
		return UA_FAIL;
	}
		
	ip_family = AF_INET;

	if (eXosip_init(stdin, stdout, sip_port) != 0)
    {
		jua_log(("sipua_uas: could not initialize eXosip\n"));
		return UA_FAIL;
    }
  
	/* detect local address */

	eXosip_guess_ip_for_via(ip_family, uas->netaddr, 63);
	if (uas->netaddr[0]=='\0')
    {
		jua_log(("sipua_uas: No ethernet interface found!\n"));
		jua_log(("sipua_uas: using ip[127.0.0.1] (debug mode)!\n"));

		strcpy(uas->netaddr, "127.0.0.1");
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

	return UA_OK;
}

int uas_done(sipua_uas_t *sipuas)
{
	xfree(sipuas);

	return UA_OK;
}

module_interface_t* sipua_new_server()
{
	eXosipua_t *jua;

	sipua_uas_t *uas;

	jua = xmalloc(sizeof(eXosipua_t));
	if(!jua)
	{
		jua_log(("sipua_new: No memory\n"));
		return NULL;
	}
	memset(jua, 0, sizeof(eXosipua_t));

	uas = (sipua_uas_t*)jua;

	uas->match_type = uas_match_type;

    uas->init = uas_init;
	uas->done = uas_done;
    
	uas->start = uas_start;
	uas->shutdown = uas_shutdown;

	uas->address = uas_address;

	uas->add_coding = uas_add_coding;
	uas->clear_coding = uas_clear_coding;

	uas->set_listener = uas_set_lisener;

	uas->regist = uas_regist;
	uas->unregist = uas_unregist;

	uas->accept = uas_accept;

	uas->invite = uas_invite;
	uas->answer = uas_answer;
	uas->bye = uas_bye;

	return uas;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC module_loadin_t mediaformat =
{
   "uas",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   sipua_new_server   /* Module initializer */
};
