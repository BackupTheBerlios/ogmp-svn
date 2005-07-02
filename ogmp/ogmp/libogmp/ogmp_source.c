/***************************************************************************
                          ogmp_source.c
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
#include <timedia/ui.h>

#define SOURCE_LOG
#define SOURCE_DEBUG

#ifdef SOURCE_LOG
 #define src_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define src_log(fmtargs)
#endif

#ifdef SOURCE_DEBUG
 #define src_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define src_debug(fmtargs)
#endif

int source_loop(void * gen)
{
   ogmp_source_t * osource = (ogmp_source_t *)gen;

   rtime_t itv;
   
   osource->finish = 0;
   osource->demuxing = 1;

   src_debug (("source_loop: start demuxing ...\n"));

   while (1)
   {
	   {/*lock*/ xthr_lock(osource->lock);}
      
      if (osource->finish)
      {
         src_log (("\nogmplyer: Last demux and quit\n"));
         osource->control->demux_next(osource->control, 1);         
         break;
      }

      itv = osource->control->demux_next(osource->control, 0);
            
      if( itv < 0 && itv == MP_EOF)
      {
         src_log(("(source_loop: stop demux)\n"));
         break;
      }
      
      {/*unlock*/ xthr_unlock(osource->lock);}
   }
   
   osource->demuxing = 0;

   {/*unlock*/ xthr_unlock(osource->lock);}

   src_log (("\n(ogmplyer: source stopped\n"));
   
   return MP_OK;
}

int source_start (media_source_t *msrc)
{
	ogmp_source_t *osource = (ogmp_source_t*)msrc;

	if(osource->demuxing)
	{
		src_log (("source_start: source already running\n"));

		return MP_OK;
	}
		
	/* start thread */
	if(osource->nstream == 0)
	{
		src_debug (("source_start: no source stream available\n"));

		return MP_FAIL;
	}

	src_debug (("source_start: start\n"));

	osource->demuxer = xthr_new(source_loop, osource, XTHREAD_NONEFLAGS);
   
	return MP_OK;
}

int source_stop (media_source_t *msrc)
{
   int ret;
	
   ogmp_source_t *source = (ogmp_source_t*)msrc;
   
   if (!source->demuxing) 
	   return MP_OK;

   src_log (("source_stop: source stops ...\n"));

   {/*lock*/ xthr_lock(source->lock);}

   source->finish = 1;

   {/*unlock*/ xthr_unlock(source->lock);}
   
   xthr_wait(source->demuxer, &ret);

   source->demuxer = NULL;

   //playa->stop (playa);
   
   return MP_OK;
}

int source_done (media_source_t *msrc)
{
	ogmp_source_t *source = (ogmp_source_t*)msrc;
   
	msrc->format->done(msrc->format);

	xthr_done_lock(source->lock);
	xfree(source);

	return MP_OK;
}

int source_done_format_handler(void *gen)
{
   media_format_t * fmt = (media_format_t *)gen;

   fmt->done(fmt);

   return MP_OK;
}

int source_cb_on_player_ready(void *gen, media_player_t *player)
{
	ogmp_source_t *src = (ogmp_source_t*)gen;

	source_start(&src->tsource.source);

	return MP_OK;
}

int source_add_destinate(transmit_source_t *tsrc, char *mime, char *cname, char *nettype, char *addrtype, char *netaddr, int rtp_port, int rtcp_port)
{
	int i;
	ogmp_source_t *src = (ogmp_source_t*)tsrc;

	for(i=0; i<src->nstream; i++)
	{
		if(src->players[i]->match_play_type(src->players[i], "netcast")
			&& src->players[i]->receiver.match_type(&src->players[i]->receiver, mime, NULL))
		{
			media_transmit_t* trans = (media_transmit_t*)src->players[i];
			
			return trans->add_destinate(trans, cname, nettype, addrtype, netaddr, rtp_port, rtcp_port);
		}
	}

	return 0;
}

int source_remove_destinate(transmit_source_t *tsrc, char *mime, char *cname, char *nettype, char *addrtype, char *netaddr, int rtp_port, int rtcp_port)
{
	int i;
	ogmp_source_t *src = (ogmp_source_t*)tsrc;

	for(i=0; i<src->nstream; i++)
	{
		if(src->players[i]->match_play_type(src->players[i], "netcast")
			&& src->players[i]->receiver.match_type(&src->players[i]->receiver, mime, NULL))
		{
			media_transmit_t* trans = (media_transmit_t*)src->players[i];
			
			return trans->remove_destinate(trans, cname, nettype, addrtype, netaddr, rtp_port, rtcp_port);
		}
	}

	return -1;
}

media_source_t* source_open(char* name, media_control_t* control, char* mode, void* mode_param)
{
	media_source_t *msrc;
	transmit_source_t *tsrc;
	ogmp_source_t* osource;

	xlist_t* format_handlers;
	media_format_t *format = NULL;
   
	xlist_user_t $lu;

	int num, i, nplayer;
   
	module_catalog_t * mod_cata = NULL;

   int support = 0;
    
   if(0 == strcmp(mode, "playback"))
      support = 1;
   if(0 == strcmp(mode, "netcast"))
      support = 1;
        
   if(!support)
   {
      src_debug(("source_open: mode not support!\n"));
      return NULL;
   }

   osource = xmalloc(sizeof(ogmp_source_t));
	if(!osource)
   {
      src_debug(("source_open: no memory!\n"));
	   return NULL;
   }

	memset(osource, 0, sizeof(ogmp_source_t));

	/* player controler */
	osource->control = control;

	mod_cata = control->modules(control);
   
	format_handlers = xlist_new();
	num = catalog_create_modules (mod_cata, "format", format_handlers);
	src_log (("source_open: %d format module found\n", num));

	/* create a media format, also get the capabilities of the file */
	format = (media_format_t *) xlist_first (format_handlers, &$lu);
	while (format)
	{
		osource->nstream = format->open(format, name, osource->control);
		if (osource->nstream > 0)
		{
			src_log (("source_open: %d streams\n", osource->nstream));

			osource->tsource.source.format = format;

			xlist_remove_item(format_handlers, format);
         
			break;
		}
      
		format = (media_format_t *)xlist_next(format_handlers, &$lu);
	}

	xlist_done(format_handlers, source_done_format_handler);

	if (osource->nstream == 0)
	{
		src_log(("source_setup: no format can open '%s'\n", name));

		xfree(osource);

		return NULL;
	}

	/* set audio source */
	osource->control->set_format (osource->control, "av", format);

	msrc = (media_source_t*)osource;

	msrc->done = source_done;
	msrc->start = source_start;
	msrc->stop = source_stop;

   if(0 == strcmp(mode, "playback"))
	{
		if(0 == format->new_all_player(format, osource->control, "playback", mode_param))
		{
			source_done(msrc);
			return NULL;
		}
	}
    else if(0 == strcmp(mode, "netcast"))
	{
		netcast_parameter_t *np = (netcast_parameter_t*)mode_param;
		
		rtpcap_set_t* rtpcapset = rtp_capable_from_format(format, np->subject, np->info, np->user_profile);

		if(0 == format->new_all_player(format, osource->control, "netcast", rtpcapset))
		{
			source_done(msrc);
			return NULL;
		}

      /* In "netcast" mode */
      tsrc = (transmit_source_t*)osource;
      tsrc->add_destinate = source_add_destinate;
      tsrc->remove_destinate = source_remove_destinate;
      
      /* collect the players of the file */
      nplayer = format->players(format, "netcast", osource->players, MAX_NCAP);

      src_log(("source_setup: '%s' opened by %d players\n", name, nplayer));

      for(i=0; i<nplayer; i++)
      {
         src_log(("source_setup: source_cb_on_player_ready@%x\n", (int)source_cb_on_player_ready));
         osource->players[i]->set_callback(osource->players[i], CALLBACK_PLAYER_READY, source_cb_on_player_ready, osource);
      }
   }
    
   osource->lock = xthr_new_lock();
   osource->wait_request = xthr_new_cond(XTHREAD_NONEFLAGS);
   
   /*test
   osource->control->demux_next(osource->control, 0);
   */
   
   return msrc;
}

/**
 * Find sessions in the format which match cname and mimetype in source
 * Move member_of_session(call->rtp_format) to member_of_session(source->players);
 */
int source_associate(media_source_t* msrc, media_format_t* rtp_fmt, char* cname)
{
   int i;
   media_player_t* player = NULL;
   ogmp_source_t *osrc = (ogmp_source_t*)msrc;

   if(!rtp_fmt->support_type(rtp_fmt, "mime", "application/sdp"))
   {
      src_debug(("source_associate: Not a rtp format\n"));
      return XRTP_EUNSUP;
   }
   
   for(i=0; i<osrc->nstream; i++)
   {
      media_stream_t *strm = rtp_fmt->first;
      
      player = osrc->players[i];
      
      while(strm)
      {
         rtp_stream_t* rtp_strm = (rtp_stream_t*)strm;
         
         if(player->match_play_type(player, "netcast") && player->receiver.match_type(&player->receiver, strm->mime, NULL))
         {
            media_transmit_t *transmit = (media_transmit_t*)player;
            
            if(cname)
            {
               session_move_member_by_cname(rtp_strm->session, transmit->session(transmit), cname);  
            }
            else
            {
               session_move_all_guests(rtp_strm->session, transmit->session(transmit));
            }
         }
         
         strm = strm->next;
      }
   }
   
   return 0;
}

/**
 * Find sessions in the format which match cname and mimetype in source
 * Move_member_from_session_of(call->rtp_format) to_session_of(source->players);
 */
int source_associate_guests(media_source_t* msrc, media_format_t* rtp_fmt)
{
   int i;
   
   media_player_t* player = NULL;
   ogmp_source_t *osrc = (ogmp_source_t*)msrc;

   if(!rtp_fmt->support_type(rtp_fmt, "mime", "application/sdp"))
   {
      src_debug(("source_associate: Not a rtp format, pause\n"));           
      return XRTP_EUNSUP;
   }

   for(i=0; i<osrc->nstream; i++)
   {
      media_stream_t *strm;

      player = osrc->players[i];

      strm = rtp_fmt->first;
      while(strm)
      {
         rtp_stream_t* rtp_strm = (rtp_stream_t*)strm;

         if(player->match_play_type(player, "netcast") && player->receiver.match_type(&player->receiver, strm->mime, NULL))
         {
            media_transmit_t *transmit;
            xrtp_session_t *tr_ses;

            transmit = (media_transmit_t*)player;
            tr_ses = transmit->session(transmit);
            
            session_move_all_guests(rtp_strm->session, tr_ses);
         }

         strm = strm->next;
      }
   }

   return 0;
}
