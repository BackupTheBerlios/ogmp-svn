/***************************************************************************
                          ogmp_server.c
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

#include "ogmp.h"
#include "rtp_cap.h"

#include <timedia/xstring.h>
#include <timedia/xmalloc.h>

#define SERV_LOG

#ifdef SERV_LOG
 #define serv_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define serv_log(fmtargs)
#endif

int server_loop(void * gen)
{
   ogmp_server_t * server = (ogmp_server_t *)gen;

   xrtp_hrtime_t itv;
   
   server->finish = 0;
   while (1)
   {
      {/*lock*/ xthr_lock(server->lock);}
      
      if (!server->demuxing)
      {
         serv_log (("\nserver_loop: waiting for demux request\n"));
         xthr_cond_wait(server->wait_request, server->lock);
         serv_log (("\nserver_loop: start demux...\n\n"));
      }

      if (server->finish)
      {
         serv_log (("\nogmplyer: Last demux and quit\n"));
         server->control->demux_next(server->control, 1);
         
         break;
      }

      itv = server->control->demux_next(server->control, 0);
      if( itv < 0 && itv == MP_EOF)
      {
         serv_log(("(server_loop: stop demux)\n"));
         break;
      }
      
      server->demuxing = 1;
      {/*unlock*/ xthr_unlock(server->lock);}
   }
   
   {/*unlock*/ xthr_unlock(server->lock);}

   server->demuxing = 0;
   serv_log (("\n(ogmplyer: server stopped\n"));
   
   return MP_OK;
}

int server_start (ogmp_server_t *server)
{
   if (!server->valid)
   {
      serv_log (("server_loop: server is not available\n"));
      return MP_FAIL;
   }
   
   if (!server->demuxing)
   {
	  serv_log (("server_start: server starts ...\n"));
      server->demuxing = 1;
      xthr_cond_signal(server->wait_request);
   }
   
   return MP_OK;
}

int server_stop (ogmp_server_t *server)
{
   int ret;
   
   serv_log (("server_stop: server stops ...\n"));

   if (!server->valid) return MP_OK;
   if (!server->demuxing) return MP_OK;

   {/*lock*/ xthr_lock(server->lock);}

   server->demuxing = 0;
   server->finish = 1;

   {/*unlock*/ xthr_unlock(server->lock);}
   
   xthr_wait(server->demuxer, &ret);

   //playa->stop (playa);
   
   return MP_OK;
}

int server_done_format_handler(void *gen)
{
   media_format_t * fmt = (media_format_t *)gen;

   fmt->done(fmt);

   return MP_OK;
}

int server_cb_on_player_ready(void *gen, media_player_t *player)
{
	ogmp_server_t *serv = (ogmp_server_t*)gen;

	serv_log (("server_cb_on_player_ready: player ready\n"));

	server_start(serv);

	return MP_OK;
}

/**
 * return capability number, if error, return ZERO
 */
int server_setup(ogmp_server_t *server, char *mode, void* extra)
{
	/* define a player */
	media_format_t *format = NULL;
   
	config_t * conf;

	xlist_user_t $lu;

	int format_ok = 0;

	int num, i;
   
	module_catalog_t * mod_cata = NULL;

	ogmp_setting_t *setting;

	serv_log (("svr_setup: play '%s', with modules in dir:%s\n", server->fname, MODDIR));
   
	/* Initialise */
	conf = conf_new ( "ogmprc" );
   
	mod_cata = catalog_new( "mediaformat" );
	num = catalog_scan_modules ( mod_cata, VERSION, MODDIR );
   
	server->format_handlers = xlist_new();

	num = catalog_create_modules (mod_cata, "format", server->format_handlers);
	serv_log (("svr_setup: %d format module found\n", num));

	/* set audio server */
	server->valid = 0;
   
	server->lock = xthr_new_lock();
	server->wait_request = xthr_new_cond(XTHREAD_NONEFLAGS);

	/* player controler */
	server->control = (media_control_t*)new_media_control();
   
	server->control->config(server->control, conf, mod_cata);

	/* For some player handles multi-stream, device can be pre-opened. */
	if(strcmp(mode, "netcast") == 0)
		server->control->add_device(server->control, "rtp", server_config_rtp, conf);
   
	/* create a media format, also get the capabilities of the file */
	format = (media_format_t *) xlist_first (server->format_handlers, &$lu);
	while (format)
	{
		/* open media source */
		if (format->open(format, server->fname, server->control, conf, mode, extra) >= MP_OK)
		{
			format_ok = 1;
			break;
		}
      
		format = (media_format_t *)xlist_next(server->format_handlers, &$lu);
	}

	if (!format_ok)
	{
		serv_log(("server_setup: no format can open '%s'\n", server->fname));

		xthr_done_cond(server->wait_request);
		xthr_done_lock(server->lock);

		return 0;
	}

	server->control->set_format (server->control, "av", format);

	/* collect the caps of the players of the file */
	server->nplayer = format->players(format, "rtp", server->players, MAX_NCAP);
	serv_log (("server_setup: '%s' opened by %d players\n", server->fname, server->nplayer));

	setting = server_setting(server->control);

	for(i=0; i<server->nplayer; i++)
	{
		xrtp_media_t *rtp_media;
		
		rtp_media = (xrtp_media_t*)server->players[i]->media(server->players[i]);

		server->sipua->add(server->sipua, server->call, rtp_media);

		server->players[i]->set_callback(server->players[i], CALLBACK_PLAYER_READY, server_cb_on_player_ready, server);
	}

	server->format = format;
	server->valid = 1;

	/* start thread */
	if(server->nplayer > 0)
		server->demuxer = xthr_new(server_loop, server, XTHREAD_NONEFLAGS);
   
	return server->nplayer;
}

/****************************************************************************************
 * SIP Callbacks
 */

int server_action_onregister(void *user, char *from, int result, void* info)
{
	ogmp_server_t *serv = (ogmp_server_t*)user;
	
	serv->registered = 0;

	switch(result)
	{
		case(SIPUA_EVENT_REGISTRATION_SUCCEEDED):
		{
			serv->registered = 1;

			serv_log(("\nserver_action_onregister: registered to [%s] !\n\n", from));
			break;
		}
		case(SIPUA_EVENT_UNREGISTRATION_SUCCEEDED):
		{
			serv_log(("\nserver_action_onregister: unregistered to [%s] !\n\n", from));

			xthr_cond_broadcast(serv->wait_unregistered);

			break;
		}
		case(SIPUA_EVENT_REGISTRATION_FAILURE):
		{
			serv_log(("\nserver_action_onregister: fail to register to [%s]!\n\n", from));
			break;
		}
		case(SIPUA_EVENT_UNREGISTRATION_FAILURE):
		{
			serv_log(("\nserver_action_onregister: fail to unregister to [%s]!\n\n", from));
			break;
		}
	}

	return MP_OK;
}

int server_action_oncall(void *user, char *from, void* info_in, void* info_out)
{
	char* sdp_body_in = (char*)info_in;
	char** sdp_out = (char**)info_out;

	ogmp_server_t *server = (ogmp_server_t*)user;

	rtpcap_set_t* rtpcapset;

	serv_log(("\nserver_action_oncall: call from [%s]\n\n", from));

	if(sdp_body_in)
	{
		sdp_message_t *sdp_message_in;
		
		serv_log (("server_action_onconnect: received sdp\n"));
		serv_log (("---------------------------------\n"));
		serv_log (("%s", sdp_body_in));
		serv_log (("---------------------------------\n"));

		sdp_message_init(&sdp_message_in);

		if (sdp_message_parse(sdp_message_in, sdp_body_in) != 0)
		{
			serv_log(("\nserver_action_onconnect: fail to parse sdp\n"));
			sdp_message_free(sdp_message_in);
			return 0;
		}
	
		/* Retrieve capable from the sdp message */
		rtpcapset = rtp_capable_from_sdp(sdp_message_in);

		rtpcapset->user_profile = server->user_profile;

		serv_log(("\nserver_action_onconnect: %d capabilities found\n\n", xlist_size(rtpcapset->rtpcaps)));

		sdp_message_free(sdp_message_in);
	}

	/* parameter setting stage should be here, eg: rtsp conversation ... */

 	/* generate new empty conference */
	server->call = server->sipua->new_conference(server->sipua, rtpcapset->callid, 
								rtpcapset->subject, rtpcapset->sbytes, 
								rtpcapset->info, rtpcapset->ibytes, 
								NULL, 0);

	/* rtp session establish */
	server_setup(server, "netcast", rtpcapset);
	
	*sdp_out = server->sdp_body;

	serv_log(("\nserver_action_oncall: cn[%s] support %d capables\n\n", SEND_REGNAME, server->nplayer));

	return server->nplayer;
}

int server_action_onconnect(void *user, char *from, void* info_in)
{
	ogmp_server_t *server = (ogmp_server_t*)user;

	serv_log(("\nserver_action_onconnect: [%s] connected\n", from));

	server_start(server);

	return 0;
}

/* change call status */
int server_action_onreset(void *user, char *from, void* info_in)
{
	ogmp_server_t *server = (ogmp_server_t*)user;

	serv_log(("server_action_onreset: recall from cn[%s]\n", from));

	server_stop(server);

	return 0;
}

int server_action_onbye(void *user, char *from)
{
	int i, ndest;
	media_transmit_t *mt;
	ogmp_server_t *server = (ogmp_server_t*)user;

	for(i=0; i<server->nplayer; i++)
	{
		mt = (media_transmit_t*)(server->players[i]);
		ndest = mt->delete_destinate(mt, from);
	}

	if(ndest == 0)
		server_stop(server);

	return UA_OK;
}
/****************************************************************************************/

int server_done(ogmp_server_t *server)
{
	server->format->close(server->format);
   
	xlist_done(server->format_handlers, server_done_format_handler);
   
	xthr_done_cond(server->wait_request);
	xthr_done_lock(server->lock);

	xthr_done_cond(server->wait_unregistered);

	if(server->sdp_body)
		free(server->sdp_body);

	xfree(server);

	return MP_OK;
}

int server_command(ogmp_server_t* serv, ogmp_command_t* cmd)
{
	xthr_lock(serv->command_lock);

	serv->command = cmd;
		
	xthr_unlock(serv->command_lock);

	xthr_cond_signal(serv->wait_command);

	return MP_OK;
}

int server_execute_command( ogmp_server_t* serv, ogmp_command_t* cmd)
{
	switch(cmd->type)
	{
		case(COMMAND_TYPE_REGISTER):
		{
			sipua_action_t *act;
			
			if(serv->registered)
			{
				serv_log(("server_execute_command: already registered\n"));
				break;
			}

			act = sipua_new_action (serv,
									server_action_onregister, 
									server_action_oncall, 
									server_action_onconnect, 
									server_action_onreset,
									server_action_onbye);

			serv->sipua->regist(serv->sipua, act);
			
			break;
		}
		case(COMMAND_TYPE_UNREGISTER):
		{
			if(!serv->registered)
			{
				serv_log(("server_execute_command: already unregistered\n"));
				break;
			}

			serv->sipua->unregist(serv->sipua);
			
			break;
		}
	}

	return MP_OK;
}

int server_main_thread(void* gen)
{
	ogmp_server_t *serv = (ogmp_server_t*)gen;

	while(1)
	{
		ogmp_command_t* command;

		xthr_lock(serv->command_lock);

		if(serv->command == NULL)
			xthr_cond_wait(serv->wait_command, serv->command_lock);

		command = serv->command;

		if(command->type == COMMAND_TYPE_EXIT)
		{
			command->type = COMMAND_TYPE_UNREGISTER;

			server_execute_command(serv, command);
			xthr_cond_wait(serv->wait_unregistered, serv->command_lock);

			xthr_unlock(serv->command_lock);
			break;
		}

		server_execute_command(serv, command);

		serv->command = NULL;

		xthr_unlock(serv->command_lock);
	}

	return MP_OK;
}

ogmp_server_t* server_new(sipua_uas_t* uas, user_profile_t* user, int bw)
{
	ogmp_server_t *server;
		
	server = xmalloc(sizeof(ogmp_server_t));

	memset(server, 0, sizeof(ogmp_server_t));

	server->fname = FNAME;

	server->user_profile = user;

	server->control = new_media_control();
	if(!server->control)
	{
		xfree(server);
		return NULL;
	}

	server->sipua = sipua_new(uas, user, bw, server->control);
	if(!server->sipua)
	{
		server->control->done(server->control);
		xfree(server);
		return NULL;
	}

	server->command_lock = xthr_new_lock();
	server->wait_command = xthr_new_cond(XTHREAD_NONEFLAGS);
	server->wait_unregistered = xthr_new_cond(XTHREAD_NONEFLAGS);

	server->main_thread = xthr_new(server_main_thread, server, XTHREAD_NONEFLAGS);
	if(!server->sipua)
	{
		server->control->done(server->control);
		server->sipua->done(server->sipua);
		xfree(server);
		return NULL;
	}

	serv_log(("server_new: server ready\n\n"));
	return server;
}

