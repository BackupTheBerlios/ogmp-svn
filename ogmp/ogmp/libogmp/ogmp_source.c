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
   ogmp_source_t * source = (ogmp_source_t *)gen;

   xrtp_hrtime_t itv;
   
   source->finish = 0;
	
   source->demuxing = 1;

   while (1)
   {
      {/*lock*/ xthr_lock(source->lock);}
      
      if (source->finish)
      {
         src_log (("\nogmplyer: Last demux and quit\n"));
         source->control->demux_next(source->control, 1);
         
         break;
      }

      itv = source->control->demux_next(source->control, 0);
      if( itv < 0 && itv == MP_EOF)
      {
         src_log(("(source_loop: stop demux)\n"));
         break;
      }
      
      {/*unlock*/ xthr_unlock(source->lock);}
   }
   
   source->demuxer = NULL;

   {/*unlock*/ xthr_unlock(source->lock);}

   src_log (("\n(ogmplyer: source stopped\n"));
   
   return MP_OK;
}

int source_start (media_source_t *msrc)
{
	ogmp_source_t *source = (ogmp_source_t*)msrc;

	if(source->demuxer)
		return MP_OK;
		
	/* start thread */
	if(source->nstream == 0)
		return MP_FAIL;
		
	source->demuxer = xthr_new(source_loop, source, XTHREAD_NONEFLAGS);
   
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

	src_log (("source_cb_on_player_ready: player ready\n"));

	source_start(&src->source);

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

media_source_t* source_open(char* name, media_control_t* control, char *mode)
{
	media_source_t *msrc;
	transmit_source_t *tsrc;
	ogmp_source_t* source;

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
    
	source = xmalloc(sizeof(ogmp_source_t));
	if(!source)
    {
        src_debug(("source_open: no memory!\n"));
		return NULL;
    }

	memset(source, 0, sizeof(ogmp_source_t));

	/* player controler */
	source->control = control;
   
	mod_cata = control->modules(control);
   
	format_handlers = xlist_new();
	num = catalog_create_modules (mod_cata, "format", format_handlers);
	src_log (("source_open: %d format module found\n", num));

	/* create a media format, also get the capabilities of the file */
	format = (media_format_t *) xlist_first (format_handlers, &$lu);
	while (format)
	{
		/* open media source
		 * mode: "playback"; "netcast"
		 */
		source->nstream = format->open(format, name, source->control);
		if (source->nstream > 0)
		{
			src_log (("source_open: %d streams\n", source->nstream));

			source->source.format = format;

			xlist_remove_item(format_handlers, format);
			break;
		}
      
		format = (media_format_t *)xlist_next(format_handlers, &$lu);
	}

	xlist_done(format_handlers, source_done_format_handler);

	if (source->nstream == 0)
	{
		src_log(("source_setup: no format can open '%s'\n", name));

		xfree(source);

		return NULL;
	}

	/* set audio source */
	source->control->set_format (source->control, "av", format);

	msrc = (media_source_t*)source;

	msrc->done = source_done;
	msrc->start = source_start;
	msrc->stop = source_stop;

	if(0 == format->new_all_player(format, source->control, mode, NULL))
	{
        source_done(msrc);

        return NULL;
	}

    if(0 == strcmp(mode, "netcast"))
	{
        /* In "netcast" mode */
        tsrc = (transmit_source_t*)source;
    
        tsrc->add_destinate = source_add_destinate;
        tsrc->remove_destinate = source_remove_destinate;

        /* collect the players of the file */
        nplayer = format->players(format, "rtp", source->players, MAX_NCAP);
        src_log(("source_setup: '%s' opened by %d players\n", name, nplayer));

        for(i=0; i<nplayer; i++)
            source->players[i]->set_callback(source->players[i], CALLBACK_PLAYER_READY, source_cb_on_player_ready, source);
    }
    
    source->lock = xthr_new_lock();
    source->wait_request = xthr_new_cond(XTHREAD_NONEFLAGS);

    return msrc;
}
