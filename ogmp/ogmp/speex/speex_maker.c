/***************************************************************************
                   speex_maker.c - Generate voice from audio_in device
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
/*
#define SPXMK_LOG
*/
#define SPXMK_DEBUG

#ifdef SPXMK_LOG
 #define spxmk_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define spxmk_log(fmtargs)
#endif

#ifdef SPXMK_DEBUG
 #define spxmk_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define spxmk_debug(fmtargs)
#endif

#define MAX_FRAME_BYTES 2000

struct global_const_s
{
   const char media_type[6];
   const char mime_type[13];
   const char play_type[6];

} global_const = {"audio", "audio/speex", "local"};

int spxmk_match_type(media_receiver_t * mr, char *mime, char *fourcc)
{
   /* FIXME: due to no strncasecmp on win32 mime is case sensitive */
	if (mime && strncmp(mime, "audio/speex", 11) == 0)
	{
		spxmk_log(("speex_match_type: mime = 'audio/speex'\n"));
		return 1;
	}
   
	return 0;
}

#if defined (OGMP_RECORD_TEST) && defined (OGMP_NO_CHUNK)

int spxmk_receive_media (media_receiver_t *mr, media_frame_t *auf, int64 samplestamp, int last_packet)
{
	speex_encoder_t *enc = (speex_encoder_t *)mr;
   media_stream_t *stream = enc->media_stream;

	return stream->player->receiver.receive_media(&stream->player->receiver, auf, samplestamp, last_packet);
}

#elif defined (OGMP_RECORD_TEST)

// test: none encode
int spxmk_receive_media (media_receiver_t *mr, media_frame_t *auf, int64 samplestamp, int last_packet)
{
	speex_encoder_t *enc = (speex_encoder_t *)mr;

	int delta_nsample = 0;
   int cache_nsample = 0;

   int todo_nbyte = auf->bytes;
   int done_nbyte = 0;
   int delta_nbyte = 0;

	media_frame_t spxf;
	media_stream_t *stream = enc->media_stream;

   char *cut = auf->raw;

   /* finish last remain */
   if(enc->cache_nbyte > 0)
   {
      cache_nsample = enc->cache_nbyte / SPEEX_SAMPLE_BYTES;

      delta_nsample = enc->frame_nsample - cache_nsample;
      delta_nbyte = enc->frame_nbyte - enc->cache_nbyte;

      memcpy(&enc->encoding_frame[enc->cache_nbyte], auf->raw, delta_nbyte);

      done_nbyte = delta_nbyte;
      todo_nbyte -= delta_nbyte;

      samplestamp -= cache_nsample;

	   /* make speex media frame */
      spxf.sno = enc->sno++;
      spxf.samplestamp = samplestamp;
	   spxf.raw = enc->encoding_frame;
	   spxf.bytes = enc->frame_nbyte;
	   spxf.nraw = enc->frame_nsample;
	   spxf.eots = 1;

	   stream->player->receiver.receive_media(&stream->player->receiver, &spxf, (samplestamp - cache_nsample), last_packet);

	   /* for next group */
	   samplestamp = samplestamp + enc->frame_nbyte;;

      cut += delta_nbyte;
      enc->cache_nbyte = 0;
   }

   while(todo_nbyte >= enc->frame_nbyte)
   {
	   /* make speex media frame */
	   spxf.raw = cut;
	   spxf.bytes = enc->frame_nbyte;
	   spxf.nraw = enc->frame_nsample;

      if(todo_nbyte == enc->frame_nbyte)
	      spxf.eots = 1;
      else
	      spxf.eots = 0;

	   /* submit */
	   stream->player->receiver.receive_media(&stream->player->receiver, &spxf, samplestamp, last_packet);

      done_nbyte += enc->frame_nbyte;
      todo_nbyte -= enc->frame_nbyte;
      cut += enc->frame_nbyte;

      samplestamp += enc->frame_nsample;
   }

   /* samples left for next */
   if(todo_nbyte > 0)
      memcpy(enc->encoding_frame, &auf->raw[done_nbyte], todo_nbyte);

   enc->cache_nbyte = todo_nbyte;

	return MP_OK;
}

#else

int spxmk_receive_media (media_receiver_t *mr, media_frame_t *auf, int64 samplestamp, int last_packet)
{
	speex_encoder_t *enc = (speex_encoder_t *)mr;

	int delta_nsample = 0;
   int cache_nsample = 0;

   int todo_nbyte = auf->bytes;
   int done_nbyte = 0;
   int delta_nbyte = 0;

	media_frame_t spxf;
	media_stream_t *stream = enc->media_stream;

   int spx_nbyte;

   char *cut = auf->raw;
   
   /* finish last remain */
   if(enc->cache_nbyte > 0)
   {
      cache_nsample = enc->cache_nbyte / SPEEX_SAMPLE_BYTES;

      delta_nsample = enc->frame_nsample - cache_nsample;
      delta_nbyte = enc->frame_nbyte - enc->cache_nbyte;

      memcpy(&enc->encoding_frame[enc->cache_nbyte], auf->raw, delta_nbyte);
	   spx_nbyte = spxc_encode(enc, enc->speex_info, enc->encoding_frame, enc->frame_nbyte, enc->spxcode, enc->frame_nbyte);

      done_nbyte = delta_nbyte;
      todo_nbyte -= delta_nbyte;

      samplestamp -= cache_nsample;
      
	   /* make speex media frame */      
      spxf.sno = enc->sno++;
      spxf.samplestamp = samplestamp;
	   spxf.raw = enc->spxcode;
	   spxf.bytes = spx_nbyte;
	   spxf.nraw = enc->frame_nsample;
	   spxf.eots = 1;

	   stream->player->receiver.receive_media(&stream->player->receiver, &spxf, samplestamp, last_packet);

	   /* for next group */
	   samplestamp = samplestamp + enc->frame_nsample;

      cut += delta_nbyte;
      enc->cache_nbyte = 0;
   }

   while(todo_nbyte >= enc->frame_nbyte)
   {
	   spx_nbyte = spxc_encode(enc, enc->speex_info, cut, enc->frame_nbyte, enc->spxcode, enc->frame_nbyte);

      /* make speex media frame */
      spxf.sno++;
      spxf.samplestamp = samplestamp;
	   spxf.raw = enc->spxcode;
	   spxf.bytes = spx_nbyte;
	   spxf.nraw = enc->frame_nsample;
	   spxf.eots = 1;

	   /* submit */
	   stream->player->receiver.receive_media(&stream->player->receiver, &spxf, samplestamp, last_packet);

      done_nbyte += enc->frame_nbyte;
      todo_nbyte -= enc->frame_nbyte;
      cut += enc->frame_nbyte;;

      samplestamp += enc->frame_nsample;
   }

   /* samples left for next */
   if(todo_nbyte > 0)
      memcpy(enc->encoding_frame, &auf->raw[done_nbyte], todo_nbyte);

   enc->cache_nbyte = todo_nbyte;

	return MP_OK;
}

#endif

int spxmk_done(media_maker_t *maker)
{
	speex_encoder_t *enc = (speex_encoder_t *)maker;

	/*Destroy the encoder state */
	speex_encoder_destroy(enc->est);

	/*Destroy the bit-packing struct */
	speex_bits_destroy(&enc->encbits);

	xfree(enc->encoding_frame);
	xfree(enc->spxcode);
   
	xfree(maker);

	return MP_OK;
}                     

int spxmk_start(media_maker_t *maker)
{
	speex_encoder_t *enc = (speex_encoder_t *)maker;

   enc->input_device->start(enc->input_device, DEVICE_INPUT);

   spxmk_debug(("\rspxmk_start: speex maker started\n"));
   
   return MP_OK;
}

int spxmk_stop(media_maker_t *maker)
{
	speex_encoder_t *enc = (speex_encoder_t *)maker;

   enc->input_device->stop(enc->input_device, DEVICE_INPUT);

   return MP_OK;
}

int speex_start_stream (media_stream_t* stream)
{
   if(!stream->maker)
      return MP_FAIL;
      
   spxmk_start (stream->maker);
   
   return MP_OK;
}

int speex_stop_stream (media_stream_t* stream)
{
   if(!stream->maker)

      return MP_FAIL;

   spxmk_stop (stream->maker);

   return MP_OK;
}
   
int spxmk_link_stream(media_maker_t* maker, media_stream_t* stream, media_control_t* control, media_info_t* minfo)
{
	int ret;
	int rate;
	int nchannel;
   control_setting_t *setting = NULL;

	speex_encoder_t *enc = (speex_encoder_t *)maker;

	speex_info_t* spxinfo= (speex_info_t*)minfo;

   media_device_t* dev = control->find_device (control, "portaudio", "input");
   if(!dev)
      return MP_FAIL;

   setting = control->fetch_setting(control, "audio", dev);
   if(!setting)
   {
      spxmk_log(("source_open_device: use default setting for '%s' device\n", mediatype));
   }
   else
   {
	   module_catalog_t * mod_cata = control->modules(control);
      dev->setting(dev, setting, mod_cata);
   }

	/* Create mode */
	if(spxinfo->audioinfo.info.sample_rate < 6000)
		return MP_FAIL;
	else if(spxinfo->audioinfo.info.sample_rate <= 12500)
		enc->spxmode = speex_lib_get_mode(SPEEX_MODEID_NB);
	else if(spxinfo->audioinfo.info.sample_rate <= 25000)
		enc->spxmode = speex_lib_get_mode(SPEEX_MODEID_WB);
	else if(spxinfo->audioinfo.info.sample_rate <= 48000)
		enc->spxmode = speex_lib_get_mode(SPEEX_MODEID_UWB);
	else
		return MP_FAIL;

	spxinfo->audioinfo.info.sample_bits = SPEEX_SAMPLE_BITS;
   spxinfo->nframe_per_packet = 1;

	ret = dev->set_io(dev, minfo, &maker->receiver);
	if (ret < MP_OK)
	{
		spxmk_log (("spxmk_new_media_stream: speex stream fail to open\n"));
		return ret;
	}

   stream->maker = maker;

   enc->media_stream = stream;
   enc->speex_info = spxinfo;
   enc->clock = control->clock(control);
   enc->est = speex_encoder_init(enc->spxmode);
   
   speex_encoder_ctl(enc->est, SPEEX_SET_SAMPLING_RATE, &spxinfo->audioinfo.info.sample_rate);

   /* Set the quality to 8 (15 kbps) 
	 * spxinfo->quality=8;
	if(spxinfo->vbr && spxinfo->vbr_quality >= 0.0 && spxinfo->vbr_quality <= 10.0)
	{     
		speex_encoder_ctl(spxinfo->est, SPEEX_SET_VBR, &spxinfo->vbr);
		speex_encoder_ctl(spxinfo->est, SPEEX_SET_QUALITY, &spxinfo->vbr_quality);
	}
	else
	{
		if(spxinfo->cbr)
			speex_encoder_ctl(spxinfo->est, SPEEX_SET_BITRATE, &spxinfo->cbr);
		else if(spxinfo->cbr_quality >= 0 && spxinfo->cbr_quality <= 10)
			speex_encoder_ctl(spxinfo->est, SPEEX_SET_QUALITY, &spxinfo->cbr_quality);
	}
 
	if(spxinfo->complexity)
		speex_encoder_ctl(spxinfo->est, SPEEX_SET_COMPLEXITY, &spxinfo->complexity);
   */
   {
      int bitrate = 8000;
		speex_encoder_ctl(enc->est, SPEEX_SET_BITRATE, &bitrate);
   }
   
	/* Initialization of the structure that holds the bits */
	speex_bits_init(&enc->encbits);

   rate = enc->speex_info->audioinfo.info.sample_rate;
	enc->frame_nsample = enc->speex_info->nsample_per_frame;
   
	nchannel = spxinfo->audioinfo.channels;
	enc->frame_nbyte = nchannel * enc->frame_nsample * sizeof(int16);
   
	enc->lookahead = 0;
   
	/*speex_encoder_ctl(spxinfo->est, SPEEX_GET_LOOKAHEAD, &enc->lookahead);*/
   
	if (enc->speex_info->denoise || enc->speex_info->agc)
	{
		enc->encpreprocess = speex_preprocess_state_init(enc->frame_nsample, rate);
      
	   if (enc->speex_info->denoise)
		   speex_preprocess_ctl(enc->encpreprocess, SPEEX_PREPROCESS_SET_DENOISE, &enc->speex_info->denoise);
	   if (enc->speex_info->agc)
		   speex_preprocess_ctl(enc->encpreprocess, SPEEX_PREPROCESS_SET_AGC, &enc->speex_info->agc);

      enc->lookahead += enc->frame_nsample;
	}

	enc->encoding_frame = xmalloc(enc->frame_nbyte);
	enc->spxcode = xmalloc(enc->frame_nbyte);
   
   enc->input_device = dev;   

	return MP_OK;
}

module_interface_t* media_new_maker()
{
   speex_encoder_t *enc = xmalloc(sizeof(struct speex_encoder_s));
   if(!enc)
   {
      spxmk_debug(("vorbis_new_player: No memory to allocate\n"));
      return NULL;
   }

   memset(enc, 0, sizeof(struct speex_encoder_s));

   enc->maker.done = spxmk_done;
   enc->maker.link_stream = spxmk_link_stream;

   enc->maker.receiver.match_type = spxmk_match_type;

   enc->maker.start = spxmk_start;
   enc->maker.stop = spxmk_stop;
   
   enc->maker.receiver.receive_media = spxmk_receive_media;
   
   return enc;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC module_loadin_t mediaformat =
{
   "create",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   media_new_maker   /* Module initializer */
};
                   
