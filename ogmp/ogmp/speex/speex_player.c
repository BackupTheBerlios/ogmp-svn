/***************************************************************************
                          speex_player.c - Voice Player
                             -------------------
    begin                : Tue Jan 20 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org; heming@bigfoot.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include "speex_codec.h"

#include <timedia/xmalloc.h>
#include <string.h>
#include <stdlib.h>

#define SPEEX_PLAYER_LOG
#define SPEEX_PLAYER_DEBUG

#ifdef SPEEX_PLAYER_LOG
 #define spxp_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define spxp_log(fmtargs)
#endif

#ifdef SPEEX_PLAYER_DEBUG
 #define spxp_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define spxp_debug(fmtargs)
#endif

struct global_const_s
{
   const char media_type[6];
   const char mime_type[13];
   const char play_type[6];

} global_const = {"audio", "audio/speex", "local"};

int spxp_set_callback (media_player_t * mp, int type, int(*call)(), void * user)
{
   speex_decoder_t *dec = (speex_decoder_t *)mp;

   switch(type)
   {
      case (CALLBACK_PLAYER_READY):

         dec->callback_on_ready = call;
         dec->callback_on_ready = user;
         spxp_log(("spxp_set_callback: 'on_ready' callback added\n"));
         break;

      case (CALLBACK_MEDIA_STOP):
      
         dec->callback_on_media_stop = call;
         dec->callback_on_media_stop_user = user;
         spxp_log(("spxp_set_callback: 'on_media_stop' callback added\n"));
         break;
         
      default:

         spxp_log(("vorbis_set_callback: callback[%d] unsupported\n", type));
         return MP_EUNSUP;
   }

   return MP_OK;
}

int spxp_match_type (media_player_t * mp, char *mime, char *fourcc)
{
    /* FIXME: due to no strncasecmp on win32 mime is case sensitive */
	if (mime && strncmp(mime, "audio/speex", 11) == 0)
	{
      spxp_log(("vorbis_match_type: mime = 'audio/speex'\n"));
      return 1;
	}
   
	return 0;
}

int spxp_loop(void *gen)
{
	speex_decoder_t *dec = (speex_decoder_t*)gen;
	media_player_t *mp = (media_player_t*)dec;

	media_frame_t * auf = NULL;

    media_pipe_t *output = mp->device->pipe(mp->device);
	dec->stop = 0;
	while(1)
	{
		xthr_lock(dec->pending_lock);

		if (dec->stop)
		{			
			xthr_unlock(dec->pending_lock);
			break;
		}

		if (xlist_size(dec->pending_queue) == 0) 
		{
			//vsend_log(("vsend_loop: no packet waiting, sleep!\n"));
			xthr_cond_wait(dec->packet_pending, dec->pending_lock);
			//vsend_log(("vsend_loop: wakeup! %d packets pending\n", xlist_size(vs->pending_queue)));
		}

		/* sometime it's still empty, weired? */
		if (xlist_size(dec->pending_queue) == 0)
		{
			xthr_unlock(dec->pending_lock);
			continue;
		}

		auf = (media_frame_t*)xlist_remove_first(dec->pending_queue);

		xthr_unlock(dec->pending_lock);

		output->put_frame (output, auf, auf->eots);
	}

	auf = (media_frame_t*)xlist_remove_first(dec->pending_queue);

	while(auf)
		output->recycle_frame (output, auf);

	return MP_OK;
}


int spxp_open_stream (media_player_t *mp, media_info_t *media_info)
{
   speex_decoder_t *dec = NULL;
   
   speex_info_t *spxinfo = (speex_info_t *)media_info;
   
   struct audio_info_s ai;
   
   int ret;
   
   if (!mp->device)
   {
      spxp_log (("vorbis_open_stream: No device to play vorbis audio\n"));
      return MP_FAIL;
   }
   
   dec = (speex_decoder_t *)mp;
   
   dec->speex_info = spxinfo;
   
   ai.info.sample_rate = spxinfo->audioinfo.info.sample_rate;
   ai.info.sample_bits = SPEEX_SAMPLE_BITS;
   ai.channels = spxinfo->audioinfo.channels;
   
   ret = mp->device->set_media_info(mp->device, (media_info_t*)&ai);
   
   if (ret < MP_OK)
   {
      spxp_log (("vorbis_open_stream: vorbis stream fail to open\n"));
      return ret;
   }

   /* thread start */
   dec->pending_queue = xlist_new();
   dec->pending_lock = xthr_new_lock();
   dec->packet_pending = xthr_new_cond(XTHREAD_NONEFLAGS);
   dec->thread = xthr_new(spxp_loop, dec, XTHREAD_NONEFLAGS);
   
   return ret;
}

int spxp_close_stream (media_player_t * mp)
{
   int loop_ret;

   speex_decoder_t *dec = (speex_decoder_t*)mp;

   xthr_lock(dec->pending_lock);
   dec->stop = 1;
   xthr_unlock(dec->pending_lock);
   
   xthr_cond_signal(dec->packet_pending);

   xthr_wait(dec->thread, &loop_ret);

   xlist_done(dec->pending_queue, NULL);
   xthr_done_lock(dec->pending_lock);
   xthr_done_cond(dec->packet_pending);
   
   return MP_OK;
}

int spxp_stop (media_player_t *mp)
{
   spxp_debug(("spxp_stop: to stop speex player\n"));

   spxp_close_stream(mp);

   mp->device->stop(mp->device);

   ((speex_decoder_t*)mp)->receiving_media = 0;

   return MP_OK;
}

int spxp_receive_next (media_player_t *mp, void *spx_packet, int64 samplestamp, int last_packet)
{
   media_pipe_t * output = NULL;
   media_frame_t * auf = NULL;
   
   speex_decoder_t *dec = (speex_decoder_t *)mp;
   int sample_rate = ((media_info_t*)dec->speex_info)->sample_rate;

   if (!mp->device)
   {
      spxp_debug(("spxp_receive_next: No device to play vorbis audio\n"));
      return MP_FAIL;
   }

   output = mp->device->pipe(mp->device);

   /* decode and submit */
   auf = spxc_decode(dec->speex_info, (ogg_packet*)spx_packet, output);
   if(!auf)
   {
      spxp_log(("spxp_receive_next: No audio samples decoded\n"));
      return MP_FAIL;
   }

   if (!dec->receiving_media)
   {
      dec->ts_usec_now = 0;
      dec->last_samplestamp = samplestamp;
   }

   if (dec->last_samplestamp != samplestamp)
   {
      dec->ts_usec_now += (int)((samplestamp - dec->last_samplestamp) / (double)sample_rate * 1000000);
      dec->last_samplestamp = samplestamp;
   }
   
   auf->ts = dec->ts_usec_now;

   if(last_packet)
	   auf->eots = 1;
   else
	   auf->eots = 0;
   
   xthr_lock(dec->pending_lock);
   xlist_addto_last(dec->pending_queue, auf);
   xthr_unlock(dec->pending_lock);
   
   xthr_cond_signal(dec->packet_pending);

   dec->receiving_media = 1;

   return MP_OK;
}

const char* spxp_media_type(media_player_t * mp)
{
   return global_const.media_type;
}

const char* spxp_play_type(media_player_t * mp)
{
   return global_const.play_type;
}

media_pipe_t * spxp_pipe(media_player_t * p)
{
   if (!p->device) return NULL;

   return p->device->pipe(p->device);
}

int spxp_done(media_player_t *mp)
{
   if(mp->device)
   {
	   spxp_stop(mp);

	   mp->device->done (mp->device);
   }

   xfree(mp);

   return MP_OK;
}

int spxp_set_options (media_player_t * mp, char *opt, void *value)
{
    if(!value)
	{
        spxp_debug(("vorbis_set_options: param 'value' is NULL point\n"));
        return MP_FAIL;
    }

	{
        spxp_log(("vorbis_set_options: the option is not supported\n"));
        return MP_EUNSUP;
    }
    
    return MP_OK;
}

int spxp_done_device(void *gen)
{
   media_device_t* dev = (media_device_t*)gen;

   dev->done(dev);

   return MP_OK;
}

int spxp_set_device(media_player_t * mp, media_control_t *cont, module_catalog_t *cata)
{
   control_setting_t *setting = NULL;

   media_device_t *dev = NULL;
   
   spxp_log(("spxp_set_device: need audio device\n"));

   dev = cont->find_device(cont, "audio_out");
   
   if(!dev) return MP_FAIL;

   setting = cont->fetch_setting(cont, "audio_out", dev);
   if(!setting)
   {
      spxp_log(("spxp_set_device: use default setting for audio device\n"));
   }
   else
   {
      dev->setting(dev, setting, cata);     
   }
   
   mp->device = dev;

   return MP_OK;
}

module_interface_t * media_new_player()
{
   media_player_t *mp = NULL;

   speex_decoder_t *dec = xmalloc(sizeof(struct speex_decoder_s));
   if(!dec)
   {
      spxp_debug(("vorbis_new_player: No memory to allocate\n"));
      return NULL;
   }

   memset(dec, 0, sizeof(struct speex_decoder_s));

   mp = (media_player_t *)dec;

   mp->done = spxp_done;

   mp->play_type = spxp_play_type;
   mp->media_type = spxp_media_type;
   mp->codec_type = spxp_media_type;

   mp->set_callback = spxp_set_callback;
   mp->pipe = spxp_pipe;
   
   mp->match_type = spxp_match_type;
   
   mp->open_stream = spxp_open_stream;
   mp->close_stream = spxp_close_stream;

   mp->receive_media = spxp_receive_next;
   mp->set_options = spxp_set_options;

   mp->set_device = spxp_set_device;
   
   mp->stop = spxp_stop;

   return mp;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC module_loadin_t mediaformat = {

   "playback",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   media_new_player   /* Module initializer */
};
                   
