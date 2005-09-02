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

#include <stdlib.h>

#include <timedia/xmalloc.h>
#include "eXosipua.h"

int jcall_init(eXosipua_t *jua)
{
	int k;
	for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
		memset(&(jua->jcalls[k]), 0, sizeof(jcall_t));
		jua->jcalls[k].state = NOT_USED;
    }
	/*
	eXosip_get_localip(jua->sipuas.address);
	*/
	return 0;
}

void __call_free()
{
}

#if defined(ORTP_SUPPORT) /*|| defined(WIN32)*/
void
rcv_telephone_event(RtpSession *rtp_session, jcall_t *ca)
{
  /* telephone-event received! */
}
#endif

int jcall_sdp_body(eXosipua_t *jua, eXosip_event_t *je, const char **sdp_body)
{
	int pos;
	osip_body_t *sdp;

	osip_list_t *bodies = je->response->bodies;

	pos = 0;
	while (!osip_list_eol (bodies, pos))
	{
		sdp = (osip_body_t*)osip_list_get(bodies, pos);

		if(0 == strcmp(sdp->content_type->type, "application") && 0 == strcmp(sdp->content_type->subtype, "sdp"))
		{
			*sdp_body = sdp->body;
			return sdp->length;
		}

		pos++;
	}

	*sdp_body = NULL;
	return 0;
}

int jcall_osip_subject (eXosipua_t *jua, eXosip_event_t *je, const char **subject)
{
	int pos;
	osip_header_t *h;

	osip_list_t *headers = je->response->headers;

	pos = 0;
	while (!osip_list_eol (headers, pos))
	{
		h = (osip_header_t*)osip_list_get(headers, pos);

		if(0 == strcmp(h->hname, "Subject"))
		{
			*subject = h->hvalue;
			return strlen(*subject);
		}

		pos++;
	}

	*subject = NULL;
	return 0;
}

int
jcall_get_number_of_pending_calls(eXosipua_t *jua)
{
	int pos=0;
	int k;
	for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
		if (jua->jcalls[k].state != NOT_USED)
		{
			pos++;
		}
    }

	return pos;
}

jcall_t *jcall_find_call(eXosipua_t *jua, int pos)
{
  int k;
  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
  {
      if (jua->jcalls[k].state != NOT_USED)
	  {
		if (pos==0)
			return &(jua->jcalls[k]);
		pos--;
	  }
  }
  return NULL;
}

int jcall_new(eXosipua_t *jua, eXosip_event_t *je)
{
	/*event back to sipuac */
	osip_message_t *message = NULL;
	osip_from_t  *from = NULL;
	osip_uri_t *from_uri = NULL;
 
	sipua_call_event_t call_e;
 
	memset(&call_e, 0, sizeof(sipua_call_event_t));

	if(jua->ncall == MAX_SIPUA_LINES-1)
	{
		/* All lines are busy */
		eXosip_call_send_answer (je->tid, SIP_STATUS_CODE_BUSYHERE, NULL);
		return 0;
	}

	call_e.event.type = SIPUA_EVENT_NEW_CALL;
	jcall_sdp_body(jua, je, (const char**)&call_e.event.content);

	call_e.tid = je->tid;
	call_e.cid = je->cid;
	call_e.did = je->did;
 
	call_e.sbytes = jcall_osip_subject(jua, je, (const char**)&call_e.subject);
	call_e.textinfo = je->textinfo;
 
	call_e.local_uri = NULL;

	message = je->request;
	if(message)
	{
		call_e.req_uri = message->req_uri->string;
		call_e.local_uri = message->req_uri->string;
		
		from = osip_message_get_from(message);
		
		if(from)
		{
			from_uri = osip_from_get_url(from);
			osip_uri_to_str(from_uri, &call_e.remote_uri);
		}
	}

	if (je->response->reason_phrase[0]!='\0')
		call_e.reason_phrase = je->response->reason_phrase;

	call_e.status_code = je->response->status_code;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &call_e.event);

	return 0;
}

int jcall_closed(eXosipua_t *jua, eXosip_event_t *je)
{
	/* event back to sipuac */
	sipua_call_event_t call_e;

	memset(&call_e, 0, sizeof(sipua_call_event_t));

	call_e.tid = je->tid;
	call_e.cid = je->cid;
	call_e.did = je->did;

	call_e.event.call_info = (sipua_call_t*)je->external_reference;
	call_e.event.type = SIPUA_EVENT_CALL_CLOSED;
	call_e.status_code = je->response->status_code;
 
	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &call_e.event);

	return 0;
}

int jcall_remove(eXosipua_t *jua, jcall_t *ca)
{
	if (ca==NULL)
		return -1;

#if defined(MEDIASTREAMER_SUPPORT)
	if (ca->enable_audio>0)
    {
		ca->enable_audio = -1;
		os_sound_close(ca);
    }
#elif defined(XRTP_SUPPORT)
	{
		if(ca->callin_info)
			rtp_capable_done_set(ca->callin_info);
	}
#endif

	ca->state = NOT_USED;
	return 0;
}

int jcall_proceeding(eXosipua_t *jua, eXosip_event_t *je)
{
	/* event back to sipuac */
	sipua_call_event_t call_e;

	call_e.event.type = SIPUA_EVENT_PROCEEDING;
	call_e.event.call_info = (sipua_call_t*)je->external_reference;

	call_e.sbytes = jcall_osip_subject(jua, je, (const char**)&call_e.subject);
	call_e.textinfo = je->textinfo;
 
	call_e.req_uri = je->response->req_uri->string;
	call_e.remote_uri = call_e.req_uri;

	if(je->request)
		call_e.local_uri = je->request->req_uri->string;
	else
		call_e.local_uri = NULL;

	if (je->response->reason_phrase[0]!='\0')
	{
		call_e.reason_phrase = je->response->reason_phrase;
		call_e.status_code = je->response->status_code;
	}

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &call_e.event);

	return 0;
}

int jcall_ringing(eXosipua_t *jua, eXosip_event_t *je)
{
	/* event back to sipuac */
	sipua_call_event_t call_e;

	call_e.event.type = SIPUA_EVENT_RINGING;
	call_e.event.call_info = (sipua_call_t*)je->external_reference;

	call_e.sbytes = jcall_osip_subject(jua, je, (const char**)&call_e.subject);
	call_e.textinfo = je->textinfo;
 
	call_e.req_uri = je->response->req_uri->string;
	call_e.local_uri = je->request->req_uri->string;
	call_e.remote_uri = call_e.req_uri;

	if (je->response->reason_phrase[0]!='\0')
	{
		call_e.reason_phrase = je->response->reason_phrase;
		call_e.status_code = je->response->status_code;
	}

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &call_e.event);

	return 0;
}

int jcall_answered(eXosipua_t *jua, eXosip_event_t *je)
{
	sipua_call_event_t call_e;

	/* event back to sipuac */
	memset(&call_e, 0, sizeof(sipua_call_event_t));

	call_e.event.call_info = (sipua_call_t*)je->external_reference;

	call_e.event.type = SIPUA_EVENT_ANSWERED;
	jcall_sdp_body(jua, je, (const char**)&call_e.event.content);

	if (je->response->reason_phrase[0]!='\0')
	{
		call_e.reason_phrase = je->response->reason_phrase;
		call_e.status_code = je->response->status_code;
	}
			
#if 0
	{
		osip_message_t *ack;

		generating_ack_for_2xx(&ack, je->osip_dialog_t *dialog);
  
		i = osip_transaction_init(&transaction, NICT, eXosip.j_osip, ack);
		if (i!=0)
		{
			/* TODO: release the j_call.. */

			osip_message_free(reg);
		}
		else
		{
			jr->r_last_tr = transaction;
		}
	}
#endif

/*
#ifdef MEDIASTREAMER_SUPPORT
	if (ca->remote_sdp_audio_ip[0]=='\0')
    {
		osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
		ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
		ca->payload = je->payload;
		osip_strncpy(ca->payload_name, je->payload_name, 49);

		if (0==os_sound_start(ca))
			ca->enable_audio=1; //audio is started
    }
#endif
*/

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &call_e.event);

	return 0;
}

int jcall_ack(eXosipua_t *jua, eXosip_event_t *je)
{
	sipua_event_t e;
   
	sipua_call_t* call;

    /* event back to sipuac */
	memset(&e, 0, sizeof(sipua_event_t));

	call = e.call_info = (sipua_call_t*)je->external_reference;

	if (je->cid != call->cid && je->did != call->did)
		return -1; 
	
	e.type = SIPUA_EVENT_ACK;
	e.content = NULL;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &e);

	return 0;
}

int jcall_redirected(eXosipua_t *jua, eXosip_event_t *je)
{
	jcall_t *ca;
	int k;

	/* event back to sipuac */
	sipua_event_t sip_e;
	sip_e.call_info = (sipua_call_t*)je->external_reference;
	sip_e.type = SIPUA_EVENT_REDIRECTED;

	for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
		if (jua->jcalls[k].state != NOT_USED
				&& jua->jcalls[k].cid==je->cid
				&& jua->jcalls[k].did==je->did)
			break;
    }

	if (k==MAX_NUMBER_OF_CALLS)
		return -1;

	sip_e.lineno = k;

	ca = &(jua->jcalls[k]);

#if defined(MEDIASTREAMER_SUPPORT)
  
	if (ca->enable_audio>0)
    {
		ca->enable_audio = -1;
		os_sound_close(ca);
    }

#elif defined(XRTP_SUPPORT)
			
	if(ca->callin_info)
	{
		rtp_capable_done_set(ca->callin_info);
				
		ca->callin_info = NULL;
	}

#endif

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);

	ca->state = NOT_USED;
  
	return 0;
}

char* jcall_dequota_copy(char *quota)
{
   char *start;
   char *end;
   
   start = quota;

   while(*start == ' ' || *start == '\t' || *start == '\n') start++;

   if(*start != '\"')
   {
      return xstr_clone(quota);
   }
   start++;
   end = start;
   
   while(*end && *end != '\"')
      end++;

   return xstr_nclone(start, end-start);
}

int jcall_requestfailure(eXosipua_t *jua, eXosip_event_t *je)
{
	/* event back to sipuac */
	sipua_call_event_t call_e; 
	osip_message_t *message = je->response;

	call_e.auth_realm = NULL;

	call_e.event.type = SIPUA_EVENT_REQUESTFAILURE;
	call_e.event.call_info = (sipua_call_t*)je->external_reference;

	call_e.tid = je->tid;
	call_e.cid = je->cid;
	call_e.did = je->did;
 
	call_e.event.call_info->cid = je->cid;
	call_e.event.call_info->did = je->did;

	jcall_osip_subject(jua, je, (const char**)&call_e.subject);
	call_e.textinfo = je->textinfo;
 
	call_e.req_uri = je->response->req_uri->string;
	call_e.local_uri = je->request->req_uri->string;
	call_e.remote_uri = call_e.req_uri;

	if (je->response->reason_phrase[0]!='\0')
	{
		call_e.reason_phrase = je->response->reason_phrase;
		call_e.status_code = je->response->status_code;
	}
	else
	{
		call_e.reason_phrase = NULL;
		call_e.status_code = SIPUA_STATUS_UNKNOWN;
	}

	if(je->response->status_code == SIP_STATUS_CODE_PROXYAUTHENTICATIONREQUIRED && message)
	{
		osip_proxy_authenticate_t *proxy_auth;

		proxy_auth = (osip_proxy_authenticate_t*)osip_list_get(message->proxy_authenticates,0);
		call_e.auth_realm = jcall_dequota_copy(osip_proxy_authenticate_get_realm(proxy_auth));
	}

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &call_e.event);

	if(call_e.auth_realm)
	{
		xfree(call_e.auth_realm);
		call_e.auth_realm = NULL;
	}

	return 0;
}

int jcall_serverfailure(eXosipua_t *jua, eXosip_event_t *je)
{
	/* event back to sipuac */
	sipua_call_event_t call_e;

	call_e.event.type = SIPUA_EVENT_SERVERFAILURE;
	call_e.event.call_info = (sipua_call_t*)je->external_reference;

	call_e.sbytes = jcall_osip_subject(jua, je, (const char**)&call_e.subject);
	call_e.textinfo = je->textinfo;
 
	call_e.req_uri = je->response->req_uri->string;
	call_e.local_uri = je->request->req_uri->string;
	call_e.remote_uri = call_e.req_uri;

	if (je->response->reason_phrase[0]!='\0')
	{
		call_e.reason_phrase = je->response->reason_phrase;
		call_e.status_code = je->response->status_code;
	}
	else
	{
		call_e.reason_phrase = NULL;
		call_e.status_code = SIPUA_STATUS_UNKNOWN;
	}

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &call_e.event);

	return 0;
}

int jcall_globalfailure(eXosipua_t *jua, eXosip_event_t *je)
{
	/* event back to sipuac */
	sipua_call_event_t call_e;

	call_e.event.type = SIPUA_EVENT_GLOBALFAILURE;
	call_e.event.call_info = (sipua_call_t*)je->external_reference;

	call_e.sbytes = jcall_osip_subject(jua, je, (const char**)&call_e.subject);
	call_e.textinfo = je->textinfo;

	call_e.req_uri = je->response->req_uri->string;
	call_e.local_uri = je->request->req_uri->string;
	call_e.remote_uri = call_e.req_uri;

	if (je->response->reason_phrase[0]!='\0')
	{
		call_e.reason_phrase = je->response->reason_phrase;
		call_e.status_code = je->response->status_code;
	}
	else
	{
		call_e.reason_phrase = NULL;
		call_e.status_code = SIPUA_STATUS_UNKNOWN;
	}

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &call_e.event);

	return 0;
}

int jsubscription_requestfailure(eXosipua_t *jua, eXosip_event_t *je)
{
	return 0;
}

int jsubscription_serverfailure(eXosipua_t *jua, eXosip_event_t *je)
{
	return 0;
}

int jsubscription_globalfailure(eXosipua_t *jua, eXosip_event_t *je)
{
	return 0;
}

int jsubscription_new(eXosipua_t *jua, eXosip_event_t *je)
{
	return 0;
}

int jsubscription_answered(eXosipua_t *jua, eXosip_event_t *je)
{
	return 0;
}

int jsubscription_proceeding(eXosipua_t *jua, eXosip_event_t *je)
{
	return 0;
}

int jsubscription_notify(eXosipua_t *jua, eXosip_event_t *je)
{
	return 0;
}

int jsubscription_redirected(eXosipua_t *jua, eXosip_event_t *je)
{
	return 0;
}

int jinsubscription_new(eXosipua_t *jua, eXosip_event_t *je)
{
	osip_message_t *request;
	
	eXosip_insubscription_build_notify(je->did, EXOSIP_SUBCRSTATE_PENDING, EXOSIP_NOTIFY_AWAY, &request);
	eXosip_insubscription_send_request(je->did, request);
			
	return 0;
}
