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
#include <timedia/xmalloc.h>
#include <timedia/ui.h>
#include <stdarg.h>

#define OGMP_VERSION  1

extern ogmp_ui_t* global_ui;

#define CLIE_LOG

#ifdef CLIE_LOG
 #define clie_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define clie_log(fmtargs)
#endif

/****************************************************************************************/

int client_done_format_handler(void *gen)
{
   media_format_t * fmt = (media_format_t *)gen;

   fmt->done(fmt);

   return MP_OK;
}

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

				client->user_prof = client->reg_profile;

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

            client->ogui->ui.beep(&client->ogui->ui);

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

			/* now to negotiate call, which will generate call structure with sdp message for reply
			 * still no rtp session created !
			 */
			call = sipua_negotiate_call(sipua, client->user_prof, rtpcapset,
							client->mediatypes, client->default_rtp_ports, 
							client->default_rtcp_ports, client->nmedia,
							client->control);
			
			if(!call)
			{
				/* Fail to receive the call, miss it */
				break;
			}

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
					client->lines[i] = call;
					sipua->uas->accept(sipua->uas, lineno);

					client->ogui->ui.beep(&client->ogui->ui);
                    sipua_answer(&client->sipua, call, SIPUA_STATUS_RINGING);

					break;
				}
			}

			sdp_message_free(sdp_message);

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

			sipua_set_t *call = e->call_info; 
			call->status = SIPUA_EVENT_ANSWERED;

			printf("client_sipua_event: SIPUA_EVENT_ANSWERED\n");

			sdp_message_init(&sdp_message);


			if (sdp_message_parse(sdp_message, sdp_body) != 0)
			{
				clie_log(("\nsipua_event: fail to parse sdp\n"));
				sdp_message_free(sdp_message);
				
				break;
			}

			rtpcapset = rtp_capable_from_sdp(sdp_message);

			/* rtp sessions created */
			sipua_establish_call(sipua, call, "playback", rtpcapset, client->format_handlers, client->control, client->pt);

			rtp_capable_done_set(rtpcapset);
			sdp_message_free(sdp_message);

			break;
		}
		case(SIPUA_EVENT_ACK):
		{
			/* Callee establishs call after its answer is acked
			sipua_set_t *call = e->call_info; */

			/* rtp sessions created */
			sipua_establish_call(sipua, e->call_info, "playback", e->call_info->rtpcapset, client->format_handlers, client->control, client->pt);

			rtp_capable_done_set(e->call_info->rtpcapset);
			e->call_info->rtpcapset = NULL;

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

	xfree(set);
	
	return UA_OK;
}

int client_done_call(sipua_t* sipua, sipua_set_t* set)
{
	int i;
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	if(ua->call != set)
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
		ua->call = NULL;
	}

	ua->control->release_bandwidth(ua->control, set->bandwidth);


	return sipua_done_sip_session(set);
}

int sipua_done(sipua_t *sipua)
{
	int i;
	ogmp_client_t *ua = (ogmp_client_t*)sipua;

	sipua->uas->shutdown(sipua->uas);

	for(i=0; i<MAX_SIPUA_LINES; i++)
		if(ua->lines[i])
			sipua_done_sip_session(ua->lines[i]);

	if(ua->call)
		sipua_done_sip_session(ua->call);

	xthr_done_lock(ua->lines_lock);




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
	int i;

	ogmp_setting_t *setting;
	ogmp_client_t* clie = (ogmp_client_t*)sipua;
	sipua_set_t* call;
	int bw_budget = clie->control->book_bandwidth(clie->control, 0);

	setting = client_setting(clie->control);

	call = sipua_new_call(sipua, clie->user_prof, NULL, subject, sbytes, desc, dbytes,
						clie->mediatypes, clie->default_rtp_ports, clie->default_rtcp_ports, clie->nmedia,
						clie->control, bw_budget, setting->codings, setting->ncoding, clie->pt);
	if(!call)
	{
		clie_log(("client_new_call: no call created\n"));
		return NULL;
	}

	for(i=0; i<MAX_SIPUA_LINES; i++)
	{
		if(!clie->lines[i])
		{
			clie->lines[i] = call;
			break;
		}
	}

	clie_log(("client_new_call: new call on line %d\n", i));
	
	return call;
}

int client_set_profile(sipua_t* sipua, user_profile_t* prof)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	client->user_prof = prof;

	return UA_OK;
}

user_profile_t* client_profile(sipua_t* sipua)

{
	return ((ogmp_client_t*)sipua)->user_prof;
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

	for(i=0; i<MAX_SIPUA_LINES; i++)
		if(client->lines[i])
			busylines[n++] = i;

	return n;
}

/* current call session */
sipua_set_t* client_session(sipua_t* sipua)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	return client->call;
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

media_source_t* client_set_background_source(sipua_t* sipua, char* name)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;
	
	if(client->backgroud_source)
	{
		client->backgroud_source->stop(client->backgroud_source);
		client->backgroud_source->done(client->backgroud_source);
		client->backgroud_source = NULL;
	}

	if(name && name[0])
	{
        /*
        client->backgroud_source = source_open(name, client->control, "netcast", );
        */
	} 


	return client->backgroud_source;
}

/* call media attachment */
int client_attach_source(sipua_t* sipua, sipua_set_t* call, transmit_source_t* tsrc)
{
	xlist_user_t lu;
	char *cname = call->rtpcapset->cname;

	rtpcap_descript_t *rtpcap = xlist_first(call->rtpcapset->rtpcaps, &lu);
	while(rtpcap)
	{
		if(rtpcap->netaddr)
			tsrc->add_destinate(tsrc, rtpcap->profile_mime, cname, 
								rtpcap->nettype, rtpcap->addrtype, rtpcap->netaddr, 
								rtpcap->rtp_portno, rtpcap->rtcp_portno);
		else
			tsrc->add_destinate(tsrc, rtpcap->profile_mime, cname, 
								call->rtpcapset->nettype, 
								call->rtpcapset->addrtype, 
								call->rtpcapset->netaddr, 
								rtpcap->rtp_portno, rtpcap->rtcp_portno);
	
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

	if(client->call)
		return NULL;

	client->call = client->lines[line];
	client->lines[line] = NULL;

	/* FIXME: Howto transfer call from waiting session */

	return client->call;
}

int client_hold(sipua_t* sipua, sipua_set_t* call)
{
	int i;
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	if(!client->call)
		return -1;

	for(i=0; i<MAX_SIPUA_LINES; i++)
		if(!client->lines[i])
			break;

	client->lines[i] = call;

	/* FIXME: Howto transfer call to waiting session */

	return i;
}
	
int client_answer(sipua_t *sipua, sipua_set_t* call, int reply)
{
	ogmp_client_t *client = (ogmp_client_t*)sipua;

	switch(reply)
	{
		case SIPUA_STATUS_ANSWER:
		{
			/* establish rtp sessions */
			int bw;

			/* Now create rtp sessions of the call */
			bw = sipua_establish_call(sipua, call, "playback", call->rtpcapset, 
							client->format_handlers, client->control, client->pt);

			if(bw < 0)
			{
				sipua_answer(&client->sipua, call, SIPUA_STATUS_REJECT);

				return UA_FAIL;
			}

			/* Anser the call */
			sipua_answer(&client->sipua, call, reply);

			if(client->backgroud_source)
				client_attach_source(sipua, call, (transmit_source_t*)client->backgroud_source);

			xfree(call->reply_body);
			call->reply_body = NULL;
			
			break;
		}

		default:
        {
			/* Anser the call */
			sipua_answer(&client->sipua, call, reply);
        }
	}

	return UA_OK;
}

sipua_t* client_new(char *uitype, sipua_uas_t* uas, module_catalog_t* mod_cata, int bandwidth)
{
	int nmod;
	int nformat;

	ogmp_client_t *client=NULL;

	sipua_t* sipua;

	client = xmalloc(sizeof(ogmp_client_t));
	memset(client, 0, sizeof(ogmp_client_t));

	client->ogui = (ogmp_ui_t*)client_new_ui(mod_cata, uitype);
    if(client->ogui == NULL)
    {
        clie_log (("client_new: No cursesui module found!\n"));
        xfree(client);

        return NULL;
    }
    
	sipua = (sipua_t*)client;

    client->ogui->set_sipua(client->ogui, sipua);

	client->control = new_media_control();

	client->course_lock = xthr_new_lock();
	client->wait_course_finish = xthr_new_cond(XTHREAD_NONEFLAGS);

	/* Initialise */
	client->conf = conf_new ( "ogmprc" );
   
	client->format_handlers = xlist_new();

	nmod = catalog_create_modules (mod_cata, "format", client->format_handlers);
	clie_log (("client_new: %d format module found\n", nmod));

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

	sipua->open_source = client_open_source;
	sipua->close_source = client_close_source;
	sipua->attach_source = client_attach_source;
	sipua->detach_source = client_detach_source;

	/* switch current call session */
 	sipua->pick = client_pick;
 	sipua->hold = client_hold;
	/*
	sipua->add = sipua_add;
	sipua->remove = sipua_remove;
	*/
	sipua->call = sipua_call;
	sipua->answer = client_answer;

	sipua->options_call = sipua_options_call;
	sipua->info_call = sipua_info_call;

	sipua->bye = sipua_bye;
	sipua->recall = sipua_recall;
   
	clie_log(("client_new: client ready\n\n"));

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

int main(int argc, char** argv)
{
    sipua_t* sipua = NULL;
	sipua_uas_t* uas = NULL;
	module_catalog_t *mod_cata = NULL;

	int sip_port;

	if(argc < 2)
	{
		sip_port = 5060;
	}
	else
	{
		sip_port = strtol(argv[1], NULL, 10);
	}

	clie_log (("main: sip port is %d\n", sip_port));

	clie_log (("main: modules in dir:'%s'\n", MOD_DIR));

	mod_cata = catalog_new( "mediaformat" );
    
	catalog_scan_modules ( mod_cata, OGMP_VERSION, MOD_DIR );
    
	uas = client_new_uas(mod_cata, "eXosipua");
    if(!uas)
        clie_log (("main: fail to create sipua server!\n"));
    
	if(uas && uas->init(uas, sip_port, "IN", "IP4", NULL, NULL) >= UA_OK)
	{
		sipua = client_new("cursesui", uas, mod_cata, 64*1024);

        if(sipua)
            client_start(sipua);
        else
            clie_log (("main: fail to create sipua!\n"));
	}

	return 0;
}

