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
 
#include "format_rtp/rtp_format.h"

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

#define SIPUA_MAX_RING 6
#define MAX_CALL_BANDWIDTH  5000  /* in Bytes */

/****************************************************************************************/
int client_call_ringing(void* gen)
{
	int i;
	sipua_set_t* call;
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

		for(i=0; i<MAX_SIPUA_LINES; i++)
		{
			call = client->lines[i];
            
			if(call && call->status == SIPUA_EVENT_NEW_CALL)
			{
				ringing = 1;

				if(call->ring_num == 0)
				{
					/* On hold or redirect this call */
					call->status = SIPUA_EVENT_ONHOLD;
					client->nring--;

					if(client->background_source)
						client->sipua.answer(&client->sipua, call, SIPUA_STATUS_ANSWER, client->background_source);

					if(client->nring == 0)
						break;

					continue;
				}
				
				sipua_answer(&client->sipua, call, SIPUA_STATUS_RINGING, NULL);

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
			client_regist(&client->sipua, user_prof, user_prof->cname);

		time_msec_sleep(clock, intv*1000, &ms_remain);
	}

	time_end(clock);

	user_prof->thread_register = NULL;

	return UA_OK;
}

int client_sipua_event(void* lisener, sipua_event_t* e)
{
	sipua_t *sipua = (sipua_t*)lisener;

	ogmp_client_t *client = (ogmp_client_t*)lisener;

	switch(e->type)
	{
		case(SIPUA_EVENT_REGISTRATION_SUCCEEDED):
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
			}

			if(user_prof->reg_status == SIPUA_STATUS_UNREG_DOING)
            {
                xstr_done_string(user_prof->reg_server);
                user_prof->reg_server = NULL;

				user_prof->reg_status = SIPUA_STATUS_NORMAL;
            }

			/* registering transaction completed */


            client->reg_profile = NULL;

			/* Should be exact expire seconds returned by registrary*/
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
			if(!user_prof->thread_register)
				user_prof->thread_register = xthr_new(client_register_loop, user_prof, XTHREAD_NONEFLAGS);

            if(client->on_register)
                client->on_register(client->user_on_register, SIPUA_STATUS_REG_OK, user_prof->reg_reason_phrase);
                
            //client->ogui->ui.beep(&client->ogui->ui);

			break;
		}
		case(SIPUA_EVENT_REGISTRATION_FAILURE):
		case(SIPUA_EVENT_UNREGISTRATION_FAILURE):
		{
			char buf[100];

			sipua_reg_event_t *reg_e = (sipua_reg_event_t*)e;
			user_profile_t* user_prof = client->reg_profile;

            if(!user_prof)
                break;
                
            snprintf(buf, 99, "Register %s Error[%i]: %s",
					reg_e->server, reg_e->status_code, reg_e->server_info);

			clie_log(("client_sipua_event: %s\n", buf));

			user_prof->reg_status = SIPUA_STATUS_REG_FAIL;

			if(user_prof->reg_reason_phrase) 
				xfree(user_prof->reg_reason_phrase);
                
			user_prof->reg_reason_phrase = xstr_clone(reg_e->server_info);

			/* registering transaction completed */
			client->reg_profile = NULL;

            if(client->on_register)
                client->on_register(client->user_on_register, SIPUA_STATUS_REG_FAIL, user_prof->reg_reason_phrase);

			break;
		}
		case(SIPUA_EVENT_NEW_CALL):
		{
			/* Callee receive a new call, now in negotiation stage */
			int i;
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;
			sipua_set_t *call;

			int lineno = e->lineno;

			rtpcap_set_t* rtpcapset;

			sdp_message_t *sdp_message;

			char* sdp_body = (char*)e->content;

			char *p, *q;

			sdp_message_init(&sdp_message);

			if (sdp_message_parse(sdp_message, sdp_body) != 0)
			{
				clie_log(("\nsipua_event: fail to parse sdp\n"));
				sdp_message_free(sdp_message);
				
				return UA_FAIL;
			}

			rtpcapset = rtp_capable_from_sdp(sdp_message);
			rtpcapset->user_profile = client->sipua.user_profile;

			/* now to negotiate call, which will generate call structure with sdp message for reply
			 * still no rtp session created !
			 */
			call = sipua_negotiate_call(sipua, client->sipua.user_profile, rtpcapset,
							client->mediatypes, client->default_rtp_ports, 
							client->default_rtcp_ports, client->nmedia,
							client->control, MAX_CALL_BANDWIDTH);
			
			if(!call)
			{
				/* Fail to receive the call, miss it */
				break;
			}

			call_e->event.call_info = call;

			call->cid = call_e->cid;

			call->did = call_e->did;
			call->rtpcapset = rtpcapset;
            
			call->status = SIPUA_EVENT_NEW_CALL;

            /* Parse from uri */
			p = call_e->remote_uri;
			while(*p != '<') p++;
			q = ++p;
			while(*q != ':') q++;

			call->proto = xstr_nclone(p, q-p);
			p = ++q;
			while(*q != '>') q++;

			call->from = xstr_nclone(p, q-p);

			for(i=0; i<MAX_SIPUA_LINES; i++)
			{
				if(client->lines[i] == NULL)
				{
					call->ring_num = SIPUA_MAX_RING;

					client->lines[i] = call;
					sipua->uas->accept(sipua->uas, lineno);

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

					break;
				}
			}

            /* Cannot free here, need for media specific parsing !!!
            sdp_message_free(sdp_message);
            */
			break;
		}
		case(SIPUA_EVENT_PROCEEDING):
		{
			sipua_set_t *call = e->call_info;

			call->status = SIPUA_EVENT_PROCEEDING;

			break;
		}
		case(SIPUA_EVENT_RINGING):
		{	
			sipua_set_t *call = e->call_info;

			call->status = SIPUA_EVENT_RINGING;

			break;
		}
		case(SIPUA_EVENT_ANSWERED):
		{
			/* Caller establishs call when callee is answered */
			rtpcap_set_t* rtpcapset;
			sdp_message_t *sdp_message;
			char* sdp_body = (char*)e->content;

            int bw;

			sipua_set_t *call = e->call_info; 
			call->status = SIPUA_EVENT_ANSWERED;
			/*
			printf("client_sipua_event: call[%s] answered\n", call->subject);
			printf("client_sipua_event: SDP\n");
			printf("------------------------\n");
			printf("%s", sdp_body);
			printf("------------------------\n");
			*/
			sdp_message_init(&sdp_message);

			if (sdp_message_parse(sdp_message, sdp_body) != 0)
			{
				clie_log(("\nsipua_event: fail to parse sdp\n"));
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
				sipua_answer(&client->sipua, call, SIPUA_STATUS_DECLINE, NULL);

				break;
			}

			rtp_capable_done_set(rtpcapset);

			break;
		}
		case(SIPUA_EVENT_ACK):
		{
            int bw;

			sipua_set_t *call = e->call_info;
            
            if(call->status == SIPUA_EVENT_ONHOLD)
			{
				client->sipua.attach_source(&client->sipua, call, (transmit_source_t*)client->background_source);
				break;
			}

			call->rtpcapset->rtpcap_selection = RTPCAP_ALL_CAPABLES;

			/* Now create rtp sessions of the call */
			bw = sipua_establish_call(sipua, call, "playback", call->rtpcapset,
                                client->format_handlers, client->control, client->pt);

			if(bw < 0)
			{
				sipua_answer(&client->sipua, call, SIPUA_STATUS_REJECT, NULL);
				break;
			}

			call->status = SIPUA_EVENT_ACK;

			break;
		}
		case(SIPUA_EVENT_REQUESTFAILURE):
		{
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;
			sipua_set_t *call = e->call_info;

			client->ogui->ui.beep(&client->ogui->ui);
			printf("client_sipua_event: SIPUA_EVENT_REQUESTFAILURE\n");

			if(call_e->status_code == SIPUA_STATUS_UNKNOWN)
				call->status = SIPUA_EVENT_REQUESTFAILURE;
			else
				call->status = call_e->status_code;

			break;
		}
		case(SIPUA_EVENT_SERVERFAILURE):
		{
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;
			sipua_set_t *call = e->call_info;

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
			sipua_call_event_t *call_e = (sipua_call_event_t*)e;
			sipua_set_t *call = e->call_info;

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
	sipua_set_t *set = (sipua_set_t*)gen;
	
	if(set->setid.nettype) xfree(set->setid.nettype);

	if(set->setid.addrtype) xfree(set->setid.addrtype);
	if(set->setid.netaddr) xfree(set->setid.netaddr);

	xfree(set->setid.id);
	xfree(set->version);

	xstr_done_string(set->subject);
	xstr_done_string(set->info);

    xstr_done_string(set->proto);
    xstr_done_string(set->from);


	set->rtp_format->done(set->rtp_format);
	rtp_capable_done_set(set->rtpcapset);

	xfree(set);
	
	return UA_OK;
}

int client_done_call(sipua_t* sipua, sipua_set_t* set)
{
	int i;
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	if(ua->sipua.incall != set)
    {
		for(i=0; i<MAX_SIPUA_LINES; i++)

        {
			if(ua->lines[i] == set)
			{
				ua->lines[i] = NULL;
				break;
			}
        }
    }
	else
	{
		ua->sipua.incall = NULL;
	}

	ua->control->release_bandwidth(ua->control, set->bandwidth);




	return sipua_done_sip_session(set);
}

/*
int client_done(ogmp_client_t *client)
{
   client->rtp_format->close(client->rtp_format);

   xlist_done(client->format_handlers, client_done_format_handler);

   xthr_done_lock(client->nring_lock);
   conf_done(client->conf);

   if(client->sdp_body)
	   free(client->sdp_body);

   xfree(client);

   return MP_OK;
}
*/

int sipua_done(sipua_t *sipua)
{
	int i;
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	sipua->uas->shutdown(sipua->uas);

	for(i=0; i<MAX_SIPUA_LINES; i++)
		if(ua->lines[i])
			sipua_done_sip_session(ua->lines[i]);

	if(ua->sipua.incall)
		sipua_done_sip_session(ua->sipua.incall);

	xthr_done_lock(ua->lines_lock);
    xthr_done_lock(ua->nring_lock);
    conf_done(ua->conf);

	xfree(ua);

	return UA_OK;
}

/* NOTE! if match, return ZERO, otherwise -1 */
int sipua_match_call(void* tar, void* pat)
{
	sipua_set_t *set = (sipua_set_t*)tar;
	sipua_setid_t *id = (sipua_setid_t*)pat;

	if(	!strcmp(set->setid.id, id->id) && 
		!strcmp(set->setid.username, id->username) && 
		!strcmp(set->setid.nettype, id->nettype) && 
		!strcmp(set->setid.addrtype, id->addrtype) && 
		!strcmp(set->setid.netaddr, id->netaddr))
			return 0;

	return -1;
}

sipua_set_t* client_find_call(sipua_t* sipua, char* id, char* username, char* nettype, char* addrtype, char* netaddr)
{
	int i;
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	for(i=0; i<MAX_SIPUA_LINES; i++)
	{
		sipua_set_t* set = ua->lines[i];

		if(set && !strcmp(set->setid.id, id) && 
			!strcmp(set->setid.username, username) && 
			!strcmp(set->setid.nettype, nettype) && 
			!strcmp(set->setid.addrtype, addrtype) && 
			!strcmp(set->setid.netaddr, netaddr))
			break;
	}

	return ua->lines[i];
}

sipua_set_t* client_new_call(sipua_t* sipua, char* subject, int sbytes, char *desc, int dbytes)
{
	sipua_setting_t *setting;
	sipua_set_t* call;
	ogmp_client_t* clie = (ogmp_client_t*)sipua;

	int bw_budget = clie->control->book_bandwidth(clie->control, MAX_CALL_BANDWIDTH);

	setting = sipua->setting(sipua);

	call = sipua_new_call(sipua, clie->sipua.user_profile, NULL, subject, sbytes, desc, dbytes,
						clie->mediatypes, clie->default_rtp_ports, clie->default_rtcp_ports, clie->nmedia,
						clie->control, bw_budget, setting->codings, setting->ncoding, clie->pt);

	if(!call)
	{
		clie_log(("client_new_call: no call created\n"));
		return NULL;
	}

	return call;
}

char* client_set_call_source(sipua_t* sipua, sipua_set_t* call, media_source_t* source)
{
	sipua_setting_t *setting;
	ogmp_client_t* clie = (ogmp_client_t*)sipua;
    char *sdp;

	setting = sipua->setting(sipua);

	sdp = sipua_call_sdp(sipua, call, MAX_CALL_BANDWIDTH, clie->control, clie->mediatypes,
                       clie->default_rtp_ports, clie->default_rtcp_ports,
                       clie->nmedia, source, clie->pt);
 
    if(sdp)
        call->renew_body = sdp;

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

int client_regist(sipua_t *sipua, user_profile_t *user, char *userloc)
{
	int ret;
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	/* before start, check if already have a register transaction */
	if(client->reg_profile)
	{
		return UA_FAIL;
	}

	client->reg_profile = user;

	ret = sipua_regist(sipua, user, userloc);

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
sipua_set_t* client_session(sipua_t* sipua)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	return client->sipua.incall;
}

sipua_set_t* client_line(sipua_t* sipua, int line)
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
int client_attach_source(sipua_t* sipua, sipua_set_t* call, transmit_source_t* tsrc)
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

int client_detach_source(sipua_t* sipua, sipua_set_t* call, transmit_source_t* tsrc)
{
	return MP_EIMPL;
}
	
/* switch current call session */
sipua_set_t* client_pick(sipua_t* sipua, int line)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

    if(client->sipua.incall)
	{
		client->lines[line]->status = SIPUA_EVENT_ONHOLD;
		return client->lines[line];
	}

	client->sipua.incall = client->lines[line];
	client->lines[line] = NULL;

	/* FIXME: Howto transfer call from waiting session */

    return client->sipua.incall;
}

int client_hold(sipua_t* sipua)
{
	int i;
	ogmp_client_t *client = (ogmp_client_t*)sipua;


	if(!client->sipua.incall)
		return -1;

	for(i=0; i<MAX_SIPUA_LINES; i++)
		if(!client->lines[i])
			break;

	client->lines[i] = client->sipua.incall;
	client->sipua.incall->status = SIPUA_EVENT_ONHOLD;

	client->sipua.incall = NULL;

	/* FIXME: Howto transfer call to waiting session */

	return i;
}
	
int client_answer(sipua_t *sipua, sipua_set_t* call, int reply, media_source_t* source)
{  
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	switch(reply)
	{        
		case SIPUA_STATUS_ANSWER:
		{
			/**
             * Anser the call with new sdp, if source available.
             */
            if(source)
            {
                char *sdp = client_set_call_source(sipua, call, source);
            
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

/***********************************
 *  Set Callbacks for SIPUA        *
 ***********************************/
int client_set_register_callback (sipua_t *sipua, int(*callback)(void*callback_user,int result,char*reason), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_register = callback;
    client->user_on_register = callback_user;
    return UA_OK;
}

int client_set_newcall_callback (sipua_t *sipua, int(*callback)(void*callback_user,int lineno,char *caller,char *subject,char *info), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_newcall = callback;
    client->user_on_newcall = callback_user;
    return UA_OK;
}

int client_set_conversation_start_callback (sipua_t *sipua, int(*callback)(void *callback_user, int lineno), void* callback_user)
{
    ogmp_client_t *client = (ogmp_client_t*)sipua;

    client->on_conversation_start = callback;
    client->user_on_conversation_start = callback_user;
    return UA_OK;
}

int client_set_conversation_end_callback (sipua_t *sipua, int(*callback)(void *callback_user, int lineno), void* callback_user)
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
/************************************
 *  End of set Callbacks for SIPUA  *
 ************************************/
 

sipua_t* client_new(char *uitype, sipua_uas_t* uas, module_catalog_t* mod_cata, int bandwidth)
{
	int nmod;
	int nformat;

	ogmp_client_t *client=NULL;

    sipua_t* sipua;

	client = xmalloc(sizeof(ogmp_client_t));
	memset(client, 0, sizeof(ogmp_client_t));

	client->ogui = (ogmp_ui_t*)client_new_ui(client, mod_cata, uitype);
    if(client->ogui == NULL)
    {
        clie_log (("client_new: No ui module found!\n"));
        xfree(client);

        return NULL;
    }    
    
	sipua = (sipua_t*)client;

    client->ogui->set_sipua(client->ogui, sipua);

	client->control = new_media_control();

	client->nring_lock = xthr_new_lock();

	/* Initialise */
	client->conf = conf_new ( "ogmprc" );
   
	client->format_handlers = xlist_new();

	nmod = catalog_create_modules (mod_cata, "format", client->format_handlers);
	printf("client_new: %d format module found\n", nmod);

	/* set sip client */
	client->valid = 0;

	/* player controler */
	client->control->config(client->control, client->conf, mod_cata);
	client->control->add_device(client->control, "rtp", client_config_rtp, client->conf);
	
	client->format_handlers = xlist_new();

    nformat = catalog_create_modules (mod_cata, "format", client->format_handlers);

    //clie_log (("client_new_sipua: %d format module found\n", nformat));

    client->lines_lock = xthr_new_lock();

	/* set media ports, need to seperate to configure module */
	client->mediatypes[0] = xstr_clone("audio");
	client->default_rtp_ports[0] = 3500;
	client->default_rtcp_ports[0] = 3501;
	client->nmedia = 1;

	client->control->set_bandwidth_budget(client->control, bandwidth);

	uas->set_listener(uas, client, client_sipua_event);

	sipua->uas = uas;

	sipua->done = sipua_done;

	sipua->userloc = sipua_userloc;
	sipua->locate_user = sipua_locate_user;

	sipua->set_profile = client_set_profile;
	sipua->profile = client_profile;

	sipua->regist = client_regist;
	sipua->unregist = client_unregist;

	sipua->new_call = client_new_call;
	sipua->done_call = client_done_call;

 	/* lines management */
 	sipua->lock_lines = client_lock_lines;
 	sipua->unlock_lines = client_unlock_lines;


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

	sipua->options_call = sipua_options_call;
	sipua->info_call = sipua_info_call;

    sipua->bye = sipua_bye;
    sipua->set_call_source = client_set_call_source;
    
    /* Set Callbacks */
    
    sipua->set_register_callback = client_set_register_callback;
    sipua->set_newcall_callback = client_set_newcall_callback;
    sipua->set_conversation_start_callback = client_set_conversation_start_callback;
    sipua->set_conversation_end_callback = client_set_conversation_end_callback;
    sipua->set_bye_callback = client_set_bye_callback;
    
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
