/***************************************************************************
                          speex_player.c - Voice Player
                             -------------------
    begin                : Tue Jan 20 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org; heming@bigfoot.com
 ***************************************************************************/

/***************************************************************************
 *                                                                        *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include "speex_codec.h"

#include <timedia/ui.h>
#include <timedia/xmalloc.h>
#include <string.h>
#include <stdlib.h>

/*
#define SPEEX_PLAYER_LOG
*/
#define SPEEX_PLAYER_DEBUG

#ifdef SPEEX_PLAYER_LOG
 #define spxp_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
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
   char media_type[6];
   char mime_type[13];
   char play_type[6];

} global_const = {"audio", "audio/speex", "local"};

int spxp_set_callback (media_player_t * mp, int type, int(*call)(), void * user)
{
   speex_decoder_t *dec = (speex_decoder_t *)mp;

   switch(type)
   {
      case (CALLBACK_PLAYER_READY):

         dec->callback_on_ready = call;
         dec->callback_on_ready_user = user;
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

int spxp_match_play_type (media_player_t * mp, char *play)
{
   /* FIXME: due to no strncasecmp on win32 mime is case sensitive */
	if (strcmp(play, "playback") == 0)
		return 1;

	return 0;
}

int spxp_loop(void *gen)
{
   rtime_t usec_delay, usec_remain;
   
   speex_decoder_t *dec = (speex_decoder_t*)gen;
	media_player_t *mp = (media_player_t*)dec;

	media_frame_t * auf = NULL;

	media_pipe_t *output = mp->device->pipe(mp->device);
   mp->device->start(mp->device, DEVICE_OUTPUT);
   
	spxp_debug(("\rspxp_loop: start\n"));
   
	dec->stop = 0;
	while(1)
	{
      if(dec->delayed_frame == NULL)

      {         
         xthr_lock(dec->pending_lock);

		   if (dec->stop)
		   {
			   xthr_unlock(dec->pending_lock);
			   break;
		   }     

		   auf = (media_frame_t*)xlist_remove_first(dec->pending_queue);
         if(!auf)
		   {
			   spxp_log (("\rspxp_loop: no packet waiting, sleep!\n"));
			   xthr_cond_wait(dec->packet_pending, dec->pending_lock);
			   spxp_log (("\rspxp_loop: wakeup! %d packets pending\n", xlist_size(dec->pending_queue)));

            auf = (media_frame_t*)xlist_remove_first(dec->pending_queue);
		   }

		   xthr_unlock(dec->pending_lock);
      }
      
      usec_delay = output->put_frame(output, auf, auf->eots);

      if(usec_delay == 0)
         dec->delayed_frame = NULL;
      else
      {
         dec->delayed_frame = auf;
         time_usec_sleep(dec->clock, usec_delay, &usec_remain);
      }
	}

	auf = (media_frame_t*)xlist_remove_first(dec->pending_queue);
	while(auf)
	{
		output->recycle_frame (output, auf);
		auf = (media_frame_t*)xlist_remove_first(dec->pending_queue);
	}
   
	return MP_OK;
}

int spxp_open_stream (media_player_t *mp, media_stream_t *stream, media_info_t *media_info)
{
	speex_decoder_t *dec = NULL;
   
	speex_info_t *spxinfo = (speex_info_t *)media_info;
   
	int ret;
   
	if (!mp->device)
	{
		spxp_log (("spxp_open_stream: No device to play vorbis audio\n"));
		return MP_FAIL;
	}
   
	dec = (speex_decoder_t *)mp;
   
	dec->speex_info = spxinfo;
	if(spxinfo->audioinfo.info.sample_rate < 6000 || spxinfo->audioinfo.info.sample_rate > 48000)
   {
	   spxp_debug(("\rspxp_open_stream: invalid sample rate[%d]\n", spxinfo->audioinfo.info.sample_rate));
      return MP_FAIL;
   }

   spxinfo->audioinfo.info.sample_bits = SPEEX_SAMPLE_BITS;

   spxp_log (("spxp_open_stream: rate[%d]; channels[%d]\n", spxinfo->audioinfo.info.sample_rate, spxinfo->audioinfo.channels));

	ret = mp->device->set_io(mp->device, media_info, NULL);
	if (ret < MP_OK)
	{
		spxp_debug(("spxp_open_stream: speex stream fail to open\n"));
		return ret;
	}

	/* Create mode */
   if(spxinfo->audioinfo.info.sample_rate <= 12500)
		dec->spxmode = speex_lib_get_mode(SPEEX_MODEID_NB);
	else if(spxinfo->audioinfo.info.sample_rate <= 25000)
		dec->spxmode = speex_lib_get_mode(SPEEX_MODEID_WB);
	else if(spxinfo->audioinfo.info.sample_rate <= 48000)
		dec->spxmode = speex_lib_get_mode(SPEEX_MODEID_UWB);

	dec->dst = speex_decoder_init(dec->spxmode);
	if (!dec->dst)
	{
		spxp_debug(("speex_open_header: Decoder initialization failed.\n"));
		return MP_FAIL;
	}
	
	speex_decoder_ctl(dec->dst, SPEEX_SET_SAMPLING_RATE, &(spxinfo->audioinfo.info.sample_rate));
	speex_decoder_ctl(dec->dst, SPEEX_GET_FRAME_SIZE, &(spxinfo->nsample_per_frame));
	speex_decoder_ctl(dec->dst, SPEEX_SET_ENH, &spxinfo->penh);

	/* thread start */
	dec->pending_queue = xlist_new();
	dec->pending_lock = xthr_new_lock();
	dec->packet_pending = xthr_new_cond(XTHREAD_NONEFLAGS);

	/* Initialization of the structure that holds the bits */

	speex_bits_init(&dec->decbits);
   
	spxp_debug(("spxp_open_stream: speex stream opened\n"));

   {  /* repack */
      int chunk_nbyte;
      
      spxinfo->ptime = 100;      
      chunk_nbyte = spxinfo->audioinfo.info.sample_rate / 1000 * spxinfo->ptime * spxinfo->audioinfo.channels * 4;

      dec->chunk = xmalloc(chunk_nbyte);
      memset(dec->chunk, 0, chunk_nbyte);
      
      dec->chunk_p = dec->chunk;
      dec->chunk_nsample = 0;
      dec->chunk_ptime = 0;
   }

   if(dec->callback_on_ready)
      dec->callback_on_ready(dec->callback_on_ready_user, mp, stream);
   
	return ret;
}

int spxp_close_stream (media_player_t * mp)
{
   int loop_ret;

   speex_decoder_t *dec = (speex_decoder_t*)mp;

   dec->stop = 1;
   
   xthr_cond_signal (dec->packet_pending);

   xthr_wait(dec->thread, &loop_ret);

	/*Destroy the decoder state */
	speex_decoder_destroy(dec->dst);

	/*Destroy the bit-packing struct */
	speex_bits_destroy(&dec->decbits);

   xlist_done(dec->pending_queue, NULL);

   xthr_done_lock(dec->pending_lock);
   xthr_done_cond(dec->packet_pending);
   
   xfree(dec->chunk);
   
   return MP_OK;
}

int spxp_start (media_player_t *mp)
{
   speex_decoder_t *dec = (speex_decoder_t*)mp;

   if(!mp->device)
      return MP_FAIL;
      
   if(dec->thread == NULL)
   {
      mp->device->start(mp->device, DEVICE_OUTPUT);
	   dec->thread = xthr_new(spxp_loop, dec, XTHREAD_NONEFLAGS);
   }
   
   return MP_OK;
}

int spxp_stop (media_player_t *mp)
{
   spxp_debug(("spxp_stop: to stop speex player\n"));

   spxp_close_stream(mp);

   mp->device->stop(mp->device, DEVICE_OUTPUT);
   
   ((speex_decoder_t*)mp)->receiving_media = 0;

   return MP_OK;
}

char* spxp_media_type(media_player_t * mp)
{
   return global_const.media_type;
}

char* spxp_play_type(media_player_t * mp)
{
   return global_const.play_type;
}

media_pipe_t * spxp_pipe(media_player_t * p)
{
   if (!p->device)
	   return NULL;

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
    spxp_log(("vorbis_set_options: the option is not supported\n"));
    return MP_EUNSUP;
}

int spxp_done_device(void *gen)
{
   media_device_t* dev = (media_device_t*)gen;

   dev->done(dev);

   return MP_OK;
}

int spxp_set_device(media_player_t* mp, media_control_t* cont, module_catalog_t* cata, void* extra)
{
	/* extra is Not used for audio_out device */
   control_setting_t *setting = NULL;
   module_catalog_t* modules = NULL;
   media_device_t *dev = NULL;
   speex_decoder_t *dec = (speex_decoder_t *)mp;
  
   spxp_log(("spxp_set_device: need audio device\n"));

   dev = cont->find_device(cont, "portaudio", "output");
   
   if(!dev) return MP_FAIL;


   setting = cont->fetch_setting(cont, "audio_out", dev);
   if(!setting)
   {
      spxp_log(("spxp_set_device: use default setting for audio device\n"));
   }
   else
   {
      modules = cont->modules(cont);
      dev->setting(dev, setting, modules);     
   }
   
   dec->clock = cont->clock(cont);
   dec->delayed_frame = NULL;
   
   mp->device = dev;

   return MP_OK;
}

int spxp_link_stream(media_player_t *mp, media_stream_t* strm, media_control_t *cont, void* extra)
{
	/* extra is Not used for audio_out device */
   media_device_t *dev = NULL;
   speex_decoder_t *dec = (speex_decoder_t *)mp;

   spxp_debug(("spxp_link_stream: need audio device\n"));
   dec->clock = cont->clock(cont);

   dev = cont->find_device (cont, "portaudio", "output");

   if(!dev) return MP_FAIL;

   mp->device = dev;
   mp->open_stream(mp, strm, strm->media_info);
   
   strm->player = mp;

   return MP_OK;
}

int spxp_match_type(media_receiver_t *recvr, char *mime, char *fourcc)
{
   /* FIXME: due to no strncasecmp on win32 mime is case sensitive */
	if (mime && strncmp(mime, "audio/speex", 11) == 0)
	{
		spxp_log(("spxp_match_type: mime = 'audio/speex'\n"));
		return 1;
	}
   
	return 0;
}

#if defined (OGMP_NO_CHUNK)

// local test, no decode, no chunk
int spxp_receive_next (media_receiver_t *recvr, media_frame_t *spxf, int64 samplestamp, int last_packet)
{
   media_pipe_t * output = NULL;
   media_frame_t * auf = NULL;
   media_player_t *mp = (media_player_t*)recvr;

   speex_decoder_t *dec = (speex_decoder_t *)mp;
   int sample_rate;

   sample_rate = dec->speex_info->audioinfo.info.sample_rate;

   if (!mp->device)
   {
      spxp_debug(("spxp_receive_next: No device to play speex voice\n"));
      return MP_FAIL;
   }

   output = mp->device->pipe(mp->device);

   {  /* copy */
	   auf = (media_frame_t*)output->new_frame(output, spxf->bytes, NULL);

      auf->bytes = spxf->bytes;
      
      memcpy(auf->raw, spxf->raw, spxf->bytes);

	   auf->nraw = spxf->nraw;
	   auf->usec = 1000000 / dec->speex_info->audioinfo.info.sample_rate * spxf->nraw;  /* microsecond unit */

      auf->samplestamp = spxf->samplestamp;
      auf->sno = spxf->sno;
      auf->eots = spxf->eots;
      auf->eos = spxf->eos;
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

   xthr_lock(dec->pending_lock);
	xlist_addto_last(dec->pending_queue, auf);
	xthr_unlock(dec->pending_lock);

   xthr_cond_signal(dec->packet_pending);

   dec->receiving_media = 1;
   
   return MP_OK;
}

#elif defined (OGMP_RECORD_TEST)

// local test, none decode
int spxp_receive_next (media_receiver_t *recvr, media_frame_t *spxf, int64 samplestamp, int last_packet)
{
   media_pipe_t * output = NULL;
   media_frame_t * auf = NULL;
   media_player_t *mp = (media_player_t*)recvr;

   speex_decoder_t *dec = (speex_decoder_t *)mp;
   int sample_rate;

   sample_rate = dec->speex_info->audioinfo.info.sample_rate;

   if (!mp->device)
   {
      spxp_debug(("spxp_receive_next: No device to play speex voice\n"));
      return MP_FAIL;
   }

   {  /* repack */
      int frame_ms = 1000 * spxf->nraw / dec->speex_info->audioinfo.info.sample_rate;
      memcpy(dec->chunk_p, spxf->raw, spxf->bytes);
      dec->chunk_p += spxf->bytes;
      dec->chunk_ptime += frame_ms;
      dec->chunk_nsample += spxf->nraw;

      if(dec->chunk_ptime < dec->speex_info->ptime)
         return MP_OK;
   }

   output = mp->device->pipe(mp->device);

   {  /* repack */

	   auf = (media_frame_t*)output->new_frame(output, (dec->chunk_p - dec->chunk), NULL);
      auf->bytes = dec->chunk_p - dec->chunk;
      memcpy(auf->raw, dec->chunk, auf->bytes);

	   auf->nraw = spxf->nraw;
	   auf->usec = 1000000 / dec->speex_info->audioinfo.info.sample_rate * spxf->nraw;  /* microsecond unit */

      auf->samplestamp = dec->chunk_samplestamp;
      auf->sno = dec->sno++;
      auf->eots = 1;
      auf->eos = (last_packet == MP_EOS);
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

   xthr_lock(dec->pending_lock);

	xlist_addto_last(dec->pending_queue, auf);

	xthr_unlock(dec->pending_lock);

   xthr_cond_signal(dec->packet_pending);

   dec->receiving_media = 1;

   {  /* for next repack */
      dec->chunk_samplestamp += dec->chunk_nsample;
      dec->chunk_p = dec->chunk;
      dec->chunk_nsample = 0;
      dec->chunk_ptime = 0;
   }

   return MP_OK;
}

#else

int spxp_receive_next (media_receiver_t *recvr, media_frame_t *spxf, int64 samplestamp, int last_packet)
{
   media_pipe_t * output = NULL;
   media_frame_t * auf = NULL;
   media_player_t *mp = (media_player_t*)recvr;

   media_frame_t repack;
   speex_decoder_t *dec = (speex_decoder_t *)mp;
   int sample_rate;

   sample_rate = dec->speex_info->audioinfo.info.sample_rate;

   if (!mp->device)
   {
      spxp_debug(("\rspxp_receive_next: No device to play vorbis audio\n"));
      return MP_FAIL;
   }

   {  /* repack */
      int frame_ms = 1000 * spxf->nraw / dec->speex_info->audioinfo.info.sample_rate;
      memcpy(dec->chunk_p, spxf->raw, spxf->bytes);
      dec->chunk_p += spxf->bytes;
      dec->chunk_ptime += frame_ms;
      dec->chunk_nsample += spxf->nraw;

      if(dec->chunk_ptime < dec->speex_info->ptime)
         return MP_OK;
   }

   output = mp->device->pipe(mp->device);

   {  /* decode repack */
      repack.samplestamp = dec->chunk_samplestamp;
      repack.bytes = dec->chunk_p - dec->chunk;
      repack.nraw = dec->chunk_nsample;
      repack.raw = dec->chunk;
      repack.eos = (last_packet == MP_EOS);
      repack.eots = last_packet;

      /* decode and submit */
      auf = spxc_decode(dec, dec->speex_info, &repack, output);
      if(!auf)
      {
         spxp_log(("spxp_receive_next: No audio samples decoded\n"));
         return MP_FAIL;
      }
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

   auf->samplestamp = dec->chunk_samplestamp;
   auf->sno = dec->sno++;
   auf->eos = (last_packet == MP_EOS);
   auf->eots = last_packet;
   auf->ts = dec->ts_usec_now;

   xthr_lock(dec->pending_lock);
	xlist_addto_last(dec->pending_queue, auf);
	xthr_unlock(dec->pending_lock);

   xthr_cond_signal(dec->packet_pending);

   dec->receiving_media = 1;

   {  /* for next repack */
      dec->chunk_samplestamp += dec->chunk_nsample;
      dec->chunk_p = dec->chunk;
      dec->chunk_nsample = 0;
      dec->chunk_ptime = 0;
   }

   return MP_OK;
}

#endif

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

   mp->media_type = spxp_media_type;
   mp->codec_type = spxp_media_type;

   mp->set_callback = spxp_set_callback;
   mp->pipe = spxp_pipe;
   mp->match_play_type = spxp_match_play_type;
   
   mp->open_stream = spxp_open_stream;
   mp->close_stream = spxp_close_stream;

   mp->set_options = spxp_set_options;

   mp->set_device = spxp_set_device;
   mp->link_stream = spxp_link_stream;
   
   mp->start = spxp_start;
   mp->stop = spxp_stop;
   
   mp->receiver.match_type = spxp_match_type;
   mp->receiver.receive_media = spxp_receive_next;

   return mp;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC module_loadin_t mediaformat =
{
   "playback",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   media_new_player   /* Module initializer */

};
                   

