/***************************************************************************
                          ogmp_client.c
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
#include <timedia/xmalloc.h>

#define CLIE_LOG

#ifdef CLIE_LOG
 #define clie_log(fmtargs)  do{log_printf fmtargs;}while(0)
#else
 #define clie_log(fmtargs)
#endif

struct ogmp_client_s
{
	sipua_t sipua;

	int finish;
	int valid;

	media_control_t *control;

	media_format_t *rtp_format;
	xlist_t *format_handlers;

	sipua_set_t* call;

	config_t * conf;

	char *sdp_body;

	int registered;

	int ncap;
	capable_descript_t *caps[MAX_NCAP];

	xthr_lock_t *course_lock;
	xthr_cond_t *wait_course_finish;

	ogmp_ui_t* ui;

	xthread_t* main_thread;

	xthr_cond_t* wait_unregistered;

	/* below is sipua related */
	user_profile_t* user_prof;
	user_profile_t* reg_profile;

	int expire_sec; /* registration expired seconds, time() */

	uint16 default_rtp_portno;
	uint16 default_rtcp_portno;

	int total_bandwidth;
	int conference_bandwidth;

	sipua_action_t* action;

	int pt[MAX_PAYLOAD_TYPE];
	
	xlist_t* sip_sessions;

	void* lisener;
	int (*notify_event)(void* listener, sipua_event_t* e);
};

int client_done_format_handler(void *gen)
{
   media_format_t * fmt = (media_format_t *)gen;

   fmt->done(fmt);

   return MP_OK;
}

/**
 * return capability number, if error, return ZERO
 */
int client_setup(ogmp_client_t *client, char *mode, rtpcap_set_t *rtpcapset)
{
	/* define a player */
	xlist_user_t u;
	media_format_t *format;

	char cname[256]; /* rtp cname restriction */

	rtp_capable_cname(rtpcapset, cname, 256);

	/* create a media format, also get the capabilities of the file */
	format = (media_format_t *) xlist_first(client->format_handlers, &u);
	while (format)
	{
		/* open media source */
		client->ncap = format->open_capables(format, cname, rtpcapset->rtpcaps, client->control, client->conf, mode, client->caps);
		if(client->ncap > 0)
		{
			client->control->set_format (client->control, "rtp", format);
			client->rtp_format = format;
			client->valid = 1;

			break;
		}
      
		format = (media_format_t*) xlist_next(client->format_handlers, &u);
	}

	if(!client->rtp_format)
	{
		clie_log(("ogmp_client_setup: no format support\n"));
		return 0;
	}

	return client->ncap;
}

/****************************************************************************************
 * SIP Callbacks
 */
int client_action_onregistration(void *user, char *from, int result, void* info)
{
	ogmp_client_t *clie = (ogmp_client_t*)user;
	
	clie->registered = 0;

	switch(result)
	{
		case(SIPUA_EVENT_REGISTRATION_SUCCEEDED):
		{
			clie->registered = 1;

			clie_log(("\nclient_action_onregister: registered to [%s] !\n\n", from));
			break;
		}
		case(SIPUA_EVENT_UNREGISTRATION_SUCCEEDED):
		{
			clie_log(("\nclient_action_onregister: unregistered to [%s] !\n\n", from));

			xthr_cond_broadcast(clie->wait_unregistered);

			break;
		}
		case(SIPUA_EVENT_REGISTRATION_FAILURE):
		{
			clie_log(("\nclient_action_onregister: fail to register to [%s]!\n\n", from));
			break;
		}
		case(SIPUA_EVENT_UNREGISTRATION_FAILURE):
		{
			clie_log(("\nclient_action_onregister: fail to unregister to [%s]!\n\n", from));
			break;
		}
	}

	return MP_OK;
}

int client_action_oncall(void *user, char *from, void* info_in, void* info_out)
{
	char* sdp_body_in = (char*)info_in;
	char** sdp_body_out = (char**)info_out;

	ogmp_client_t *client = (ogmp_client_t*)user;

	rtpcap_set_t* rtpcapset;

	sdp_message_t *sdp_message_in;

	clie_log(("\nclient_action_oncall: Never be called from cn[%s]\n", from));

	sdp_message_init(&sdp_message_in);

	clie_log (("client_action_oncall: received sdp\n"));
	clie_log (("---------------------------------\n"));
	clie_log (("%s", sdp_body_in));
	clie_log (("---------------------------------\n"));

	if(sdp_message_parse(sdp_message_in, sdp_body_in) != 0)
	{
		clie_log(("\nclient_action_oncall: fail to parse sdp\n"));
		sdp_message_free(sdp_message_in);
		return 0;
	}

	/* Retrieve capable from the sdp message */
	rtpcapset = rtp_capable_from_sdp(sdp_message_in);

	clie_log(("\nclient_action_oncall: with cn[%s] requires %d capables\n\n", from, xlist_size(rtpcapset->rtpcaps)));

	client_setup(client, "playback", rtpcapset);

	*sdp_body_out = client->sdp_body;

	sdp_message_free(sdp_message_in);

	return xlist_size(rtpcapset->rtpcaps);
}

int client_action_onconnect(void *user, char *from, void* info_in)
{
	char* sdp_body_in = (char*)info_in;

	ogmp_client_t *client = (ogmp_client_t*)user;

	rtpcap_set_t* rtpcapset;

	sdp_message_t *sdp_message_in;

	clie_log(("\nclient_action_onconnect: Never be called from cn[%s]\n", from));

	sdp_message_init(&sdp_message_in);

	clie_log (("client_action_oncall: received sdp\n"));
	clie_log (("---------------------------------\n"));
	clie_log (("%s", sdp_body_in));
	clie_log (("---------------------------------\n"));

	if(sdp_message_parse(sdp_message_in, sdp_body_in) != 0)
	{
		clie_log(("\nclient_action_onconnect: fail to parse sdp\n"));
		sdp_message_free(sdp_message_in);
		return 0;
	}

	/* Retrieve capable from the sdp message */
	rtpcapset = rtp_capable_from_sdp(sdp_message_in);

	clie_log(("\nclient_action_onconnect: with cn[%s] requires %d capables\n\n", from, xlist_size(rtpcapset->rtpcaps)));

	client_setup(client, "playback", rtpcapset);

	sdp_message_free(sdp_message_in);

	return xlist_size(rtpcapset->rtpcaps);
}

/* change call status */
int client_action_onreset(void *user, char *from_cn, void* info_in)
{
	clie_log(("client_action_onreset: reset from cn[%s]\n", from_cn));

	return 0;
}

int client_action_onbye(void *user, char *from_cn)
{
	clie_log(("client_action_onbye: bye from cn[%s]\n", from_cn));

	return UA_OK;
}
/****************************************************************************************/

int client_done(ogmp_client_t *client)
{
   client->rtp_format->close(client->rtp_format);
   
   xlist_done(client->format_handlers, client_done_format_handler);
   xthr_done_lock(client->course_lock);
   xthr_done_cond(client->wait_course_finish);
   conf_done(client->conf);
   
   if(client->sdp_body)
	   free(client->sdp_body);

   xfree(client);

   return MP_OK;
}

int client_sipua_event(void* lisener, sipua_event_t* e)
{
	sipua_t *sipua = (sipua_t*)lisener;

	ogmp_client_t *ua = (ogmp_client_t*)lisener;

	sipua_set_t *call_info = e->call_info;

	switch(e->type)
	{
		case(SIPUA_EVENT_REGISTRATION_SUCCEEDED):
		case(SIPUA_EVENT_UNREGISTRATION_SUCCEEDED):
		{
			sipua_reg_event_t *reg_e = (sipua_reg_event_t*)e;
			user_profile_t* user_prof = ua->reg_profile;

			if(user_prof->reg_reason_phrase) 
				xfree(user_prof->reg_reason_phrase);

			if(user_prof->reg_status == SIPUA_STATUS_REG_DOING)
			{
				user_prof->reg_server = xstr_clone(reg_e->server);
				
				user_prof->reg_reason_phrase = xstr_clone(reg_e->server_info);

				ua->user_prof = ua->reg_profile;

				user_prof->reg_status = SIPUA_STATUS_REG_OK;
			}

			if(user_prof->reg_status == SIPUA_STATUS_UNREG_DOING)
			{
				user_prof->reg_status = SIPUA_STATUS_NORMAL;
			}

			/* registering transaction completed */
			ua->reg_profile = NULL;

			break;
		}

		case(SIPUA_EVENT_REGISTRATION_FAILURE):
		case(SIPUA_EVENT_UNREGISTRATION_FAILURE):
		{
			char buf[100];

			sipua_reg_event_t *reg_e = (sipua_reg_event_t*)e;
			user_profile_t* user_prof = ua->reg_profile;

			snprintf(buf, 99, "Register %s Error[%i]: %s",
					reg_e->server, reg_e->status_code, reg_e->server_info);

			log_printf("client_sipua_event: %s\n", buf);

			user_prof->reg_status = SIPUA_STATUS_REG_FAIL;

			if(user_prof->reg_reason_phrase) 
				xfree(user_prof->reg_reason_phrase);
			user_prof->reg_reason_phrase = xstr_clone(reg_e->server_info);

			/* registering transaction completed */
			ua->reg_profile = NULL;

			break;
		}

		case(SIPUA_EVENT_NEW_CALL):
		{
			rtp_coding_t *codings;
			int ncoding=0;

			int lineno = e->lineno;

			rtpcap_set_t* rtpcapset;
			rtpcap_descript_t* rtpcap;
			sipua_set_t* call_info;

			xlist_user_t u;
			int bw_call;

			sdp_message_t *sdp_message;
			char* sdp_body = (char*)e->content;

			sdp_message_init(&sdp_message);

			if (sdp_message_parse(sdp_message, sdp_body) != 0)
			{
				clie_log(("\nsipua_event: fail to parse sdp\n"));
				sdp_message_free(sdp_message);
				
				return UA_FAIL;
			}

			rtpcapset = rtp_capable_from_sdp(sdp_message);

			codings = xmalloc(sizeof(rtp_coding_t) * xlist_size(rtpcapset->rtpcaps));

			rtpcap = xlist_first(rtpcapset->rtpcaps, &u);
			while(rtpcap)
			{
				strncpy(codings[ncoding].mime, rtpcap->profile_mime, MAX_MIME_BYTES);
				codings[ncoding].clockrate = rtpcap->clockrate;
				codings[ncoding].param = rtpcap->coding_param;
			
				rtpcap = xlist_next(rtpcapset->rtpcaps, &u);
				ncoding++;
			}

			call_info = ua->sipua.new_conference(&ua->sipua, ua->user_prof, rtpcapset->callid, 
							rtpcapset->subject, rtpcapset->sbytes, 
							rtpcapset->info, rtpcapset->ibytes,
							ua->default_rtp_portno, ua->default_rtcp_portno, 
							ua->total_bandwidth, &bw_call,
							ua->control, codings, ncoding, ua->pt);

			if(!call_info)
			{
				/* Unable receive the call, miss it */
				break;
			}

			ua->total_bandwidth -= bw_call;

			sipua->uas->set_call(sipua->uas, lineno, call_info);

			sdp_message_free(sdp_message);
			xfree(codings);

			break;
		}

		case(SIPUA_EVENT_PROCEEDING):
			break;

		case(SIPUA_EVENT_RINGING):
			break;

		case(SIPUA_EVENT_ANSWERED):
		{
			ua->action->onconnect(ua->action->sip_user, e->from, e->content);
			break;
		}
	}

	return UA_OK;
}

int sipua_done_rtp_media(void* gen)
{
	xrtp_media_t* m = (xrtp_media_t*)gen;

	session_done(m->session);

	return UA_OK;
}

int sipua_done_sip_session(void* gen)
{
	sipua_set_t *set = (sipua_set_t*)gen;
	
	if(set->setid.username) xfree(set->setid.username);
	if(set->setid.nettype) xfree(set->setid.nettype);
	if(set->setid.addrtype) xfree(set->setid.addrtype);
	if(set->setid.netaddr) xfree(set->setid.netaddr);

	xfree(set->setid.id);
	xfree(set->version);
	xfree(set->subject);
	xfree(set->info);

	xlist_done(set->mediaset, sipua_done_rtp_media);

	xfree(set);
	
	return UA_OK;
}

int sipua_done(sipua_t *sipua)
{
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	sipua->uas->shutdown(sipua->uas);

	xfree(ua->action);

	xlist_done(ua->sip_sessions, sipua_done_sip_session);

	xfree(ua);

	/* stop regist thread */
	ua->action->done(ua->action);
   
	return UA_OK;
}

/* NOTE! if match, return ZERO, otherwise -1 */
int sipua_match_conference(void* tar, void* pat)
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

sipua_set_t* client_find_conference(sipua_t* sipua, char* id, char* username, char* nettype, char* addrtype, char* netaddr)
{
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	sipua_setid_t setid;
	xlist_user_t u;

	setid.id = id;
	setid.username = username;
	setid.nettype = nettype;
	setid.addrtype = addrtype;
	setid.netaddr = netaddr;

	return (sipua_set_t*)xlist_find(ua->sip_sessions, &setid, sipua_match_conference, &u);
}

int client_done_conference(sipua_t* sipua, sipua_set_t* set)
{
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	xlist_remove_item(ua->sip_sessions, set);
	/*
	if(set->setid.username) xfree(set->setid.username);
	*/
	if(set->setid.nettype) xfree(set->setid.nettype);
	if(set->setid.addrtype) xfree(set->setid.addrtype);
	if(set->setid.netaddr) xfree(set->setid.netaddr);

	xfree(set->setid.id);
	xfree(set->version);
	xfree(set->subject);
	xfree(set->info);

	xlist_done(set->mediaset, sipua_done_rtp_media);

	xfree(set);
		
	return UA_OK;
}

int client_set_default_profile(sipua_t* sipua, user_profile_t* prof)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	client->user_prof = prof;

	return UA_OK;
}

int client_regist(sipua_t *sipua, user_profile_t *user, char * userloc)
{
	int ret;
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	/* before start, check if already have a register transaction */
	if(client->reg_profile)
	{
		return UA_FAIL;
	}

	if(!client->action)
	{
		client->action = sipua_new_action(client, 
								client_action_onregistration,
								client_action_oncall,
								client_action_onconnect,
								client_action_onreset,
								client_action_onbye);
	}

	ret = sipua_regist(sipua, user, userloc);

	if(ret < UA_OK)
		return ret;
		
	client->reg_profile = user;

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

	ret = sipua_unregist(sipua, user);

	if(ret < UA_OK)
		return ret;
		
	client->reg_profile = user;

	return ret;
}

sipua_set_t* client_create_call(ogmp_client_t* clie, char* subject, int sbytes, char *desc, int dbytes)
{
	ogmp_setting_t *setting;
	sipua_t* sipua = (sipua_t*)clie;
	sipua_set_t* call;

	int bw_conf;

	if(clie->total_bandwidth <= 0)
		return NULL;

	setting = client_setting(clie->control);

	call = sipua_new_conference(sipua, clie->user_prof, NULL,
						subject, sbytes, desc, dbytes,
						clie->default_rtp_portno, clie->default_rtcp_portno, 
						clie->total_bandwidth, &bw_conf, clie->control, 
						setting->codings, setting->ncoding, clie->pt);

	if(!call)
		return NULL;

	clie->total_bandwidth -= bw_conf;
	
	return call;
}

sipua_t* client_new_sipua(sipua_uas_t* uas, int bandwidth)
{
	int nmod;

	sipua_action_t *act=NULL;
	ogmp_client_t *client=NULL;
	module_catalog_t *mod_cata = NULL;

	sipua_t* sipua;

	client = xmalloc(sizeof(ogmp_client_t));
	memset(client, 0, sizeof(ogmp_client_t));

	sipua = (sipua_t*)client;
	client->ui = ogmp_new_ui(sipua);

	client->control = new_media_control();

	client->course_lock = xthr_new_lock();
	client->wait_course_finish = xthr_new_cond(XTHREAD_NONEFLAGS);

	clie_log (("client_new: modules in dir:%s\n", MODDIR));

	/* Initialise */
	client->conf = conf_new ( "ogmprc" );
   
	mod_cata = catalog_new( "mediaformat" );
	catalog_scan_modules ( mod_cata, VERSION, MODDIR );
   
	client->format_handlers = xlist_new();

	nmod = catalog_create_modules (mod_cata, "format", client->format_handlers);
	clie_log (("client_new: %d format module found\n", nmod));

	/* set sip client */
	client->valid = 0;

	/* player controler */
	client->control->config(client->control, client->conf, mod_cata);
	client->control->add_device(client->control, "rtp", client_config_rtp, client->conf);

	//client->lisener = client;
	//client->notify_event = client_sipua_event;
	uas->set_listener(uas, client, client_sipua_event);

	sipua->uas = uas;

	sipua->done = sipua_done;

	sipua->userloc = sipua_userloc;
	sipua->set_default_profile = client_set_default_profile;

	sipua->regist = client_regist;
	sipua->unregist = client_unregist;

	sipua->new_conference = sipua_new_conference;
	sipua->done_conference = client_done_conference;

	sipua->add = sipua_add;
	sipua->remove = sipua_remove;

	sipua->invite = sipua_invite;
	sipua->bye = sipua_bye;
	sipua->reinvite = sipua_reinvite;
   
	clie_log(("client_new: client ready\n\n"));

	return sipua;
}

int client_start(sipua_t* sipua)
{
	ogmp_client_t *clie = (ogmp_client_t*)sipua;

	clie->ui->show(clie->ui);

	return MP_OK;
}

int main(int argc, char** argv)
{
	sipua_uas_t* uas = sipua_uas(5060, "IN", "IP4", NULL, NULL);

	if(uas)
	{
		sipua_t* sipua = client_new_sipua(uas, 64*1024);

		client_start(sipua);
	}

	return 0;
}

