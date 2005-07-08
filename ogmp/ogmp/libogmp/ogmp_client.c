/***************************************************************************
                          ogmp_client.c - ogmp client starter
                             -------------------
    begin                : Tue Jul 20 2004
    copyright            : (C) 2004 by Heming
    email                : heming@bigfoot.com; heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include "rtp_format.h"

#include "ogmp.h"

#include <timedia/xstring.h>
#include <timedia/ui.h>
#include <stdarg.h>

extern ogmp_ui_t* global_ui;

#define CLIE_LOG
#define CLIE_DEBUG

#ifdef CLIE_LOG
 #define clie_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define clie_log(fmtargs)
#endif

#ifdef CLIE_DEBUG
 #define clie_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define clie_debug(fmtargs)
#endif

#define SIPUA_MAX_RING 3


#define MAX_CALL_BANDWIDTH  20480  /* in Bytes */

/****************************************************************************************/
int client_queue(sipua_t *sipua, sipua_call_t* call);
int client_answer(sipua_t *sipua, sipua_call_t* call, int reply, media_source_t* source);

int client_call_ringing(void* gen)
{
	int i;
	sipua_call_t* call;
	rtime_t r;
	ogmp_client_t *client = (ogmp_client_t*)gen;

	xclock_t *clock = time_start();

	int ringing = 1;

	while(ringing)
	{
		xthr_lock(client->nring_lock);

		if(client->nring == 0)
			break;

		client->ogui->ui.beep(&client->ogui->ui);

		ringing = 0;

		for(i=1; i<MAX_SIPUA_LINES; i++)
		{
			call = client->lines[i];
            
			if(call && call->status == SIPUA_STATUS_INCOMING)
			{
				ringing = 1;

				if(call->ring_num == 0)
				{
					/* On hold or redirect this call */
					client->nring--;

					if(client->background_source)
               {
			         clie_debug(("\nclient_call_ringing: on hold\n"));
					   call->status = SIPUA_STATUS_ONHOLD;
						client_queue(&client->sipua, call);
               }
               else
                  client_answer(&client->sipua, call, SIP_STATUS_CODE_OK, NULL);

					if(client->nring == 0)
						break;
               
					continue;
				}
				
				sipua_answer(&client->sipua, call, SIP_STATUS_CODE_RINGING, NULL);

				call->ring_num--;
			}
		}

		if(client->nring == 0)
			break;

		xthr_unlock(client->nring_lock);

		time_msec_sleep(clock, 2000, &r);
	}

	xthr_unlock(client->nring_lock);

	client->thread_ringing = NULL;

	time_end(clock);

	return UA_OK;
}

int client_done_format_handler(void *gen)
{
   media_format_t * fmt = (media_format_t *)gen;

   fmt->done(fmt);

   return MP_OK;
}

/* forward definition */
int client_regist(sipua_t *sipua, user_profile_t *user, char * userloc);

/* FIXME: resource greedy, need better solution */
int client_register_loop(void *gen)
{
	int intv = 2;  /* second */
	rtime_t ms_remain;
	
	user_profile_t* user_prof = (user_profile_t*)gen;
	ogmp_client_t* client = (ogmp_client_t*)user_prof->sipua;

	xclock_t* clock = time_start();

	user_prof->enable = 1;
	while(user_prof->enable)
	{
		user_prof->seconds_left -= intv;

		if(user_prof->seconds_left <= 0)
      {
         user_prof->regno = -1;
         client_regist(&client->sipua, user_prof, user_prof->cname);
      }

		time_msec_sleep(clock, intv*1000, &ms_remain);
	}

	time_end(clock);

	user_prof->thread_register = NULL;

	return UA_OK;
}

int client_place_call(sipua_t *sipua, sipua_call_t *call)
{
   int i;
   
   ogmp_client_t *client = (ogmp_client_t*)sipua;

   /* line #0 reserved for current call */
	for(i=1; i<MAX_SIPUA_LINES; i++)
	{
	   if(client->lines[i] == NULL)
	   {
	      call->ring_num = SIPUA_MAX_RING;

	      client->lines[i] = call;
         call->lineno = i;

	      if(!client->thread_ringing)
	      {
				client->nring++;
				client->thread_ringing = xthr_new(client_call_ringing, client, XTHREAD_NONEFLAGS);
	      }
	      else
	      {
				xthr_lock(client->nring_lock);
				client->nring++;
				xthr_unlock(client->nring_lock);
	      }

	      sipua->uas->accept(sipua->uas, call);
	      return i;
	   }
	}
         
   return -1;
}

int client_release_call(sipua_t *sipua, sipua_call_t *call)
{
   ogmp_client_t *client = (ogmp_client_t*)sipua;
   
   if(!call)
      return UA_IGNORE;

   if(call->lineno >= 0 && call->lineno < MAX_SIPUA_LINES)
      client->lines[call->lineno] = NULL;
   
	sipua->uas->release(sipua->uas);
   sipua->done_call(sipua, call);

   return UA_OK;
}

int client_sipua_event(void* lisener, sipua_event_t* e)
{
	sipua_t *sipua = (sipua_t*)lisener;

	ogmp_client_t *client = (ogmp_client_t*)lisener;

   int isreg = 0;

   switch(e->type)
	{
		case(SIPUA_EVENT_REGISTRATION_SUCCEEDED):
         isreg = 1;
		case(SIPUA_EVENT_UNREGISTRATION_SUCCEEDED):
		{
			sipua_reg_event_t *reg_e = (sipua_reg_event_t*)e;

			user_profile_t* user_prof = client->reg_profile;

         if(user_prof == NULL)
            break;
                
         if(user_prof->reg_reason_phrase)
         {
                xfree(user_prof->reg_reason_phrase);
                user_prof->reg_reason_phrase = NULL;
         }

         if(reg_e->server_info)
            user_prof->reg_reason_phrase = xstr_clone(reg_e->server_info);

         if(user_prof->reg_status == SIPUA_STATUS_REG_DOING)
			{
				user_prof->reg_server = xstr_clone(reg_e->server);

				client->sipua.user_profile = client->reg_profile;

				user_prof->reg_status = SIPUA_STATUS_REG_OK;
				user_prof->online_status = SIPUA_STATUS_ENABLE;
			}

			if(user_prof->reg_status == SIPUA_STATUS_UNREG_DOING)
         {
            xstr_done_string(user_prof->reg_server);
            user_prof->reg_server = NULL;

			   user_prof->reg_status = SIPUA_STATUS_NORMAL;
				user_prof->online_status = SIPUA_STATUS_DISABLE;
         }

			/* registering transaction completed */
         client->reg_profile = NULL;
         user_prof->regno = -1;

			/* Should be exact expire seconds returned by registrary*/
			clie_log(("client_sipua_event: expired in %d seconds\n", reg_e->seconds_expires));

         user_prof->seconds_left = reg_e->seconds_expires;
			if(reg_e->seconds_expires == 0)
			{
				/* This is not expected */
				user_prof->reg_status = SIPUA_STATUS_NORMAL;
				break;
			}

			user_prof->sipua = client;
			user_prof->enable = 1;

			/* Create a register thread */
         xthr_lock(user_prof->thread_register_lock);
         
			if(!user_prof->thread_register)
				user_prof->thread_register = xthr_new(client_register_loop, user_prof, XTHREAD_NONEFLAGS);

         xthr_unlock(user_prof->thread_register_lock);
         
         if(client->on_register)
            client->on_register(client->user_on_register, reg_e->status_code, user_prof->regname, isreg);
                
         //client->ogui->ui.beep(&client->ogui->ui);

			break;
		}
		case(SIPUA_EVENT_REGISTRATION_FAILURE):
         isreg = 1;
		case(SIPUA_EVENT_UNREGISTRATION_FAILURE):
		{
			char buf[100];

			sipua_reg_event_t *reg_e = (sipua_reg_event_t*)e;
			user_profile_t* user_prof = client->reg_profile;

         if(!user_prof)
            break;
                
         snprintf(buf, 99, "Register %s Error[%i]: %s", reg_e->server, reg_e->status_code, reg_e->server_info);
			clie_log(("client_sipua_event: %s\n", buf));
         
         if(reg_e->status_code == SIP_STATUS_CODE_PROXYAUTHENTICATIONREQUIRED)
         {
            char *userid, *passwd;

            if(0 == sipua->uas->has_authentication_info(sipua->uas, user_prof->username, sipua->proxy_realm))
            {
               user_prof->auth = 0;
               
               if(client->on_authenticate)
                  client->on_authenticate(client->user_on_authenticate, sipua->proxy_realm, user_prof, &userid, &passwd);
            }
            
            if(user_prof->auth)
            {
			      client_regist(sipua, user_prof, user_prof->cname);
               break;
            }
         }

         if(reg_e->status_code == SIP_STATUS_CODE_UNAUTHORIZED)
         {
            char *userid = NULL, *passwd = NULL;

            if(!sipua->uas->has_authentication_info(sipua->uas, user_prof->username, user_prof->realm))
            {    
               user_prof->auth = 0;

               if(client->on_authenticate)
                  client->on_authenticate(client->user_on_authenticate, user_prof->realm, user_prof, &userid, &passwd);
            }
            else
            {
               sipua->uas->clear_authentication_info(sipua->uas, user_prof->username, user_prof->realm);
               
               if(client->on_register)
                  client->on_register(client->user_on_register, reg_e->status_code, user_prof->regname, isreg);

               break;
            }

            if(user_prof->auth)
            {
			      client_regist(sipua, user_prof, user_prof->cname);
               break;
            }
         }

         if(user_prof->reg_status == SIPUA_STATUS_REG_DOING)
            user_prof->reg_status = SIPUA_STATUS_NORMAL;
                
			if(user_prof->reg_status == SIPUA_STATUS_UNREG_DOING)
            user_prof->reg_status = SIPUA_STATUS_REG_OK;

			if(user_prof->reg_reason_phrase)
         { 
				xfree(user_prof->reg_reason_phrase);
            user_prof->reg_reason_phrase = NULL;
         }
                
         if(reg_e->server_info)
            user_prof->reg_reason_phrase = xstr_clone(reg_e->server_info);

			/* registering transaction completed */
			client->reg_profile = NULL;
         user_prof->regno = -1;

         if(client->on_register)
            client->on_register(client->user_on_register, reg_e->status_code, user_prof->regname, isreg);

			break;  
		}
		case(SIPUA_EVENT_NEW_CALL):
		{
			/* Callee receive a new call, now in negotiation stage */
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;
			sipua_call_t *call;

         int lineno;

			rtpcap_set_t* rtpcapset = NULL;

			sdp_message_t *sdp_message = NULL;

         int ret;

			char* sdp_body = (char*)e->content;

			sdp_message_init(&sdp_message);

			if (sdp_message_parse(sdp_message, sdp_body) != 0)
			{
				clie_log(("\nclient_sipua_event: fail to parse sdp\n"));
				sdp_message_free(sdp_message);

            return UA_FAIL;
			}

         if(sdp_message)
            rtpcapset = rtp_capable_from_sdp(sdp_message);
         
         /* Retrieve call info */
         call = sipua_new_call(sipua, client->sipua.user_profile, call_e->remote_uri,
                        call_e->subject, call_e->sbytes, rtpcapset->info, rtpcapset->ibytes);
         if(!call)
         {
            rtp_capable_done_set(rtpcapset);
            return UA_FAIL;
         }

         call->cid = call_e->cid;
			call->did = call_e->did;
         
			call->status = SIPUA_STATUS_INCOMING;

			call->rtpcapset = rtpcapset;

         /* put in line */
         lineno = client_place_call(sipua, call);
            
         if(client->on_newcall)
            client->on_newcall(client->user_on_newcall, lineno, call);

         /* Check status after UI respond */
         if(call->status == SIPUA_STATUS_DECLINE)
         {
            /* Call is declined */
				sipua_answer(&client->sipua, call, SIP_STATUS_CODE_TEMPORARILYUNAVAILABLE, NULL);

            client_release_call(sipua, call);
            
            return UA_OK;
         }
         
         /* Get call profile */
         rtpcapset->user_profile = call->user_prof;
         
			/* now to negotiate call, which will generate call structure with sdp message for reply
			 * still no rtp session created !
			 */
			ret = sipua_negotiate_call(sipua, call, rtpcapset,
							client->mediatypes, client->default_rtp_ports, 
							client->default_rtcp_ports, client->nmedia,
							client->control, MAX_CALL_BANDWIDTH);

			if(ret < UA_OK)
			{
				/* Fail to receive the call, miss it */
				sipua_answer(&client->sipua, call, SIP_STATUS_CODE_TEMPORARILYUNAVAILABLE, NULL);

            client_release_call(sipua, call);

            return UA_FAIL;
			}

         /**
          * Cannot free here, need for media specific parsing !!!
          * sdp_message_free(sdp_message);
          */
          
			break;
		}
		case(SIPUA_EVENT_CALL_CLOSED):
		{
			sipua_call_t *call = e->call_info;
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;

			clie_log(("client_sipua_event: call@%x [%s] closed\n", (int)call, call->subject));
         
         if(client->on_terminate)
            client->on_terminate(client->user_on_terminate, call, call_e->status_code);

         client_release_call(sipua, call);
         
         break;
      }
		case(SIPUA_EVENT_PROCEEDING):
		{
			sipua_call_t *call = e->call_info;
         sipua_call_event_t *call_e = (sipua_call_event_t*)e;         
         clie_debug(("client_sipua_event: status_cade[%d]\n", call_e->status_code));

			call->status = SIPUA_EVENT_PROCEEDING;

			break;
		}
		case(SIPUA_EVENT_RINGING):
		{	
			sipua_call_t *call = e->call_info;
         sipua_call_event_t *call_e = (sipua_call_event_t*)e;
         
         clie_debug(("client_sipua_event: status_cade[%d]\n", call_e->status_code));
			call->status = SIPUA_EVENT_RINGING;

			break;
		}
		case(SIPUA_EVENT_ANSWERED):
		{
			/* Caller establishs call when callee is answered */

			sipua_call_t *call;
			rtpcap_set_t* rtpcapset;
			sdp_message_t *sdp_message;
			char* sdp_body = (char*)e->content;
			int bw;
         
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;
			clie_debug(("client_sipua_event: status_cade[%d]\n", call_e->status_code));

			call = e->call_info; 
			call->status = SIP_STATUS_CODE_OK;

		   printf("client_sipua_event: call[%s] answered\n", call->subject);
		   printf("client_sipua_event: SDP\n");
		   printf("------------------------\n");
		   printf("%s", sdp_body);
		   printf("------------------------\n");

         sdp_message_init(&sdp_message);

			if (sdp_message_parse(sdp_message, sdp_body) != 0)
			{
				clie_log(("\nclient_sipua_event: fail to parse sdp\n"));

				sdp_message_free(sdp_message);
				
				break;
			}

			/* capable answered by callee */
			rtpcapset = rtp_capable_from_sdp(sdp_message);
			rtpcapset->user_profile = call->user_prof;
			rtpcapset->rtpcap_selection = RTPCAP_ALL_CAPABLES;

			/* rtp sessions created */
			bw = sipua_establish_call(sipua, call, "playback", rtpcapset,
                                   client->format_handlers, client->control, client->pt);
			if(bw < 0)
			{
				sipua_answer(&client->sipua, call, SIP_STATUS_CODE_DECLINE, NULL);
            break;
			}

			rtp_capable_done_set(rtpcapset);

         break;
		}
		case(SIPUA_EVENT_ACK):
		{
         int bw;
		   sipua_call_t *call;

		   call = e->call_info;
            
         if(call->status == SIPUA_STATUS_ONHOLD)
			{
            exit(1);
            client->sipua.attach_source(&client->sipua, call, (transmit_source_t*)client->background_source);
				break;
			}

			call->rtpcapset->rtpcap_selection = RTPCAP_ALL_CAPABLES;

			/* Now create rtp sessions of the call */
			bw = sipua_establish_call(sipua, call, "playback", call->rtpcapset,
                    client->format_handlers, client->control, client->pt);

			if(bw < 0)
			{
				sipua_answer(&client->sipua, call, SIP_STATUS_CODE_TEMPORARILYUNAVAILABLE, NULL);
				break;
			}

         /**
          * Find sessions in the format which match cname and mimetype in source
          * Move_member_from_session_of(call->rtp_format) to_session_of(source->players);
          */
         if(call->status == SIPUA_STATUS_QUEUE && client->background_source)
         {
            sipua->unsubscribe_bandwidth(sipua, call);
            source_associate_guests(client->background_source, call->rtp_format);
         }


			call->status = SIPUA_EVENT_ACK;

			break;
		}
		case(SIPUA_EVENT_REQUESTFAILURE):
		{
			sipua_call_t *call = e->call_info;
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;
         user_profile_t *user_prof = call->user_prof;
         
         clie_debug(("client_sipua_event: status_cade[%d]\n", call_e->status_code));

			client->ogui->ui.beep(&client->ogui->ui);

         clie_debug(("client_sipua_event: SIPUA_EVENT_REQUESTFAILURE\n"));

			if(call_e->status_code == SIPUA_STATUS_UNKNOWN)
				call->status = SIPUA_EVENT_REQUESTFAILURE;
			else
				call->status = call_e->status_code;

         if(call_e->status_code == SIP_STATUS_CODE_PROXYAUTHENTICATIONREQUIRED)
         {
            char *userid, *passwd;

            clie_log(("client_sipua_event: realm[%s] requires authentication\n", call_e->auth_realm));

            user_prof->auth = 0;

            if(sipua->uas->has_authentication_info(sipua->uas, user_prof->username, call_e->auth_realm))
            {
               user_prof->auth = 1;
            }
            else if(client->on_authenticate)
                  client->on_authenticate(client->user_on_authenticate, sipua->proxy_realm, user_prof, &userid, &passwd);

            if(user_prof->auth)
            {               
               sipua_retry_call(sipua, call);
               break;
            }
         }
         else if(client->on_progress)
            client->on_progress(client->user_on_progress, call, call_e->status_code);

			break;
		}
		case(SIPUA_EVENT_SERVERFAILURE):
		{
			sipua_call_t *call = e->call_info;
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;
         clie_debug(("client_sipua_event: status_cade[%d]\n", call_e->status_code));

			client->ogui->ui.beep(&client->ogui->ui);
			printf("client_sipua_event: SIPUA_EVENT_SERVERFAILURE\n");

			if(call_e->status_code == SIPUA_STATUS_UNKNOWN)
				call->status = SIPUA_EVENT_SERVERFAILURE;
			else
				call->status = call_e->status_code;

         break;
		}
		case(SIPUA_EVENT_GLOBALFAILURE):
		{
			sipua_call_t *call = e->call_info;
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;
         clie_debug(("client_sipua_event: status_cade[%d]\n", call_e->status_code));

			client->ogui->ui.beep(&client->ogui->ui);
			printf("client_sipua_event: SIPUA_EVENT_GLOBALFAILURE\n");

			if(call_e->status_code == SIPUA_STATUS_UNKNOWN)
				call->status = SIPUA_EVENT_GLOBALFAILURE;
			else
				call->status = call_e->status_code;

			break;
		}
	}

	return UA_OK;
}

int sipua_done_sip_session(void* gen)
{
	sipua_call_t *call = (sipua_call_t*)gen;

   if(!call)
      return UA_OK;
	
	if(call->setid.id)
   {
      xfree(call->setid.id);
      xfree(call->setid.nettype);
      xfree(call->setid.addrtype);
      xfree(call->setid.netaddr);
   }
   
	if(call->version)
	   xfree(call->version);

	if(call->subject)
	   xstr_done_string(call->subject);
	if(call->info)
	   xstr_done_string(call->info);

	if(call->proto)
      xstr_done_string(call->proto);
	if(call->from)
      xstr_done_string(call->from);

	if(call->rtp_format)
      call->rtp_format->done(call->rtp_format);
   
	if(call->rtpcapset)
	   rtp_capable_done_set(call->rtpcapset);

	xfree(call);
	
	return UA_OK;
}

int client_done_call(sipua_t* sipua, sipua_call_t* call)
{
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	ua->control->release_bandwidth(ua->control, call->bandwidth_hold);

   return sipua_done_sip_session(call);
}

int sipua_done(sipua_t *sipua)
{
	int i;
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	sipua->uas->shutdown(sipua->uas);

	for(i=0; i<MAX_SIPUA_LINES; i++)
		if(ua->lines[i])
			sipua_done_sip_session(ua->lines[i]);


	xthr_done_lock(ua->lines_lock);
   xthr_done_lock(ua->nring_lock);
   conf_done(ua->conf);

	xfree(ua);

	return UA_OK;

}

user_t* client_load_userdata (sipua_t* sipua, const char* loc, const char* uid, const char* tok, const int tsz)
{
	ogmp_client_t* clie = (ogmp_client_t*)sipua;
   
   clie->user = sipua_load_user (loc, uid, tok, tsz);
   if(clie->user)
   {
      sipua_locate_user(sipua, clie->user);
   }

   return clie->user;
}

/* NOTE! if match, return ZERO, otherwise -1 */
int sipua_match_call(void* tar, void* pat)
{
	sipua_call_t *call = (sipua_call_t*)tar;
	sipua_setid_t *id = (sipua_setid_t*)pat;

	if(!strcmp(call->setid.id, id->id) &&
		!strcmp(call->setid.username, id->username) &&
		!strcmp(call->setid.nettype, id->nettype) &&
		!strcmp(call->setid.addrtype, id->addrtype) &&
		!strcmp(call->setid.netaddr, id->netaddr))
			return 0;

	return -1;
}

sipua_call_t* client_find_call(sipua_t* sipua, char* id, char* username, char* nettype, char* addrtype, char* netaddr)
{
	int i;
	ogmp_client_t *ua = (ogmp_client_t*)sipua;


	for(i=0; i<MAX_SIPUA_LINES; i++)
	{
		sipua_call_t* call = ua->lines[i];


		if(call && !strcmp(call->setid.id, id) && 
			!strcmp(call->setid.username, username) && 
			!strcmp(call->setid.nettype, nettype) && 

			!strcmp(call->setid.addrtype, addrtype) && 
			!strcmp(call->setid.netaddr, netaddr))
			break;
	}

	return ua->lines[i];
}

sipua_call_t* client_make_call(sipua_t* sipua, const char* subject, int sbytes, const char *desc, int dbytes)
{
   int ret;
	sipua_setting_t *setting;
	sipua_call_t* call;
	ogmp_client_t* clie = (ogmp_client_t*)sipua;

	int bw_budget;

	if(clie->lines[0])
   {
        clie_log(("client_call: You are in call, can not make a new call\n"));
        return NULL;
   }

	bw_budget = clie->control->book_bandwidth(clie->control, MAX_CALL_BANDWIDTH);

	setting = sipua->setting(sipua);

	call = sipua_new_call(sipua, clie->sipua.user_profile, NULL, subject, sbytes, desc, dbytes);
	if(!call)
	{
		clie_log(("client_new_call: no call created\n"));

		return NULL;
	}

   client_place_call(sipua, call);

	ret = sipua_make_call(sipua, call, NULL, clie->mediatypes,
                  clie->default_rtp_ports, clie->default_rtcp_ports,
                  clie->nmedia, clie->control, bw_budget, setting->codings,
                  setting->ncoding, clie->pt);

   if(ret < UA_OK)
   {
      client_release_call(sipua, call);
      return NULL;
   }

	return call;
}


char* client_set_call_source(sipua_t* sipua, sipua_call_t* call, media_source_t* source)
{
	sipua_setting_t *setting;
	ogmp_client_t* clie = (ogmp_client_t*)sipua;
   char *sdp;

	setting = sipua->setting(sipua);

	sdp = sipua_call_sdp(sipua, call, MAX_CALL_BANDWIDTH, clie->control, clie->mediatypes,
                       clie->default_rtp_ports, clie->default_rtcp_ports,
                       clie->nmedia, source, clie->pt);
	if(sdp)
	{
      printf("client_set_call_source: sdp\n");
      printf("%s\n", sdp);
      printf("client_set_call_source: sdp\n");

      call->renew_body = sdp;
	}

	return sdp;
}

int client_set_profile(sipua_t* sipua, user_profile_t* prof)

{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	client->sipua.user_profile = prof; 

	return UA_OK;

}

user_profile_t* client_profile(sipua_t* sipua)
{
	return sipua->user_profile;
}

/*FIXME: how about unicode authentication data*/
int client_authenticate(sipua_t *sipua, int profile_no, const char* realm, const char *auth_id, int authbytes, const char* passwd, int pwdbytes)
{
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

   user_profile_t *prof = (user_profile_t*)xlist_at(ua->user->profiles, profile_no);
   if(!prof)
      return UA_FAIL;

   if(sipua->uas->set_authentication_info(sipua->uas, prof->username, auth_id, passwd, realm) < UA_OK)
      prof->auth = 0;
   else
      prof->auth = 1;

   return UA_OK;
}

int client_regist(sipua_t *sipua, user_profile_t *prof, char *userloc)
{
	int ret;
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	/* before start, check if already have a register transaction */
	if(client->reg_profile && client->reg_profile != prof)
		return UA_FAIL;

	client->reg_profile = prof;

	ret = sipua_regist(sipua, prof, userloc);

	if(ret < UA_OK)

      client->reg_profile = NULL;
		
	return ret;
}

int client_unregist(sipua_t *sipua, user_profile_t *user)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;
	int ret;

	/* before start, check if already have a register transaction */
	if(client->reg_profile)
		return UA_FAIL;

	client->reg_profile = user;

	if(user->thread_register)
	{
		user->enable = 0;

		xthr_wait(user->thread_register, &ret);
	}

	ret = sipua_unregist(sipua, user);

	if(ret < UA_OK)
        client->reg_profile = NULL;

	return ret;
}

int client_register_profile (sipua_t *sipua, int profile_no)
{
	user_profile_t *prof;
	ogmp_client_t *client = (ogmp_client_t*)sipua;
   
   if(!client->user)
   {
	   clie_debug(("client_register_profile: no user\n"));
      return UA_FAIL;
   }   
   
   prof = (user_profile_t*)xlist_at(client->user->profiles, profile_no);
   if(!prof)
   {
	   clie_debug(("client_register_profile: no profile\n"));
      return UA_FAIL;
   }
   if(prof->reg_status != SIPUA_STATUS_NORMAL)
   {
	   clie_debug(("client_register_profile: busy\n"));
      return UA_BUSY;
   }
   
   prof->regno = -1;
   sipua->regist(sipua, prof, client->user->userloc);
   
	clie_debug(("client_register_profile: registering ...\n"));
   
   return UA_OK;

}

int client_unregister_profile (sipua_t *sipua, int profile_no)
{
	user_profile_t *prof;
	ogmp_client_t *client = (ogmp_client_t*)sipua;

   if(!client->user)
      return UA_FAIL;

   prof = (user_profile_t*)xlist_at(client->user->profiles, profile_no);
   if(!prof)
      return UA_FAIL;
      
   if(prof->reg_status != SIPUA_STATUS_REG_OK)
      return UA_BUSY;

   if(prof == sipua->profile(sipua))
      return UA_REJECT;

   prof->regno = -1;
   sipua->unregist(sipua, prof);

	clie_debug(("client_unregister_profile: unregistering ...\n"));


   return UA_OK;
}

/* lines management */
int client_lock_lines(sipua_t* sipua)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	xthr_lock(client->lines_lock);

	return UA_OK;
}

int client_unlock_lines(sipua_t* sipua)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	xthr_unlock(client->lines_lock);

	return UA_OK;
}

int client_line_range(sipua_t* sipua, int *min, int *max)
{
	*min = 1;
	*max = MAX_SIPUA_LINES;
   
	return UA_OK;
}

int client_lines(sipua_t* sipua, int *nbusy)
{
	int i;
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	for(i=0; i<MAX_SIPUA_LINES; i++)
		if(client->lines[i])
			*nbusy++;

	return MAX_SIPUA_LINES;
}

int client_busylines(sipua_t* sipua, int *busylines, int nlines)
{
	int i;
	int n=0;
    
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	xthr_lock(client->lines_lock);

   for(i=0; i<nlines; i++)
      if(client->lines[i])
			busylines[n++] = i;


	xthr_unlock(client->lines_lock);

	return n;
}

/* current call session */
sipua_call_t* client_session(sipua_t* sipua)
{
   sipua_call_t *call = NULL;   
   ogmp_client_t *client = (ogmp_client_t*)sipua;

   xthr_lock(client->lines_lock);
   
   call = client->lines[0];
   
   xthr_unlock(client->lines_lock);

   return call;
}

sipua_call_t* client_line(sipua_t* sipua, int line)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	return client->lines[line];
}

media_source_t* client_open_source(sipua_t* sipua, char* name, char* mode, void* param)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

   return source_open(name, client->control, mode, param);
}


int client_close_source(sipua_t* sipua, media_source_t* src)
{
    /*
    ogmp_client_t *client = (ogmp_client_t*)sipua;
    */

	return src->done(src);
}

media_source_t* client_set_background_source(sipua_t* sipua, char* name, char *subject, char *info)
{
   ogmp_client_t *client = (ogmp_client_t*)sipua;

   if(!client->sipua.user_profile)
	{
		clie_debug(("client_set_background_source: No user profile found\n"));
		return NULL;
	}

	if(client->background_source)

   {
      client->background_source->stop(client->background_source);
      client->background_source->done(client->background_source);
      client->background_source = NULL;

		if(client->background_source_subject)
			xstr_done_string(client->background_source_subject);
         
		if(client->background_source_info)
			xstr_done_string(client->background_source_info);

		client->background_source_subject = NULL;
		client->background_source_info = NULL;
   }

   if(name && name[0])
	{
      netcast_parameter_t np;

		np.subject = client->background_source_subject;
		np.info = client->background_source_info;

		np.user_profile = client->sipua.user_profile;

		client->background_source = source_open(name, client->control, "netcast", &np);		
		client->background_source_subject = xstr_clone(subject);
		client->background_source_info = xstr_clone(info);
	}

    return client->background_source;
}

/* call media attachment */
int client_attach_source(sipua_t* sipua, sipua_call_t* call, transmit_source_t* tsrc)
{
	xlist_user_t lu;
	char *cname = call->rtpcapset->cname;
	/*
	where are you?
	int bw_budget = clie->control->book_bandwidth(clie->control, MAX_CALL_BANDWIDTH);
	*/
	rtpcap_descript_t *rtpcap = xlist_first(call->rtpcapset->rtpcaps, &lu);

	while(rtpcap)
	{
		if(rtpcap->netaddr)
		{
			/* The net address for the media */
			tsrc->add_destinate(tsrc, rtpcap->profile_mime, cname, 
								rtpcap->nettype, 
								rtpcap->addrtype, 
								rtpcap->netaddr, 
								rtpcap->rtp_portno, rtpcap->rtcp_portno);
		}
		else
		{
			/* The default media net address */


			tsrc->add_destinate(tsrc, rtpcap->profile_mime, cname, 

								call->rtpcapset->nettype, 
								call->rtpcapset->addrtype, 
								call->rtpcapset->netaddr, 
								rtpcap->rtp_portno, rtpcap->rtcp_portno);
		}

	
		rtpcap = xlist_next(call->rtpcapset->rtpcaps, &lu);
	}

	return MP_EIMPL;
}

int client_detach_source(sipua_t* sipua, sipua_call_t* call, transmit_source_t* tsrc)
{
	return MP_EIMPL;
}
	
/* switch current call session */
sipua_call_t* client_pick(sipua_t* sipua, int line)
{
   sipua_call_t *current;
   ogmp_client_t *client = (ogmp_client_t*)sipua;

   xthr_lock(client->lines_lock);

   current = client->lines[0];
   if(current)
		current->status = SIPUA_STATUS_ONHOLD;

	client->lines[0] = client->lines[line];
	client->lines[line] = current;

   if(client->lines[0])
		client->lines[0]->status = SIPUA_STATUS_SERVE;
      
   xthr_unlock(client->lines_lock);

	/* FIXME: Howto transfer call from waiting session */

   return client->lines[0];
}

int client_hold(sipua_t* sipua)
{
	int i=0;
	ogmp_client_t *client = (ogmp_client_t*)sipua;
   sipua_call_t *current = client->lines[0];

   current = client->lines[0];
   if(!current)
		return -1;

   xthr_lock(client->lines_lock);
   
   for(i=0; i<MAX_SIPUA_LINES; i++)
		if(!client->lines[i])
			break;

   /* There always a free line available */
   current->status = SIPUA_STATUS_ONHOLD;

   client->lines[i] = current;
   client->lines[0] = NULL;

   xthr_unlock(client->lines_lock);

	/* FIXME: Howto transfer call to waiting session */

	return i;
}

int client_answer(sipua_t *sipua, sipua_call_t* call, int reply, media_source_t* source)
{  
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	switch(reply)
	{        
		case SIP_STATUS_CODE_OK:
		{
		   /**
          * Answer the call with new sdp, if source available.
          */
         if(source)
         {
            char *sdp = client_set_call_source(sipua, call, source);
            
            /**
             * Find sessions in the format which match cname and mimetype in source
             * Move_member_from_session_of(call->rtp_format) to_session_of(source->players);
             */
            source_associate_guests(source, call->rtp_format);

            sipua_answer(&client->sipua, call, reply, sdp);

            xstr_done_string(sdp);
         }
         else
         {
            sipua_answer(&client->sipua, call, reply, call->reply_body);
         }
            
			xfree(call->reply_body);
			call->reply_body = NULL;
			
			break;
		}
		default:
      {
			/* Anser the call */
			sipua_answer(&client->sipua, call, reply, NULL);
      }
	}

	return UA_OK;
}

int client_queue(sipua_t *sipua, sipua_call_t* call)
{
   char *sdp;

   ogmp_client_t *client = (ogmp_client_t*)sipua;

   if(!client->background_source)
   {
      clie_debug (("client_new: No background source!\n"));      
      return UA_FAIL;                                  
   }
   
   sdp = client_set_call_source(sipua, call, client->background_source);

   call->status = SIPUA_STATUS_QUEUE;
   
   sipua_answer(&client->sipua, call, SIP_STATUS_CODE_OK, sdp);

   xstr_done_string(sdp);
   
	return UA_OK;
}

/*****************************
 *  Set Callbacks for SIPUA  *
 *****************************/

int client_set_register_callback (sipua_t *sipua, int(*callback)(void*callback_user,int result,char*reason,int isreg), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_register = callback;
    client->user_on_register = callback_user;
    
    return UA_OK;
}

int client_set_progress_callback (sipua_t *sipua, int(*callback)(void*callback_user, sipua_call_t *call, int statuscode), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_progress = callback;
    client->user_on_progress = callback_user;

    return UA_OK;
}

int client_set_authentication_callback (sipua_t *sipua, int(*callback)(void*callback_user, char* realm, user_profile_t* profile, char** user_id, char** user_password), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_authenticate = callback;
    client->user_on_authenticate = callback_user;

    return UA_OK;
}

int client_set_newcall_callback (sipua_t *sipua, int(*callback)(void*callback_user,int lineno,sipua_call_t *call), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_newcall = callback;
    client->user_on_newcall = callback_user;
    
    return UA_OK;
}        

int client_set_terminate_callback (sipua_t *sipua, int(*callback)(void*callback_user, sipua_call_t *call, int statuscode), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_terminate = callback;
    client->user_on_terminate = callback_user;

    return UA_OK;
}

int client_set_conversation_start_callback (sipua_t *sipua, int(*callback)(void *callback_user,int lineno,sipua_call_t *call), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_conversation_start = callback;
    client->user_on_conversation_start = callback_user;
    
    return UA_OK;
}

int client_set_conversation_end_callback (sipua_t *sipua, int(*callback)(void *callback_user,int lineno,sipua_call_t *call), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_conversation_end = callback;
    client->user_on_conversation_end = callback_user;
    
    return UA_OK;
}

int client_set_bye_callback (sipua_t *sipua, int (*callback)(void *callback_user, int lineno, char *caller, char *reason), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;


    client->on_bye = callback;
    client->user_on_bye = callback_user;

    return UA_OK;
}

int client_subscribe_bandwidth(sipua_t *sipua, sipua_call_t* call)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;
   return call->bandwidth_hold = client->control->book_bandwidth(client->control, call->bandwidth_need);
}

int client_unsubscribe_bandwidth(sipua_t *sipua, sipua_call_t* call)
{
   int hold;
   
   ogmp_client_t* client = (ogmp_client_t*)sipua;

   hold = call->bandwidth_hold;
   
   client->control->release_bandwidth(client->control, call->bandwidth_hold);
   call->bandwidth_hold = 0;

   return hold;
}

/************************************/
/*  End of set Callbacks for SIPUA  */
/************************************/ 

sipua_t* client_new(char *uitype, sipua_uas_t* uas, char* proxy_realm, module_catalog_t* mod_cata, int bandwidth)
{
	int nformat;

	ogmp_client_t *client=NULL;

   sipua_t* sipua;

	client = xmalloc(sizeof(ogmp_client_t));
	memset(client, 0, sizeof(ogmp_client_t));

	client->ogui = (ogmp_ui_t*)client_new_ui(client, mod_cata, uitype);
   if(client->ogui == NULL)
   {
        clie_debug (("client_new: No %s module found!\n", uitype));
        xfree(client);

        return NULL;
   }    
    
	sipua = (sipua_t*)client;

   client->ogui->set_sipua(client->ogui, sipua);

	client->control = new_media_control();

	client->nring_lock = xthr_new_lock();


	/* Initialise */
	client->conf = conf_new ( "ogmprc" );
   

	/* set sip client */
	client->valid = 0;

	/* player controler */
	client->control->config(client->control, client->conf, mod_cata);

	client->control->add_device(client->control, "rtp", client_config_rtp, client->conf);
	
	client->format_handlers = xlist_new();

   nformat = catalog_create_modules (mod_cata, "format", client->format_handlers);

   clie_log (("client_new_sipua: %d format module found\n", nformat));

   client->lines_lock = xthr_new_lock();

	/* set media ports, need to seperate to configure module */
	client->mediatypes[0] = xstr_clone("audio");
	client->default_rtp_ports[0] = 3500;
	client->default_rtcp_ports[0] = 3501;
	client->nmedia = 1;

	client->control->set_bandwidth_budget(client->control, bandwidth);

	uas->set_listener(uas, client, client_sipua_event);

	sipua->uas = uas;
   sipua->proxy_realm = xstr_clone(proxy_realm);

	sipua->done = sipua_done;

	sipua->userloc = sipua_userloc;
	sipua->locate_user = sipua_locate_user;

	sipua->load_userdata = client_load_userdata;
   
	sipua->set_profile = client_set_profile;
	sipua->profile = client_profile;

	sipua->authenticate = client_authenticate;
	sipua->regist = client_regist;
	sipua->unregist = client_unregist;

	sipua->register_profile = client_register_profile;
	sipua->unregister_profile = client_unregister_profile;

	sipua->make_call = client_make_call;
	sipua->done_call = client_done_call;

 	/* lines management */
 	sipua->lock_lines = client_lock_lines;
 	sipua->unlock_lines = client_unlock_lines;

   sipua->line_range = client_line_range;
   sipua->lines = client_lines;
 	sipua->busylines = client_busylines;
 	/* current call session */

	sipua->session = client_session;

 	sipua->line = client_line;

   sipua->set_background_source = client_set_background_source;
	sipua->setting = client_setting;

	sipua->open_source = client_open_source;
	sipua->close_source = client_close_source;
	sipua->attach_source = client_attach_source;
	sipua->detach_source = client_detach_source;

	/* switch current call session */
 	sipua->pick = client_pick;
 	sipua->hold = client_hold;
  
	sipua->call = sipua_call;   
   
	sipua->answer = client_answer;
   
	sipua->queue = client_queue;

	sipua->options_call = sipua_options_call;
	sipua->info_call = sipua_info_call;

   sipua->bye = sipua_bye;
   sipua->set_call_source = client_set_call_source;
   
   /* Set Callbacks */    
   sipua->set_register_callback = client_set_register_callback;
   sipua->set_authentication_callback = client_set_authentication_callback;
   sipua->set_progress_callback = client_set_progress_callback;
   sipua->set_newcall_callback = client_set_newcall_callback;
   sipua->set_terminate_callback = client_set_terminate_callback;
   sipua->set_conversation_start_callback = client_set_conversation_start_callback;

   sipua->set_conversation_end_callback = client_set_conversation_end_callback;
   sipua->set_bye_callback = client_set_bye_callback;

   sipua->subscribe_bandwidth = client_subscribe_bandwidth;
   sipua->unsubscribe_bandwidth = client_unsubscribe_bandwidth;
    
   clie_log(("client_new: sipua ready!\n\n"));

	return sipua;
}

int client_start(sipua_t* sipua)
{
	ogmp_client_t *clie = (ogmp_client_t*)sipua;


	clie->ogui->ui.show(&clie->ogui->ui);

	return MP_OK;
}

int client_done_uas(void* gen)
{
    sipua_uas_t* uas = (sipua_uas_t*)gen;

    uas->done(uas);

    return UA_OK;
}

sipua_uas_t* client_new_uas(module_catalog_t* mod_cata, char* type)
{
    sipua_uas_t* uas = NULL;

    xlist_user_t lu;
    xlist_t* uases  = xlist_new();
    int found = 0;

    int nmod = catalog_create_modules (mod_cata, "uas", uases);
    if(nmod)
    {
        uas = (sipua_uas_t*)xlist_first(uases, &lu);
        while(uas)
        {
            if(uas->match_type(uas, type))
            {
                xlist_remove_item(uases, uas);
                found = 1;
                break;
            }   

            uas = xlist_next(uases, &lu);
        }
    }

    xlist_done(uases, client_done_uas);

    if(!found)
        return NULL;

    return uas;
}
