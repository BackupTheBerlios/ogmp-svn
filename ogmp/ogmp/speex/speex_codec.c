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
 
#include <timedia/xmalloc.h>
#include "speex_codec.h"
#include <timedia/ui.h>
/*
*/
#define SPEEX_CODEC_LOG
#define SPEEX_CODEC_DEBUG

#ifdef SPEEX_CODEC_LOG
 #define spxc_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else                     
 #define spxc_log(fmtargs)
#endif

#ifdef SPEEX_CODEC_DEBUG
 #define spxc_debug(fmtargs)  do{printf fmtargs;}while(0)
#else                
 #define spxc_debug(fmtargs)
#endif

media_frame_t* spxc_decode(speex_info_t *spxinfo, media_frame_t *spxf, media_pipe_t *output)
{
	int i;

	int nframes;
	int nchannel;
	int frame_size;
	int nsample;
	int frame_bytes;
   
	int lost=0;
	char *pcm;

	media_frame_t *auf = NULL;

	nframes = spxinfo->nframe_per_packet;
	frame_size = spxinfo->nsample_per_frame;
	nchannel = spxinfo->audioinfo.channels;

	nsample = nframes * frame_size;
	frame_bytes = nchannel * frame_size * sizeof(int16);
   
	/*Copy Ogg packet to Speex bitstream*/
	speex_bits_read_from(&spxinfo->decbits, (char*)spxf->raw, spxf->bytes);

	auf = (media_frame_t*)output->new_frame(output, nframes * frame_bytes, NULL);
	if (!auf)
	{
		spxc_debug(("spxc_decode: no frame space available\n"));
		return NULL;
	}

	pcm = auf->raw;

	for (i=0; i<nframes; i++)
	{
		int ret;

		/*Decode frame*/
		if (!lost)
			ret = speex_decode_int(spxinfo->dst, &spxinfo->decbits, (int16*)pcm);
		else
			ret = speex_decode_int(spxinfo->dst, NULL, (int16*)pcm);
		
		speex_decoder_ctl(spxinfo->dst, SPEEX_GET_BITRATE, &(spxinfo->bitrate_now));

		if (ret==-1)
			break;

		if (ret==-2)
		{
			spxc_log(("speex_decode: Decoding error: corrupted stream?\n"));
			output->recycle_frame(output, auf);
			break;
		}

		if (speex_bits_remaining(&spxinfo->decbits)<0)
		{
			spxc_log(("speex_decode: Decoding overflow: corrupted stream?\n"));
			output->recycle_frame(output, auf);
			break;
		}

		/* frame bytes will double after decode stereo, samples stored as left,right,left,right,... */
		if (nchannel==2)
			speex_decode_stereo_int((int16*)pcm, frame_size, &spxinfo->decstereo);
		
		pcm += frame_bytes;
	}

	auf->nraw = nsample;
	auf->usec = 1000000 * nsample / ((media_info_t*)spxinfo)->sample_rate;  /* microsecond unit */

	return auf;
}

int spxc_encode(speex_encoder_t* enc, speex_info_t* spxinfo, char* pcm, int pcm_bytes, char* spx, int spx_bytes)
{
	int nframes;
	int nchannel;
	int frame_size;
	int nsample;
	int frame_bytes;
   
	nframes = spxinfo->nframe_per_packet;
	frame_size = spxinfo->nsample_per_frame;
	nchannel = spxinfo->audioinfo.channels;

	nsample = nframes * frame_size;
	frame_bytes = nchannel * frame_size * sizeof(int16);

	/*Flush all the bits in the struct so we can encode a new frame*/
	speex_bits_reset(&spxinfo->encbits);

	if (nchannel==2)
   {
		spxc_debug(("\rspeex_encode: encode stereo\n"));
		speex_encode_stereo_int((short*)pcm, frame_size, &spxinfo->encbits);
	}
   	
	if (spxinfo->agc || spxinfo->denoise)
   {
		spxc_debug(("\rspeex_encode: agc or denoise perprocess\n"));
		speex_preprocess(spxinfo->encpreprocess, (short*)pcm, NULL);
   }
   
	/*Encode the frame*/
	speex_encode_int(spxinfo->est, (short*)pcm, &spxinfo->encbits);

	speex_bits_insert_terminator(&spxinfo->encbits);

	spx_bytes = speex_bits_write(&spxinfo->encbits, spx, spx_bytes);

	return spx_bytes;
}
