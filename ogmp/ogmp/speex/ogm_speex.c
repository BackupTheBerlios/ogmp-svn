/***************************************************************************
                          ogm_speex.c  -  Ogg/Speex stream
                             -------------------
    begin                : Sun Jan 18 2004
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
 
/* some code took from www.speex.org project,
 * Copyright (C) 2002-2003 Jean-Marc Valin 
 */

#include <timedia/xmalloc.h>
#include <timedia/xstring.h>
#include <timedia/ui.h>

#include "../format_ogm/ogm_format.h"

#include "speex_info.h"

#define OGM_SPEEX_LOG
#define OGM_SPEEX_DEBUG

#ifdef OGM_SPEEX_LOG
 #define ogm_speex_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define ogm_speex_log(fmtargs)  
#endif 

#ifdef OGM_SPEEX_DEBUG
 #define ogm_speex_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define ogm_speex_debug(fmtargs)
#endif

int ogm_detect_speex(ogg_packet * packet)
{
	char sig[9];

	strncpy(sig, packet->packet, 8);
	sig[8] = '\0';

	if ((packet->bytes >= 60) && !strncmp(&(packet->packet[0]), "Speex   ", 8)) /* "Speex" following 3 blank space */
	{
		/* 'Ogg/Speex' stream detected */
		ogm_speex_log(("ogm_detect_speex: voice stream detected\n"));
		return 1;
	}
      
	return 0;   
}                                     

int speex_open_header(ogg_packet *op, speex_info_t *spxinfo)
{
   SpeexHeader *header;

   SpeexStereoState stereo = SPEEX_STEREO_STATE_INIT;

   audio_info_t *ainfo = (audio_info_t*)spxinfo;

   header = speex_packet_to_header((char*)op->packet, op->bytes);
   if (!header)
   {
      ogm_speex_log(("speex_open_header: Cannot read header\n"));
      return MP_FAIL;
   }

   if (header->mode >= SPEEX_NB_MODES)
   {
      ogm_speex_log(("speex_open_header: Mode #%d not exist in this version\n", header->mode));
      return MP_FAIL;
   }
      
   if (header->speex_version_id > 1)
   {
      ogm_speex_log(("speex_open_header: Cannot decode version.%d speex source\n", header->speex_version_id));
      return MP_FAIL;
   }

   spxinfo->version = header->speex_version_id;
   spxinfo->bitstream_version = header->mode_bitstream_version;
   spxinfo->mode = header->mode;

   /* only in svn recently*/ 
   spxinfo->spxmode = speex_lib_get_mode(header->mode);

   /* in released version, not work on speex v1.1.6
	spxinfo->spxmode = speex_mode_list[header->mode];
   */
	
   if (spxinfo->spxmode->bitstream_version < header->mode_bitstream_version)
   {
      ogm_speex_log(("speex_open_header: Cannot decode newer version speex source\n"));
      return MP_FAIL;
   }

   if (spxinfo->spxmode->bitstream_version > header->mode_bitstream_version) 
   {
      ogm_speex_log(("speex_open_header: Cannot decode older version speex source\n"));
      return MP_FAIL;
   }

   /* clockrate */
   ainfo->info.sample_rate = header->rate;

   if(!header->frames_per_packet)
	   header->frames_per_packet = 1;

   spxinfo->nframe_per_packet = header->frames_per_packet;
   spxinfo->nsample_per_frame = header->frame_size;
   spxinfo->bitrate_now = header->bitrate;
   spxinfo->vbr = header->vbr;

   ainfo->channels = header->nb_channels;
   ainfo->info.coding_parameter = ainfo->channels;

   if(header->nb_channels == 2)
	   memcpy(&spxinfo->decstereo, &stereo, sizeof(stereo));

   spxinfo->nheader = 2 + header->extra_headers;

   free(header);

   return MP_OK;
}

int ogm_start_speex (media_stream_t* stream)
{
   if(!stream->player)
      return MP_FAIL;

   stream->player->start (stream->player);

   return MP_OK;
}

int ogm_stop_speex (media_stream_t* stream)
{
   if(!stream->player)
      return MP_FAIL;

   stream->player->stop (stream->player);

   return MP_OK;
}

/* Open a new 'Ogg/Speex' stream in the ogm file */
media_stream_t* ogm_open_speex(ogm_media_t * handler, ogm_format_t *ogm, media_control_t *ctrl, ogg_stream_state *sstate, int sno, stream_header *sth)
{
	speex_info_t *spxinfo = NULL;

	media_stream_t *stream = NULL;

	media_format_t *mf = (media_format_t *)ogm;
   
	ogm_stream_t *ogm_strm = (ogm_stream_t *)mf->find_stream(mf, sno);
   
	ogm_speex_log(("ogm_open_speex: open speex with stream #%d as time reference\n", sno));
   
	if(!ogm_strm)
	{
		ogm_speex_log(("ogm_open_speex: no stream #%d, new allocation\n", sno));

		ogm_strm = xmalloc(sizeof(ogm_stream_t));
		if(!ogm_strm)
		{
			ogm_speex_log(("ogm_open_speex: stream xmalloc failed.\n"));
			return NULL;
		}
      
		memset(ogm_strm, 0, sizeof(ogm_stream_t));
      
		stream = (media_stream_t *)ogm_strm;
      stream->start = ogm_start_speex;
      stream->start = ogm_stop_speex;

		strncpy(stream->mime, "audio/speex\0", strlen("audio/speex")+1);

		((ogm_stream_t*)stream)->stype = 'a';
      
		spxinfo = xmalloc(sizeof(struct speex_info_s));
		if (!spxinfo)
		{
			ogm_speex_log(("ogm_open_speex: mediainfo xmalloc failed.\n"));
         stream->playable = -1;
			return stream;
		}

		memset(spxinfo, 0, sizeof(struct speex_info_s));

		stream->media_info = (media_info_t*)spxinfo;
		stream->media_info->mime = xstr_clone("audio/speex");
	}
	else
	{
		stream = (media_stream_t *)ogm_strm;
		spxinfo = (speex_info_t*)stream->media_info;      
	}
			
	if(stream->playable == -1)
      return stream;

	if(spxinfo->head_packets < 2 && stream->playable != -1)
	{
		spxinfo->head_packets++;
	
		if (spxinfo->head_packets == 1)
		{
			if(speex_open_header(&ogm->packet, spxinfo) < MP_OK)
			{
				stream->playable = -1;
			   ogm_speex_log(("ogm_open_speex: speex unplayable.\n"));
				return stream;
			}
				
			mf->add_stream(mf, stream, sno, 'a');

			stream->sample_rate = spxinfo->audioinfo.info.sample_rate;
			stream->handler = handler;
            
			ogm_strm->serial = sno;
			/*ogm_strm->acodec = ACSPEEX;*/

			ogm_strm->sno = mf->nastreams;
			ogm_strm->stype = 'a';
			ogm_strm->instate = sstate;

	      speex_bits_init(&spxinfo->decbits);

         ogm_speex_log(("ogm_open_speex: version[%d]\n", spxinfo->version));
         ogm_speex_log(("ogm_open_speex: bitstream version[%d]\n", spxinfo->bitstream_version));

         ogm_speex_debug(("ogm_open_speex: channels[%d]\n", spxinfo->audioinfo.channels));
         ogm_speex_debug(("ogm_open_speex: coding_parameter[%d]\n", spxinfo->audioinfo.info.coding_parameter));
         ogm_speex_debug(("ogm_open_speex: clockrate[%d]\n", spxinfo->audioinfo.info.sample_rate));
         ogm_speex_debug(("ogm_open_speex: bitrate[%d]\n", spxinfo->bitrate_now));
         ogm_speex_debug(("ogm_open_speex: samples/frame[%d]; frames/packet[%d]\n", spxinfo->nsample_per_frame, spxinfo->nframe_per_packet));
         ogm_speex_debug(("ogm_open_speex: headers[%d]\n", spxinfo->nheader));
         ogm_speex_debug(("ogm_open_speex: mode[%d]\n", spxinfo->mode));
         ogm_speex_debug(("ogm_open_speex: mime[%s]\n", spxinfo->audioinfo.info.mime));

			ogm_speex_debug(("ogm_open_speex: speex info ok\n"));
		} 

      if (spxinfo->head_packets < spxinfo->nheader)
		{
			/* Comment Header and Extra */
         ogm_speex_log(("ogm_open_speex: Skip Comment and Extra Header\n"));
		} 
	}
	
	ogm_speex_log(("ogm_open_speex: stream #%d opened as Speex\n", sno));

	return stream;
}

/* Handle speex packet */
int ogm_process_speex(ogm_format_t * ogm, ogm_stream_t *ogm_strm, ogg_page *page, ogg_packet *pack, int hdrlen, int64 lenbytes, int64 samplestamp, int last_packet, int stream_end)
{
	/* Output the Ogg/Speex media */
	media_frame_t frame;
	media_format_t *mf = (media_format_t *)ogm;   
	media_stream_t *stream = (media_stream_t *)ogm_strm;
	speex_info_t *spxinfo = (speex_info_t *)stream->media_info;
   
	if(stream->playable == -1)
	{
		ogm_speex_log(("ogm_process_speex: (a%d/%d) Speex voice stream isn't playable\n", ogm_strm->sno, mf->numstreams));
		return MP_FAIL;
	}

	if(spxinfo->head_packets < spxinfo->nheader)
	{
		spxinfo->head_packets++;

		/* Comment Header and Extra */
		ogm_speex_log(("ogm_process_speex: Skip rest Headers\n"));

		return MP_OK;
	}

   if(stream->playable == 0)
   {
      stream->playable = 1;
      stream->start(stream);
   }

	frame.bytes = pack->bytes;
	frame.raw = pack->packet;
   frame.nraw = spxinfo->nsample_per_frame;
	frame.samplestamp = samplestamp;
	frame.sno = pack->packetno;
   
	/* send speex packet */
	if (last_packet && stream_end)
	{   
		/* ogg_page_eos(page) */
		stream->player->receiver.receive_media(&stream->player->receiver, &frame, samplestamp, MP_EOS);
	}
	else
	{
		stream->player->receiver.receive_media(&stream->player->receiver, &frame, samplestamp, last_packet);
	}
   
	return MP_OK;
}                       

int ogm_done_speex (ogm_media_t * ogmm)
{
   xfree(ogmm);

   return MP_OK;
}

module_interface_t * ogm_new_media()
{
   ogm_media_t * ogmm = xmalloc(sizeof(struct ogm_media_s));
   if (!ogmm) {

      ogm_speex_debug(("ogm_new_media: No memory\n"));
      return NULL;
   }

   memset(ogmm, 0, sizeof(struct ogm_media_s));

   ogmm->done = ogm_done_speex;
   ogmm->detect_media = ogm_detect_speex;
   ogmm->open_media = ogm_open_speex;
   ogmm->process_media = ogm_process_speex;
   
   return ogmm;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC 
module_loadin_t mediaformat =
{
   "ogm",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   ogm_new_media   /* Module initializer */
};
