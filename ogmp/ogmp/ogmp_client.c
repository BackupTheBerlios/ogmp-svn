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
 #define clie_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define clie_log(fmtargs)
#endif

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

int client_execute_command( ogmp_client_t* clie, ogmp_command_t* cmd)
{
	switch(cmd->type)
	{
		case(COMMAND_TYPE_REGISTER):
		{
			sipua_action_t *act;
			
			clie_log(("client_execute_command: to register\n"));

			if(clie->registered)
			{
				clie_log(("client_execute_command: already registered\n"));
				break;
			}

			act = sipua_new_action (clie,
									client_action_onregistration, 
									client_action_oncall, 
									client_action_onconnect, 
									client_action_onreset,
									client_action_onbye);

			clie->sipua->regist(clie->sipua, act);
			
			break;
		}
		case(COMMAND_TYPE_UNREGISTER):
		{
			clie_log(("client_execute_command: to unregister\n"));

			if(!clie->registered)
			{
				clie_log(("client_execute_command: already unregistered\n"));
				break;
			}

			clie->sipua->unregist(clie->sipua);
			
			break;
		}
	}

	return MP_OK;
}

int client_main_thread(void* gen)
{
	ogmp_client_t *clie = (ogmp_client_t*)gen;

	clie_log(("client_main_thread: client running...\n"));

	if(!clie->ui)
		return MP_FAIL;

	while(1)
	{
		ogmp_command_t* command = clie->ui->wait_command(clie->ui);

		if(command->type == COMMAND_TYPE_EXIT)
		{
			command->type = COMMAND_TYPE_UNREGISTER;

			client_execute_command(clie, command);

			break;
		}

		client_execute_command(clie, command);
	}

	return MP_OK;
}

ogmp_client_t* client_new(sipua_uas_t* uas, ogmp_ui_t* ui, user_profile_t* user, int bw)
{
	sipua_action_t *act=NULL;
	ogmp_client_t *client=NULL;
	module_catalog_t *mod_cata = NULL;

	int nmod;

	client = xmalloc(sizeof(ogmp_client_t));
	memset(client, 0, sizeof(ogmp_client_t));

	client->control = new_media_control();

	client->sipua = sipua_new(uas, user, bw, client->control);

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
	client->ui = ui;
   
	/* player controler */
	client->control->config(client->control, client->conf, mod_cata);
	client->control->add_device(client->control, "rtp", client_config_rtp, client->conf);
   
	if(client->ui)
	{
		client->main_thread = xthr_new(client_main_thread, client, XTHREAD_NONEFLAGS);
		if(!client->main_thread)
		{
			client->control->done(client->control);
			client->sipua->done(client->sipua);
			xfree(client);
			return NULL;
		}
	}

	clie_log(("client_new: client ready\n\n"));

	return client;
}

int client_call(ogmp_client_t *clie, char *to_regname)
{
	ogmp_setting_t *setting;

	setting = client_setting(clie->control);

	xthr_lock(clie->course_lock);

	clie->call = clie->sipua->new_conference(clie->sipua, NULL,
						SESSION_SUBJECT, strlen(SESSION_SUBJECT)+1, 
						SESSION_DESCRIPT, strlen(SESSION_DESCRIPT)+1, 
						setting->codings, setting->ncoding);

	clie->sipua->invite(clie->sipua, clie->call, to_regname);

	xthr_cond_wait(clie->wait_course_finish, clie->course_lock);

	xthr_unlock(clie->course_lock);

	return MP_OK;
}

int main(int argc, char** argv)
{
	char * playmode = argv[1];
   
	/* define a player */
	xrtp_clock_t * clock_main = NULL;
	xrtp_lrtime_t remain = 0;

	sipua_uas_t *uas_clie;
	ogmp_client_t* client;
	ogmp_ui_t* ui;

	/* Initialise client */
	user_profile_t prof_clie = {"ogmpc@timedia", "ogmpc", "ogmp client", 12, "ogmpc@timedia", "54321", 3600, "myname.net", "voip.timedia.org"};

	/* sip useragent */
	clie_log (("\nmain: modules in dir:%s\n", MODDIR));

	uas_clie = sipua_uas(5070, "IN", "IP4", NULL, "127.0.0.1");

	ui = ogmp_new_ui();

	client = client_new(uas_clie, ui, &prof_clie, 64*1024);

	ui->init(client->sipua);
	ui->show(ui);
	/*
	cmd.type = COMMAND_TYPE_REGISTER;
	cmd.instruction = NULL;
	client_command(client, &cmd);
	*/
	return 0;
}

