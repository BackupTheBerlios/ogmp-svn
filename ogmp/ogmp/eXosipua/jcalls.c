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
#include "../rtp_cap.h"

//jcall_t jcalls[MAX_NUMBER_OF_CALLS];
//char   _localip[30];

//static int ___call_init = 0;

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
	sipua_call_event_t call_e;

	memset(&call_e, 0, sizeof(sipua_call_event_t));

	if(jua->ncall == MAX_SIPUA_LINES-1)
	{
		/* All lines are busy */
		eXosip_answer_call(je->did, 486, 0);
		return 0;
	}

	call_e.event.type = SIPUA_EVENT_NEW_CALL;
	call_e.event.content = je->sdp_body;

	call_e.cid = je->cid;
	call_e.did = je->did;
	call_e.subject = je->subject;
	call_e.textinfo = je->textinfo;
	call_e.req_uri = je->req_uri;
	call_e.local_uri = je->local_uri;
	call_e.remote_uri = je->remote_uri;

	if (je->reason_phrase[0]!='\0')
    {
		call_e.reason_phrase = je->reason_phrase;
		call_e.status_code = je->status_code;
    }
/*
#if defined(XRTP_SUPPORT)
	{
		sdp_message_t *sdp;
		sdp_message_init(&sdp);

		if (sdp_message_parse(sdp, je->sdp_body) == 0)
		{
			if(ca->callin_info)
			{
				rtp_capable_done_set(ca->callin_info);
				ca->callin_info = NULL;
			}

			ca->callin_info = rtp_capable_from_sdp(sdp);
		}
	}
#endif
*/
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
	jcall_t *ca;
	int k;

	/* event back to sipuac */
	sipua_event_t sip_e;
	sip_e.call_info = (sipua_set_t*)je->external_reference;
	sip_e.type = SIPUA_EVENT_PROCEEDING;

	for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
		if (jua->jcalls[k].state != NOT_USED
			&& jua->jcalls[k].cid==je->cid
			&& jua->jcalls[k].did==je->did)
		break;
    }
	if (k==MAX_NUMBER_OF_CALLS)
    {
		for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
		{
			if (jua->jcalls[k].state == NOT_USED)
				break;
		}
		if (k==MAX_NUMBER_OF_CALLS)
			return -1;

		ca = &(jua->jcalls[k]);
		memset(&(jua->jcalls[k]), 0, sizeof(jcall_t));
      
		ca->cid = je->cid;
		ca->did = je->did;
      
		if (ca->did<1 && ca->cid<1)
		{
			exit(0);
			return -1; /* not enough information for this event?? */
		}
    }

	sip_e.lineno = k;

	ca = &(jua->jcalls[k]);

	osip_strncpy(ca->textinfo,   je->textinfo, 255);
	osip_strncpy(ca->req_uri,    je->req_uri, 255);
	//osip_strncpy(ca->local_uri,  je->local_uri, 255);
	osip_strncpy(ca->remote_uri, je->remote_uri, 255);
	osip_strncpy(ca->subject,    je->subject, 255);

#if 0
	if (ca->remote_sdp_audio_ip[0]=='\0')
    {
		osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
		ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
		ca->payload = je->payload;
		osip_strncpy(ca->payload_name, je->payload_name, 49);
    }
#endif

	if (je->reason_phrase[0]!='\0')
    {
		osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
		ca->status_code = je->status_code;
    }

	ca->state = je->type;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);

	return 0;
}

int jcall_ringing(eXosipua_t *jua, eXosip_event_t *je)
{
	jcall_t *ca;
	int k;

	/* event back to sipuac */
	sipua_event_t sip_e;
	sip_e.call_info = (sipua_set_t*)je->external_reference;
	sip_e.type = SIPUA_EVENT_RINGING;

	for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
		if (jua->jcalls[k].state != NOT_USED
				&& jua->jcalls[k].cid==je->cid
				&& jua->jcalls[k].did==je->did)
			break;
    }

	if (k==MAX_NUMBER_OF_CALLS)
    {
		for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
		{
			if (jua->jcalls[k].state == NOT_USED)
			break;
		}
		if (k==MAX_NUMBER_OF_CALLS)
			return -1;
		ca = &(jua->jcalls[k]);
		memset(&(jua->jcalls[k]), 0, sizeof(jcall_t));
      
		ca->cid = je->cid;
		ca->did = je->did;
      
		if (ca->did<1 && ca->cid<1)
		{
			exit(0);
			return -1; /* not enough information for this event?? */
		}
    }

	sip_e.lineno = k;

	ca = &(jua->jcalls[k]);
	osip_strncpy(ca->textinfo,   je->textinfo, 255);
	osip_strncpy(ca->req_uri,    je->req_uri, 255);
	//osip_strncpy(ca->local_uri,  je->local_uri, 255);
	osip_strncpy(ca->remote_uri, je->remote_uri, 255);
	osip_strncpy(ca->subject,    je->subject, 255);
/*
	if (ca->remote_sdp_audio_ip[0]=='\0')
    {
		osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
		ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
		ca->payload = je->payload;
		osip_strncpy(ca->payload_name, je->payload_name, 49);
    }
*/
	if (je->reason_phrase[0]!='\0')
    {
		osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
		ca->status_code = je->status_code;
    }

	ca->state = je->type;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);

	return 0;
}

int jcall_answered(eXosipua_t *jua, eXosip_event_t *je)
{
	sipua_call_event_t call_e;

	/* event back to sipuac */
	memset(&call_e, 0, sizeof(sipua_call_event_t));

	call_e.event.call_info = (sipua_set_t*)je->external_reference;

	call_e.event.type = SIPUA_EVENT_ANSWERED;
	call_e.event.content = je->sdp_body;

	if (je->cid != call_e.cid && je->did != call_e.did)
		return -1; 
	
	if (je->reason_phrase[0]!='\0')
    {
		call_e.reason_phrase = je->reason_phrase;
		call_e.status_code = je->status_code;
    }
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
	sipua_set_t* call;

	/* event back to sipuac */
	memset(&e, 0, sizeof(sipua_event_t));

	call = e.call_info = (sipua_set_t*)je->external_reference;

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
	sip_e.call_info = (sipua_set_t*)je->external_reference;
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

int jcall_requestfailure(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

	/* event back to sipuac */
	sipua_event_t sip_e;
	sip_e.call_info = (sipua_set_t*)je->external_reference;
	sip_e.type = SIPUA_EVENT_REQUESTFAILURE;

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

#ifdef MEDIASTREAMER_SUPPORT
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

  ca->state = NOT_USED;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);

  return 0;
}

int jcall_serverfailure(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

	/* event back to sipuac */
	sipua_event_t sip_e;
	sip_e.call_info = (sipua_set_t*)je->external_reference;
	sip_e.type = SIPUA_EVENT_SERVERFAILURE;

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


#ifdef MEDIASTREAMER_SUPPORT
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

  ca->state = NOT_USED;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);

  return 0;
}

int jcall_globalfailure(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

	/* event back to sipuac */
	sipua_event_t sip_e;
	sip_e.call_info = (sipua_set_t*)je->external_reference;
	sip_e.type = SIPUA_EVENT_GLOBALFAILURE;

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

#ifdef MEDIASTREAMER_SUPPORT
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

  ca->state = NOT_USED;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);

  return 0;
}

int jcall_closed(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

	/* event back to sipuac */
	sipua_event_t sip_e;
	sip_e.call_info = (sipua_set_t*)je->external_reference;
	sip_e.type = SIPUA_EVENT_CALL_CLOSED;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    return -1;

	sip_e.lineno = k;

  ca = &(jua->jcalls[k]);

#ifdef MEDIASTREAMER_SUPPORT

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

	ca->state = NOT_USED;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);

	return 0;
}

int jcall_onhold(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

	/* event back to sipuac */
	sipua_event_t sip_e;
	sip_e.call_info = (sipua_set_t*)je->external_reference;
	sip_e.type = SIPUA_EVENT_ONHOLD;

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
  osip_strncpy(ca->textinfo,   je->textinfo, 255);
/*
  if (ca->remote_sdp_audio_ip[0]=='\0')
    {
      osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
      ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
    }
*/
  if (je->reason_phrase[0]!='\0')
    {
      osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
      ca->status_code = je->status_code;
    }
  ca->state = je->type;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);

  return 0;
}

int jcall_offhold(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

	/* event back to sipuac */
	sipua_event_t sip_e;
	sip_e.call_info = (sipua_set_t*)je->external_reference;
	sip_e.type = SIPUA_EVENT_OFFHOLD;

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
  osip_strncpy(ca->textinfo,   je->textinfo, 255);
/*
  if (ca->remote_sdp_audio_ip[0]=='\0')
    {
      osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
      ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
    }
*/
  if (je->reason_phrase[0]!='\0')
    {
      osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
      ca->status_code = je->status_code;
    }
  ca->state = je->type;

	/* event notification */
	jua->sipuas.notify_event(jua->sipuas.lisener, &sip_e);

  return 0;
}

