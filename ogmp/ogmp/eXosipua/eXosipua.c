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

/* from Josua/src */
#include "eXosip2.h"

#include <timedia/xstring.h>
#include <timedia/xmalloc.h>
#include <timedia/ui.h>

#define JUA_LOG
#define JUA_DEBUG

#ifdef JUA_LOG
 #define jua_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define jua_log(fmtargs)
#endif

#ifdef JUA_DEBUG
 #define jua_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define jua_debug(fmtargs)
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

/* Get SIP message of the call leg */
osip_message_t *eXosipua_extract_message(eXosipua_t *jua, eXosip_event_t *je)
{
   osip_message_t *message;

   if(!je->sdp_body || !je->sdp_body[0] || osip_message_init(&message) != 0)
      return NULL;  
   
   jua_debug(("eXosipua_extract_message: sdp body\n"));
   jua_debug(("%s", je->sdp_body));

   if(osip_message_parse(message, je->sdp_body, strlen(je->sdp_body)) != 0)
   {
      osip_message_free(message);
      return NULL;
   }
   
   return message;
}

osip_message_t *eXosipua_receive_message(eXosipua_t *jua, eXosip_event_t *je)
{
	eXosip_call_t *call = NULL;

	eXosip_call_find(je->cid, &call);
	if(!call)
   {
      jua_log(("jua_call_message: no call\n"));
      return NULL;
   }
   
	return call->c_inc_tr->last_response;
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

      jua_log(("jua_process_event: je->status_code[%d]\n", je->status_code));

		if (je->type==EXOSIP_CALL_NEW)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_NEW\n"));
			jcall_new(jua, je);
		}
		else if (je->type==EXOSIP_CALL_ANSWERED)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_ANSWERED\n"));
			jcall_answered(jua, je);
		}
		else if (je->type==EXOSIP_CALL_ACK)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_ACK\n"));
         jcall_ack(jua, je);
		}
		else if (je->type==EXOSIP_CALL_PROCEEDING)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_PROCEEDING\n"));
         jcall_proceeding(jua, je);
		}
		else if (je->type==EXOSIP_CALL_RINGING)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_RINGING\n"));
         jcall_ringing(jua, je);
		}
		else if (je->type==EXOSIP_CALL_REDIRECTED)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_REDIRECTED\n"));
			jcall_redirected(jua, je);
		}
		else if (je->type==EXOSIP_CALL_REQUESTFAILURE)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_REQUESTFAILURE\n"));
			jcall_requestfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_SERVERFAILURE)
		{                               
         jua_log(("jua_process_event: EXOSIP_CALL_SERVERFAILURE\n"));
			jcall_serverfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_GLOBALFAILURE)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_GLOBALFAILURE\n"));
			jcall_globalfailure(jua, je);
		}
		else if (je->type==EXOSIP_CALL_CLOSED)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_CLOSED\n"));
			jcall_closed(jua, je);
		}
		else if (je->type==EXOSIP_CALL_HOLD)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_HOLD\n"));
			jcall_onhold(jua, je);
		}
		else if (je->type==EXOSIP_CALL_OFFHOLD)
		{
         jua_log(("jua_process_event: EXOSIP_CALL_OFFHOLD\n"));
			jcall_offhold(jua, je);
		}
		else if (je->type==EXOSIP_REGISTRATION_SUCCESS)
		{
			sipua_reg_event_t reg_e;
         
			eXosip_reg_t *jr;
         osip_message_t *message = NULL;

			reg_e.event.call_info = NULL;

         reg_e.event.type = SIPUA_EVENT_REGISTRATION_SUCCEEDED;
			reg_e.status_code = je->status_code;	/* eg: 200*/
			reg_e.server_info = je->reason_phrase;  /* eg: OK */
			reg_e.server = je->req_uri;				/* eg: sip:registrar.domain */
			/*
			je->remote_uri // eg: regname@domain 
			*/

			/*
          * Retrieve exactly returned expiration seconds
			 */
         message = eXosipua_extract_message(jua, je);
         if(message)
         {
            osip_contact_t  *contact = NULL;
            osip_message_get_contact(message, 0, &contact);

            if(contact)
            {
               osip_uri_param_t *expires = NULL;
               
               osip_contact_param_get_byname(contact, "expires", &expires);

               if(expires)
                  reg_e.seconds_expires = strtol(expires->gvalue, NULL, 10);
            }

            osip_message_free(message);
         }                                                                
         else
         {
			   jr = eXosip_event_get_reginfo(je);
			   reg_e.seconds_expires = jr->r_reg_period;
         }

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
			/*
			snprintf(buf, 99, "<- (%i %i) INFO from: %s",
					je->cid, je->did, je->remote_uri);
	  
			josua_printf(buf);
			*/
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

	jua_log(("uas_address: nettype[%s]\n", uas->nettype));
	jua_log(("uas_address: addrtype[%s]\n", uas->addrtype));
	jua_log(("uas_address: netaddr[%s]\n", uas->netaddr));

   *nettype = xstr_clone(uas->nettype);
	*addrtype = xstr_clone(uas->addrtype);
	*netaddr = xstr_clone(uas->netaddr);

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

sipua_auth_t *uas_new_auth(char* username, char *realm)
{
   sipua_auth_t *auth = xmalloc(sizeof(sipua_auth_t));
   if(auth)
   {              
      auth->username = xstr_clone(username);
      auth->realm = xstr_clone(realm);
   }

   return auth;
}

int uas_done_auth(void* gen)
{
   sipua_auth_t *auth = (sipua_auth_t*)gen;
   
   xfree(auth->username);
   xfree(auth->realm);
   xfree(auth);

   return UA_OK;
}

int uas_match_auth(void *pat, void *tar)
{
   sipua_auth_t *p_auth = (sipua_auth_t *)pat;
   sipua_auth_t *t_auth = (sipua_auth_t *)tar;

   if(0==strcmp(p_auth->username, t_auth->username) && 0==strcmp(p_auth->realm, t_auth->realm))
      return 0;

   return -1;
}

int uas_set_authentication_info(sipua_uas_t *sipuas, char *username, char *userid, char*passwd, char *realm)
{
   xlist_user_t lu;
   
   sipua_auth_t *new_auth;
   sipua_auth_t auth = {username, realm};

   if(xlist_find(sipuas->auth_list, &auth, uas_match_auth, &lu))
      return UA_OK;

   new_auth = uas_new_auth(username, realm);
   if(new_auth)
   {
      xlist_addto_first(sipuas->auth_list, new_auth);
      
      if(eXosip_add_authentication_info(username, userid, passwd, NULL, realm) == 0)
         return UA_OK;
   }
   
   return UA_FAIL;
}

int uas_clear_authentication_info(sipua_uas_t *sipuas, char *username, char *realm)
{
   return UA_EIMPL;
}

int uas_has_authentication_info(sipua_uas_t *sipuas, char *username, char *realm)
{
   xlist_user_t lu;
   sipua_auth_t auth = {username, realm};

   if(xlist_find(sipuas->auth_list, &auth, uas_match_auth, &lu))
      return 1;

   return 0;
}

int uas_regist(sipua_uas_t *sipuas, int *regno, char *loc, char *registrar, char *id, int seconds)
{
	int ret;

	eXosipua_t *jua = (eXosipua_t*)sipuas;

	char* siploc, *p;

	p = siploc = xmalloc(4+strlen(loc)+1+10+1);
	if(!siploc)
		return UA_FAIL;

	strcpy(p, "sip:");
	p += 4;
	strcpy(p, loc);
   while(*p)
      p++;

   snprintf(p, 12, ":%d", sipuas->portno);

	jua_log(("uas_regist: %s on %s within %ds\n", id, registrar, seconds));

	eXosip_lock();

   if(*regno < 0)
   {
      *regno = eXosip_register_init(id, registrar, siploc);
	   if (*regno < 0)
	   {
		   eXosip_unlock();
		   return UA_FAIL;
	   }
   }

	ret = eXosip_register(*regno, seconds);

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

/* Set loose route if not set in route url */
char* uas_check_route(const char *url)
{
	int err;
	osip_uri_param_t *lr_param=NULL;
	osip_route_t *rt=NULL;
	char *route=NULL;
   
	if (url!=NULL && strlen(url)>0)
   {
		osip_route_init(&rt);
		err=osip_route_parse(rt,url);
		if (err<0)
      {
			osip_route_free(rt);



			return NULL;
		}

      /* check if the lr parameter is set , if not add it */
		osip_uri_uparam_get_byname(rt->url, "lr", &lr_param);
	  	if (lr_param==NULL)
      {
			osip_uri_uparam_add(rt->url,osip_strdup("lr"),NULL);
			osip_route_to_str(rt,&route);
         
         return route;
		}

      return xstr_clone(url);
	}

   return NULL;
}

int uas_retry_call(sipua_uas_t *sipuas, sipua_set_t* call)
{
	eXosip_lock();
	eXosip_retry_call(call->cid);
	eXosip_unlock();

   return UA_OK;
}
               
int uas_invite(sipua_uas_t *sipuas, const char *to, sipua_set_t* call_info, char* sdp_body, int sdp_bytes)
{
	eXosipua_t *jua = (eXosipua_t*)sipuas;
    
	osip_message_t *invite;
	char sdp_size[8];
	char* proxy = NULL;

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

	if(jua->sipuas.proxy[0]!='\0')
   {
		proxy = uas_check_route(jua->sipuas.proxy);
   }
#if 0
	else
   {
      proxy = uas_check_route(call_info->user_prof->registrar);
   }
#endif

	sprintf(sdp_size,"%i", sdp_bytes);

   printf("uas_invite: [%s] from %s to %s, proxy[%s]\n", call_info->subject, from, to, proxy);
	printf("\n-------Initiate SDP [%d bytes]--------\n", sdp_bytes);
	printf("Callid[%s]\n", call_info->setid.id);
	printf("----------------------------------------\n");
	printf("%s", sdp_body);
	printf("----------------------------------------\n");

	if (eXosip_build_initial_invite(&invite, to, from, proxy, call_info->subject) != 0)
		return UA_FAIL;

	/* sdp content of the call */
	osip_message_set_content_type(invite, "application/sdp");
   osip_message_set_content_length(invite, sdp_size);
	osip_message_set_body(invite, sdp_body, sdp_bytes);
   
	eXosip_lock();
   
	ret = eXosip_initiate_call(invite, call_info, NULL/*negotiation_reference*/, NULL/*local_audio_port*/);

	eXosip_unlock();
	
   if(proxy)
      osip_free(proxy);
   
	/*When to free it ???
	osip_message_free(invite);
	*/

	return ret;
}

int uas_accept(sipua_uas_t* uas, sipua_set_t *call)
{
	eXosipua_t *jua = (eXosipua_t*)uas;

   jua->ncall++;
   
   if(eXosip_set_call_reference(call->did, call) == 0)
      jua_log(("uas_accept: accepted\n"));
	
	return UA_OK;
}

int uas_release(sipua_uas_t* uas)
{
	eXosipua_t *jua = (eXosipua_t*)uas;

   if(jua->ncall > 0)
      jua->ncall--;

	return UA_OK;
}

#if 0
int eXosip_reinvite_with_authentication (struct eXosip_call_t *jc)
{
    struct eXosip_call_t *jcc;
#ifdef SM

    char *locip;
#else
    char locip[50];
#endif
	osip_message_t * cloneinvite;
  	osip_event_t *sipevent;
  	osip_transaction_t *transaction;
	int osip_cseq_num,length;
    osip_via_t *via;
    char *tmp;
    int i;

	osip_message_clone (jc->c_out_tr->orig_request, &cloneinvite);
	osip_cseq_num = osip_atoi(jc->c_out_tr->orig_request->cseq->number);
	length   = strlen(jc->c_out_tr->orig_request->cseq->number);
	tmp    = (char *)osip_malloc(90*sizeof(char));
	via   = (osip_via_t *) osip_list_get (cloneinvite->vias, 0);

	osip_list_remove(cloneinvite->vias, 0);
	osip_via_free(via);

#ifdef SM
	eXosip_get_localip_for(cloneinvite->req_uri->host,&locip);
#else
	eXosip_guess_ip_for_via(eXosip.ip_family, locip, 49);
#endif

	if (eXosip.ip_family==AF_INET6) {
		sprintf(tmp, "SIP/2.0/UDP [%s]:%s;branch=z9hG4bK%u", locip,
		      eXosip.localport, via_branch_new_random());
	} else {
		sprintf(tmp, "SIP/2.0/UDP %s:%s;branch=z9hG4bK%u", locip,
		      eXosip.localport, via_branch_new_random());
	}
#ifdef SM
	osip_free(locip);
#endif
	osip_via_init(&via);
	osip_via_parse(via, tmp);
	osip_list_add(cloneinvite->vias, via, 0);
	osip_free(tmp);

	osip_cseq_num++;
	osip_free(cloneinvite->cseq->number);
	cloneinvite->cseq->number = (char*)osip_malloc(length + 2);
	sprintf(cloneinvite->cseq->number, "%i", osip_cseq_num);

	eXosip_add_authentication_information(cloneinvite,
jc->c_out_tr->last_response);
    cloneinvite->message_property = 0;

    eXosip_call_init(&jcc);
    i = osip_transaction_init(&transaction,
		       ICT,
		       eXosip.j_osip,
		       cloneinvite);
    if (i!=0)
    {
      eXosip_call_free(jc);
      osip_message_free(cloneinvite);
      return -1;
    }
    jcc->c_out_tr = transaction;
    sipevent = osip_new_outgoing_sipmessage(cloneinvite);
    sipevent->transactionid =  transaction->transactionid;
    osip_transaction_set_your_instance(transaction, __eXosip_new_jinfo(jcc,
NULL, NULL, NULL));
    osip_transaction_add_event(transaction, sipevent);

    jcc->external_reference = 0;
    ADD_ELEMENT(eXosip.j_calls, jcc);

    eXosip_update(); /* fixed? */
    __eXosip_wakeup();
	return 0;
}
#endif

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

int uas_init(sipua_uas_t* uas, int sip_port, const char* nettype, const char* addrtype, const char* firewall, const char* proxy)
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
   {
		strcpy(uas->firewall, firewall);
      eXosip_set_firewallip(firewall);
		jua_log(("sipua_uas: firewall[%s]\n", firewall));
   }
   
	if(proxy)
   {
		strcpy(uas->proxy, proxy);
		jua_log(("sipua_uas: proxy[%s]\n", proxy));
   }
   else
   {
		jua_log(("sipua_uas: proxy depend on user profile\n"));
   }

	uas->portno = sip_port;
	
	eXosip_set_user_agent("Ogmp/eXosip");
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
   uas->auth_list = xlist_new();
   if(!uas->auth_list)
   {
      xfree(jua);
      jua_debug(("sipua_new_server: No memory for Authorization list\n"));
      return NULL;
   }

	uas->match_type = uas_match_type;

   uas->init = uas_init;
	uas->done = uas_done;
    
	uas->start = uas_start;

   uas->shutdown = uas_shutdown;

	uas->address = uas_address;

	uas->add_coding = uas_add_coding;
	uas->clear_coding = uas_clear_coding;

	uas->set_listener = uas_set_lisener;

	uas->set_authentication_info = uas_set_authentication_info;
	uas->clear_authentication_info = uas_clear_authentication_info;
	uas->has_authentication_info = uas_has_authentication_info;
   
	uas->regist = uas_regist;
	uas->unregist = uas_unregist;

	uas->accept = uas_accept;
	uas->release = uas_release;

	uas->invite = uas_invite;
	uas->answer = uas_answer;
	uas->bye = uas_bye;

   uas->retry = uas_retry_call;

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
