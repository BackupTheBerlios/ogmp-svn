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

#include <timedia/xstring.h>

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
         serv_log (("\nogmplyer: waiting for demux request\n"));
         xthr_cond_wait(server->wait_request, server->lock);
         server->demuxing = 1;
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
      
      {/*unlock*/ xthr_unlock(server->lock);}
   }
   
   server->demuxing = 0;
   {/*unlock*/ xthr_unlock(server->lock);}

   serv_log (("\n(ogmplyer: server stopped\n"));
   
   return MP_OK;
}

int server_start (ogmp_server_t *server)
{
   serv_log (("server_start: server starts ...\n"));

   if (!server->valid)
   {
      serv_log (("server_loop: server is not available\n"));
      return MP_FAIL;
   }
   
   if (!server->demuxing)
   {
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
int server_setup(ogmp_server_t *server, char *mode)
{
   /* define a player */
   //media_player_t *playa = NULL;
   media_format_t *format = NULL;
   
   config_t * conf;

   xlist_user_t $lu;

   int format_ok = 0;

   int num, i;
   
   module_catalog_t * mod_cata = NULL;

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
   format = (media_format_t *) xrtp_list_first (server->format_handlers, &$lu);
   while (format)
   {
      /* open media source */
      if (format->open(format, server->fname, server->control, conf, mode) >= MP_OK)
	  {
         format_ok = 1;
         
         break;
      }
      
      format = (media_format_t *) xrtp_list_next (server->format_handlers, &$lu);
   }

   /* open media source */
   if (!format_ok)
   {
      serv_log (("ogmplyer: no format can open '%s'\n", server->fname));

      xthr_done_cond(server->wait_request);
      xthr_done_lock(server->lock);

      return 0;
   }

   server->control->set_format (server->control, "av", format);

   /* collect the caps of the file on "rtp" netcast device */
   server->nplayer = format->players(format, "rtp", server->players, MAX_NCAP);
   for(i=0; i<server->nplayer; i++)
   {
	   server->caps[i] = server->players[i]->capable(server->players[i]);
	   server->players[i]->set_callback(server->players[i], CALLBACK_PLAYER_READY, server_cb_on_player_ready, server);
   }

   server->format = format;
   server->valid = 1;

   /* start thread */
   server->demuxer = xthr_new(server_loop, server, XTHREAD_NONEFLAGS);
   
   return server->nplayer;
}

/****************************************************************************************
 * SIP Callbacks
 */

int callback_server_oncall(void *user, char *from_cn, int from_cnlen, capable_descript_t* from_caps[], int from_ncap, capable_descript_t* **selected_caps)
{
	int i,j,c = 0;

	ogmp_server_t *server = (ogmp_server_t*)user;

	serv_log(("\ncallback_server_oncall: from cn[%s]\n\n", from_cn));

	/* parameter setting stage should be here, eg: rtsp conversation */

	/* rtp session establish */
	server_setup(server, "netcast");

	if(from_ncap == 0)
	{
		/* return all capables to answer query, negociate on client side */
		*selected_caps = server->caps;

		c =  server->nplayer;
	}
	else
	{
		/* intersect of both capables */
		for(i=0; i<from_ncap; i++)
		{
			/* negociate on server side */
			for(j=0; j<server->nplayer; j++)
			{
				if(server->caps[j]->match(server->caps[j], from_caps[i]))
					server->selected_caps[c++] = server->caps[j];
			}
		}

		*selected_caps = server->selected_caps;
	}

	serv_log(("\ncallback_server_oncall: cn[%s] support %d capables\n\n", SEND_CNAME, c));

	return c;
}

int callback_server_onconnect(void *user, char *from_cn, int from_cnlen, capable_descript_t* from_caps[], int from_ncap, capable_descript_t* **estab_caps)
{
	ogmp_server_t *server = (ogmp_server_t*)user;

	serv_log(("\ncallback_server_onconnect: with cn[%s] requires %d capables\n\n", from_cn, from_ncap));

	if(from_ncap>0)
	{
		int i,j;
		media_transmit_t *mt;

		/* link by capabilities */
      
		for(i=0; i<server->nplayer; i++)
		{
			mt = (media_transmit_t*)server->players[i];
			for(j=0; j<from_ncap; j++)

			{
				if(mt->player.match_capable((media_player_t*)mt, from_caps[j]))
				{
					rtpcap_descript_t *rtpcap = (rtpcap_descript_t *)from_caps[j];
					mt->add_destinate(mt, from_cn, from_cnlen, rtpcap->ipaddr, rtpcap->rtp_portno, rtpcap->rtcp_portno);
				}
			}
		}

		*estab_caps = from_caps;

		return from_ncap;
	}

	return 0;
}

/* change call status */
int callback_server_onreset(void *user, char *from_cn, int from_cnlen, capable_descript_t* caps[], int ncap, capable_descript_t* **estab_caps)
{
	ogmp_server_t *server = (ogmp_server_t*)user;

	serv_log(("cb_server_recall: recall from cn[%s]\n", from_cn));

	if(ncap == 0)
	{
		server_stop(server);
	}

	return ncap;
}

int callback_server_onbye(void *user, char *cname, int cnlen)
{
	int i, ndest;
	media_transmit_t *mt;
	ogmp_server_t *server = (ogmp_server_t*)user;

	for(i=0; i<server->nplayer; i++)
	{
		mt = (media_transmit_t*)(server->players[i]);
		ndest = mt->delete_destinate(mt, cname, cnlen);
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

   free(server);

   return MP_OK;
}

ogmp_server_t* server_new(sipua_t *sipua, char *fname)
{
	sipua_record_t *rec;

	ogmp_server_t *server = malloc(sizeof(ogmp_server_t));
	memset(server, 0, sizeof(ogmp_server_t));

	server->fname = FNAME;

	server->sipua = sipua;

	rec = sipua_new_record( server, callback_server_oncall, 
							server, callback_server_onconnect, 
							server, callback_server_onreset,
							server, callback_server_onbye
						  );

	sipua->regist(sipua, SEND_CNAME, strlen(SEND_CNAME), rec);

	serv_log(("server_new: server sipua ready\n"));
	return server;
}

