/***************************************************************************
                   speex_creater.c - Generate voice from audio_in device
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

#define SPXR_LOG
#define SPXR_DEBUG

#ifdef SPXR_LOG
 #define spxr_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define spxr_log(fmtargs)
#endif

#ifdef SPXR_DEBUG
 #define spxr_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define spxr_debug(fmtargs)
#endif

#define MAX_FRAME_BYTES 2000

struct global_const_s
{
   const char media_type[6];
   const char mime_type[13];
   const char play_type[6];

} global_const = {"audio", "audio/speex", "local"};

int spxr_match_type (media_receiver_t * mr, char *mime, char *fourcc)
{
    /* FIXME: due to no strncasecmp on win32 mime is case sensitive */
	if (mime && strncmp(mime, "audio/speex", 11) == 0)
	{
		spxr_log(("speex_match_type: mime = 'audio/speex'\n"));
		return 1;
	}
   
	return 0;
}

int spxr_receive_media (media_receiver_t *mr, void *mf, int64 samplestamp, int last_packet)
{
	int spx_bytes;
	char *spx_frame;

	media_frame_t *auf = (media_frame_t*)mf;
	speex_encoder_t *enc = (speex_encoder_t *)mr;
	int sample_rate = ((media_info_t*)enc->speex_info)->sample_rate;

	int delta = enc->encoding_nsample_expect - enc->encoding_nsample_buffered;
	int sample_bytes = auf->bytes/auf->nraw;

	char *over_samples;
	int over_bytes;

	media_frame_t spxf;
	media_stream_t *stream = enc->media_stream;

	/* assemble all input pcm frame to 20ms encoding frame */
	if(delta > auf->nraw)
	{
		memcpy(enc->encoding_gap, auf->raw, auf->bytes);
		enc->encoding_gap += auf->bytes;
		enc->encoding_nsample_buffered += auf->nraw;

		if(last_packet != MP_EOS)
			return MP_OK;
		else
		{
			int gap_bytes = delta * sample_bytes;
			memset(enc->encoding_gap, 0, auf->bytes - gap_bytes);
		}
	}
	else
	{
		/* 20ms buffered */
		int gap_bytes = delta * sample_bytes;
		memcpy(enc->encoding_gap, auf->raw, gap_bytes);

		if(delta == auf->nraw)
		{
			over_samples = NULL;
			over_bytes = 0;
			enc->encoding_gap = enc->encoding_frame;
		}
		else
		{
			over_samples = auf->raw + gap_bytes;
			over_bytes = auf->bytes - gap_bytes;
		}
	}

	/* encode and submit */
	spx_bytes = spxc_encode(enc, enc->speex_info, enc->encoding_frame, enc->encoding_frame_bytes, &spx_frame);
	/* samples left for next */
	if(over_bytes)
	{
		memcpy(enc->encoding_frame, over_samples, over_bytes);
		enc->encoding_nsample_buffered = auf->nraw - delta;
		enc->encoding_gap = enc->encoding_frame + over_bytes;
	}

	if(spx_frame == NULL)
		return MP_OK;

	/* make speex media frame */
	spxf.raw = spx_frame;
	spxf.bytes = spx_bytes;
	spxf.nraw = enc->encoding_nsample_expect;
	spxf.eots = 1;

	stream->player->receive_media(stream->player, &spxf, enc->group_samplestamp, last_packet);
   
	/* for next group */
	enc->group_samplestamp += enc->nsample_per_group;

	xfree(spx_frame);
	
	return MP_OK;
}

int spxr_done(media_maker_t *maker)
{
	speex_encoder_t *enc = (speex_encoder_t *)maker;

	/*Destroy the encoder state */
	speex_encoder_destroy(enc->speex_info->est);

	/*Destroy the bit-packing struct */
	speex_bits_destroy(&enc->speex_info->bits);

	xfree(enc->encoding_frame);
	xfree(maker);

	return MP_OK;
}

media_stream_t* spxr_new_media_stream(media_maker_t* maker, media_device_t* dev, media_player_t* player, media_info_t* minfo)
{
	int ret;
	media_stream_t* strm;

	speex_encoder_t *enc = (speex_encoder_t *)maker;

	int rate;
	int nchannel;
	int frame_bytes;
	int frame_size;

	speex_info_t* spxinfo= (speex_info_t*)minfo;

	/* Create mode */
	if(spxinfo->audioinfo.info.sample_rate < 6000)
		return NULL;
	else if(spxinfo->audioinfo.info.sample_rate <= 12500)
		spxinfo->spxmode = speex_lib_get_mode(SPEEX_MODEID_NB);
	else if(spxinfo->audioinfo.info.sample_rate <= 25000)
		spxinfo->spxmode = speex_lib_get_mode(SPEEX_MODEID_WB);
	else if(spxinfo->audioinfo.info.sample_rate <= 48000)
		spxinfo->spxmode = speex_lib_get_mode(SPEEX_MODEID_UWB);
	else
		return NULL;

	strm = xmalloc(sizeof(media_stream_t));
	if(!strm)
		return NULL;
	
	ret = dev->set_input_media(dev, &maker->receiver, minfo);
	if (ret < MP_OK)
	{
		spxr_log (("spxr_new_stream: speex stream fail to open\n"));
		xfree(strm);
		return NULL;
	}

	memset(strm, 0, sizeof(media_stream_t));

	strm->maker = maker;
	strm->player = player;
	enc->media_stream = strm;

	enc->speex_info = spxinfo;

	spxinfo->est = speex_encoder_init(spxinfo->spxmode);

	/* Set the quality to 8 (15 kbps) 
	spxinfo->quality=8;
	*/
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

	/* Initialization of the structure that holds the bits */
	speex_bits_init(&spxinfo->bits);

	rate = enc->speex_info->audioinfo.info.sample_rate;
	frame_size = enc->speex_info->nsample_per_frame;
	nchannel = spxinfo->audioinfo.channels;
	frame_bytes = nchannel * frame_size * sizeof(int16);
   
	enc->encoding_nsample_expect = rate / 50;  /* smaples / 20ms */
	enc->nsample_per_group = enc->nframe_group * enc->speex_info->nsample_per_frame;

	enc->lookahead = 0;
	speex_encoder_ctl(spxinfo->est, SPEEX_GET_LOOKAHEAD, &enc->lookahead);
   
	if (enc->speex_info->denoise || enc->speex_info->agc)
	{
		spxinfo->preprocess = speex_preprocess_state_init(frame_size, rate);
		speex_preprocess_ctl(spxinfo->preprocess, SPEEX_PREPROCESS_SET_DENOISE, &enc->speex_info->denoise);
		speex_preprocess_ctl(spxinfo->preprocess, SPEEX_PREPROCESS_SET_AGC, &enc->speex_info->agc);
		enc->lookahead += frame_size;
	}

	enc->encoding_frame = xmalloc(frame_bytes);
	enc->encoding_nsample_expect = frame_size;
	enc->encoding_gap = enc->encoding_frame;
	
	return strm;
}

module_interface_t* media_new_receiver()
{
   speex_encoder_t *enc = xmalloc(sizeof(struct speex_encoder_s));
   if(!enc)
   {
      spxr_debug(("vorbis_new_player: No memory to allocate\n"));
      return NULL;
   }

   memset(enc, 0, sizeof(struct speex_encoder_s));

   enc->maker.done = spxr_done;
   enc->maker.new_media_stream = spxr_new_media_stream;

   enc->maker.receiver.match_type = spxr_match_type;
   enc->maker.receiver.receive_media = spxr_receive_media;
   
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

   media_new_receiver   /* Module initializer */
};
                   
