/***************************************************************************
                          speex_codec.c  -  speex voice codec
                             -------------------
    begin                : Web Sep 1 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org, heming@bigfoot.com
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

/*
*/
#define SPEEX_CODEC_LOG
#define SPEEX_CODEC_DEBUG

#ifdef SPEEX_CODEC_LOG
 #define spxc_log(fmtargs)  do{printf fmtargs;}while(0)
#else                     
 #define spxc_log(fmtargs)
#endif

#ifdef SPEEX_CODEC_DEBUG
 #define spxc_debug(fmtargs)  do{printf fmtargs;}while(0)
#else                
 #define spxc_debug(fmtargs)
#endif

#ifndef _V_CLIP_MATH
#define _V_CLIP_MATH

static ogg_int32_t CLIP_TO_15(ogg_int32_t x)
{
      int ret=x;
      ret-= ((x<=32767)-1)&(x-32767);
      ret-= ((x>=-32768)-1)&(x+32768);
      return(ret);
}
#endif  /* _V_CLIP_MATH */

media_frame_t* spxc_decode(speex_info_t *spxinfo, ogg_packet *op, media_pipe_t *output)
{
	int i;
	int clipflag = 0;

	int nframes;
	int nchannel;
	int frame_size;
	int nsample;
	int frame_bytes;
   
	int lost=0;
	char *pcm;

	media_frame_t *auf = NULL;

	nframes = spxinfo->header->frames_per_packet;
	nchannel = spxinfo->audioinfo.channels;
	frame_size = spxinfo->header->frame_size;
	nsample = nframes * nchannel * frame_size;
	frame_bytes = nchannel * frame_size * sizeof(int16);
   
	/*Copy Ogg packet to Speex bitstream*/
	speex_bits_read_from(&spxinfo->bits, (char*)op->packet, op->bytes);

	auf = (media_frame_t*)output->new_frame(output, nframes * frame_bytes, NULL);
	if (!auf)
	{
		spxc_debug(("spxc_decode: no frame space available\n"));
		return NULL;
	}

	pcm = auf->raw;

	for (i=0; i!=nframes; i++)
	{
		int ret;

		/*Decode frame*/
		if (!lost)
			ret = speex_decode_int(spxinfo->st, &spxinfo->bits, (int16*)pcm);
		else
			ret = speex_decode_int(spxinfo->st, NULL, (int16*)pcm);

		if (ret==-1)
		{
			break;
		}

		if (ret==-2)
		{
			spxc_log(("speex_decode: Decoding error: corrupted stream?\n"));
			output->recycle_frame(output, auf);
			break;
		}

		if (speex_bits_remaining(&spxinfo->bits)<0)
		{
			spxc_log(("speex_decode: Decoding overflow: corrupted stream?\n"));
			output->recycle_frame(output, auf);
			break;
		}

		if (nchannel==2)
			speex_decode_stereo_int((int16*)pcm, frame_size, spxinfo->stereo);

		/* Convert to short and save to output file
		for (i=0; i<frame_size*channels; i++)
			out[i]=output[i];
		*/
		pcm += frame_bytes;
	}

	auf->nraw = nsample;
	auf->usec = 1000000 * nsample / ((media_info_t*)spxinfo)->sample_rate;  /* microsecond unit */

	return auf;
}

