/***************************************************************************
                          ogm_vorbis.c  -  Ogg/Vorbis stream
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
 
/* some code took from Tremor vorbisfile.c */

#include <timedia/xmalloc.h>
#include <string.h>

#include "../format_ogm/ogm_format.h"
#include "vorbis_info.h"

#ifdef OGG_TREMOR_CODEC
   #include "Tremor/ivorbiscodec.h"
#else
   #include <vorbis/codec.h>
#endif

#define OGM_VORBIS_LOG_FLAG
#define OGM_VORBIS_DEBUG_FLAG

#ifdef OGM_VORBIS_LOG_FLAG
 #define ogm_vorbis_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define ogm_vorbis_log(fmtargs)  
#endif 

#ifdef OGM_VORBIS_DEBUG_FLAG
 #define ogm_vorbis_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define ogm_vorbis_debug(fmtargs)
#endif

int ogm_detect_vorbis(ogg_packet * packet){

   if ((packet->bytes >= 7) && ! strncmp(&(packet->packet[1]), "vorbis", 6)) {

      /* 'Ogg/Vorbis' stream detected */
      ogm_vorbis_log(("ogm_vorbis.ogm_detect_vorbis: stream detected\n"));

      return 1;
   }
      
   return 0;   
}                                     

/* helpers: from Vorbis codebase info.c */
static int ilog2(unsigned int v){

  int ret=0;

  while(v)
  {
    ret++;
    v>>=1;
  }

  return(ret);
}

/* Open a new 'Ogg/Vorbis' stream in the ogm file */
int ogm_open_vorbis(ogm_media_t * handler, ogm_format_t *ogm, media_control_t *ctrl, ogg_stream_state *sstate, int sno, stream_header *sth)
{
   int ret;

   vorbis_info_t * vinfo = NULL;

   media_stream_t * stream = NULL;

   media_format_t *mf = (media_format_t *)ogm;
   
   ogm_stream_t *ogm_strm = (ogm_stream_t *)mf->find_stream(mf, sno);
   
   ogm_vorbis_log(("ogm_open_vorbis: open vorbis with stream #%d as time reference\n", sno));
   
   if( !ogm_strm )
   {
      ogm_vorbis_log(("ogm_open_vorbis: no stream #%d, new allocation\n", sno));

      ogm_strm = xmalloc(sizeof(ogm_stream_t));
      if (ogm_strm == NULL)
	  {
         ogm_vorbis_log(("ogm_open_vorbis: stream xmalloc failed.\n"));
         return MP_EMEM;
      }
      
      memset(ogm_strm, 0, sizeof(ogm_stream_t));
      
      stream = (media_stream_t *)ogm_strm;

      strncpy(stream->mime, "audio/vorbis", strlen("audio/vorbis"));
      ((ogm_stream_t*)stream)->stype = 'a';
      
      vinfo = xmalloc(sizeof(struct vorbis_info_s));
      if (!vinfo)
	  {
         ogm_vorbis_log(("ogm_open_vorbis: mediainfo xmalloc failed.\n"));

         xfree(ogm_strm);

         return MP_EMEM;
      }

      memset(vinfo, 0, sizeof(struct vorbis_info_s));
      
      stream->media_info = (media_info_t*)vinfo;
   }
   else
   {
      stream = (media_stream_t *)ogm_strm;
      vinfo = (vorbis_info_t*)stream->media_info;      
   }

   vorbis_info_init(&vinfo->vi);
   vorbis_comment_init(&vinfo->vc);

   if(vinfo->head_packets < 3 && stream->playable != -1)
   {
      ret = vorbis_synthesis_headerin(&vinfo->vi, &vinfo->vc, &ogm->packet);
      if ( ret >= 0)
	  {
         vinfo->head_packets++;
         
         if (vinfo->head_packets == 1)
		 {
            mf->add_stream(mf, stream, sno, 'a');

            stream->sample_rate = vinfo->vi.rate;
            stream->handler = handler;
            
            ogm_strm->serial = sno;
            ogm_strm->acodec = ACVORBIS;

            /* ogmtools origional code, Why???
             * stream->sample_rate = -1;
             */
            
            ogm_strm->sno = mf->nastreams;
            ogm_strm->stype = 'a';
            ogm_strm->instate = sstate;

            /* Point to the codec_setup_info */
			vinfo->some_csi = (some_codec_setup_info*)vinfo->vi.codec_setup;
			
			/* Cache the header identification */
            vinfo->header_identification = xmalloc(ogm->packet.bytes);
            if (!vinfo->header_identification)
			{
               ogm_vorbis_log(("ogm_open_vorbis: no memory to cache header setup\n"));
               return MP_EMEM;
            }
            memcpy(vinfo->header_identification, ogm->packet.packet, ogm->packet.bytes);
            vinfo->header_identification_len = ogm->packet.bytes;

            ogm_vorbis_log(("ogm_open_vorbis: (a%d/%d) Vorbis identification (head #1) retrieved in %ld bytes\n",
                              ogm_strm->sno, mf->numstreams, ogm->packet.bytes));

            ogm_vorbis_log(("ogm_open_vorbis: version: %d\n", vinfo->vi.version));
            ogm_vorbis_log(("ogm_open_vorbis: channels: %d\n", vinfo->vi.channels));
            ogm_vorbis_log(("ogm_open_vorbis: rate: %ld\n", vinfo->vi.rate));
            ogm_vorbis_log(("ogm_open_vorbis: upper bitrate: %ld\n", vinfo->vi.bitrate_upper));
            ogm_vorbis_log(("ogm_open_vorbis: nominal bitrate: %ld\n", vinfo->vi.bitrate_nominal));
            ogm_vorbis_log(("ogm_open_vorbis: lower bitrate: %ld\n", vinfo->vi.bitrate_lower));
            ogm_vorbis_log(("ogm_open_vorbis: window bitrate: %ld\n", vinfo->vi.bitrate_window));
            ogm_vorbis_log(("ogm_open_vorbis: short blocksize: %ld\n", vinfo->some_csi->blocksizes[0]));
            ogm_vorbis_log(("ogm_open_vorbis: long blocksize: %ld\n", vinfo->some_csi->blocksizes[1]));

            /* time to open the stream with a player */            
			stream->player = ctrl->find_player(ctrl, ogm->format.default_mode, stream->mime, stream->fourcc);
			if(!stream->player)
			{
				ogm_vorbis_debug(("ogm_open_vorbis: stream can't be played\n"));
				stream->playable = -1;
			}
      
			ogm_vorbis_log(("ogm_open_vorbis: player ok\n"));
		 }

         if (vinfo->head_packets == 2)
		 {
			/* Cache the header comment */
            vinfo->header_comment = xmalloc(ogm->packet.bytes);
            if (!vinfo->header_comment)
			{
               ogm_vorbis_log(("ogm_open_vorbis: no memory to cache header setup\n"));
               return MP_EMEM;
            }
            memcpy(vinfo->header_comment, ogm->packet.packet, ogm->packet.bytes);
            vinfo->header_comment_len = ogm->packet.bytes;

            ogm_vorbis_log(("ogm_open_vorbis: (a%d/%d) Vorbis comments (head #2) retrieved in %ld bytes\n",
                              ogm_strm->sno, mf->numstreams, ogm->packet.bytes));
         }
         
         if (vinfo->head_packets == 3)
		 {
            /* initialize central decode state */
            vorbis_synthesis_init( &vinfo->vd, &vinfo->vi );

            /* initialize local state for most of the decode so multiple
             * block decodes can proceed in parallel. We could init
             * multiple vorbis_block structures for vd here */
            vorbis_block_init( &vinfo->vd, &vinfo->vb );

            ogm_vorbis_log(("ogm_open_vorbis: (a%d/%d) Vorbis setup (head #3) retrieved in %ld bytes\n",
                              ogm_strm->sno, mf->numstreams, ogm->packet.bytes));
			
			/* Cache the codebook */
            vinfo->header_setup = xmalloc(ogm->packet.bytes);
            if (!vinfo->header_setup)
			{
               ogm_vorbis_log(("ogm_open_vorbis: no memory to cache header setup\n"));
               return MP_EMEM;
            }
            memcpy(vinfo->header_setup, ogm->packet.packet, ogm->packet.bytes);
            vinfo->header_setup_len = ogm->packet.bytes;

            ret = stream->player->open_stream(stream->player, (media_info_t*)vinfo);
            if( ret < MP_OK)
			{
               stream->playable = -1;
               return ret;
            }
            
            stream->playable = 1;
			vinfo->mode_bits = ilog2(vinfo->some_csi->modes-1);

            ogm_vorbis_log(("ogm_open_vorbis: (a%d/%d) Vorbis stream intialized in Stage One, playable now\n",
                              ogm_strm->sno, mf->numstreams));

            ogm_vorbis_log(("ogm_open_vorbis: vorbis_mode_bits: %d\n", vinfo->mode_bits));
         }
      }
	  else
	  {
         ogm_vorbis_log(("ogm_open_vorbis: (a%d/%d) Vorbis audio stream indicated " \
                           "but no Vorbis stream header found.\n", ogm_strm->sno, mf->numstreams));

         return MP_FAIL;
      }
   }

   ogm_vorbis_log(("ogm_open_vorbis: stream #%d opened as Vorbis\n", sno));

   return MP_OK;
}

/* Handle Vorbis packet */
int ogm_process_vorbis(ogm_format_t * ogm, ogm_stream_t *ogm_strm, ogg_page *page, ogg_packet *pack, int hdrlen, int64 lenbytes, int64 samplestamp, int last_packet, int stream_end)
{
   /* Output the Ogg/Vorbis media */
   media_format_t *mf = (media_format_t *)ogm;
   
   media_stream_t *stream = (media_stream_t *)ogm_strm;
   vorbis_info_t *vinfo = (vorbis_info_t *)stream->media_info;
   
   int ret;
   
   if(stream->playable == -1) {

      ogm_vorbis_log(("ogm_process_vorbis: (a%d/%d) Vorbis audio stream isn't playable\n", ogm_strm->sno, mf->numstreams));
      return MP_FAIL;
   }

   if(stream->playable == 0){

      if (ogg_page_granulepos(page) > 0){

         ogm_vorbis_log(("ogm_process_vorbis: (a%d/%d) Vorbis audio stream start without initialization\n", ogm_strm->sno, mf->numstreams));
         return MP_FAIL;
      }
      
      ret = vorbis_synthesis_headerin(&vinfo->vi, &vinfo->vc, pack);
      if ( ret < 0 ) {

         if ( ret == OV_ENOTVORBIS )
            ogm_vorbis_log(("ogm_process_vorbis: (a%d/%d) is NOT a Vorbis audio stream actually\n",
                           ogm_strm->sno, mf->numstreams));

         if ( ret == OV_EBADHEADER )
            ogm_vorbis_log(("ogm_process_vorbis: (a%d/%d) Vorbis audio stream with a bad head\n",
                           ogm_strm->sno, mf->numstreams));

         ogm_vorbis_log(("ogm_process_vorbis: (a%d/%d) Vorbis stream fail to initialize, packet in %ld bytes\n",
                           ogm_strm->sno, mf->numstreams, pack->bytes));

         return MP_FAIL;
      }
      
      vinfo->head_packets++;

      if (vinfo->head_packets == 2) {

		 /* Cache the header comment */
         vinfo->header_comment = xmalloc(ogm->packet.bytes);
         if (!vinfo->header_comment) {

            ogm_vorbis_log(("ogm_open_vorbis: no memory to cache header setup\n"));
            return MP_EMEM;
         }
         memcpy(vinfo->header_comment, ogm->packet.packet, ogm->packet.bytes);
         vinfo->header_comment_len = ogm->packet.bytes;

         ogm_vorbis_log(("ogm_process_vorbis: (a%d/%d) Vorbis comments (packet #2) retrieved in %ld bytes\n",
                          ogm_strm->sno, mf->numstreams, pack->bytes));
      }
      
      if (vinfo->head_packets == 3) {
      
         /* initialize central decode state */
         vorbis_synthesis_init( &vinfo->vd, &vinfo->vi );
            
         /* initialize local state for most of the decode so multiple
          * block decodes can proceed in parallel. We could init
          * multiple vorbis_block structures for vd here */
         vorbis_block_init( &vinfo->vd, &vinfo->vb );
            
         ogm_vorbis_log(("ogm_process_vorbis: (a%d/%d) Vorbis codebook (packet #3) retrieved in %ld bytes\n",
                              ogm_strm->sno, mf->numstreams, pack->bytes));

         /* Cache the codebook */
         vinfo->header_setup = xmalloc(ogm->packet.bytes);
         if (!vinfo->header_setup) {

            ogm_vorbis_log(("ogm_open_vorbis: no memory to cache codebook\n"));
            return MP_EMEM;
         }
         
         memcpy(vinfo->header_setup, ogm->packet.packet, ogm->packet.bytes);
         vinfo->header_setup_len = ogm->packet.bytes;
         
         ret = stream->player->open_stream(stream->player, (media_info_t*)vinfo);
         
         if( ret < MP_OK)
         {
            stream->playable = -1;
            return ret;
         }         
         
         stream->playable = 1;

         vinfo->mode_bits = ilog2(vinfo->some_csi->modes-1);

         ogm_vorbis_log(("ogm_process_vorbis: (a%d/%d) Vorbis stream intialized in Stage Two, playable now\n",
                        ogm_strm->sno, mf->numstreams));

         ogm_vorbis_log(("ogm_process_vorbis: vorbis_mode_bits: %d\n", vinfo->mode_bits));
      }
      
      return MP_OK;
   }
   
   /* send vorbis packet */
   if (last_packet && stream_end) {   /* ogg_page_eos(page) */

      stream->player->receive_media(stream->player, pack, samplestamp, MP_EOS);
      
   } else {

      stream->player->receive_media(stream->player, pack, samplestamp, last_packet);
   }
   
   return MP_OK;
}

int ogm_done_vorbis (ogm_media_t * ogmm) {

   xfree(ogmm);

   return MP_OK;
}

module_interface_t * ogm_new_media() {

   ogm_media_t * ogmm = xmalloc(sizeof(struct ogm_media_s));
   if (!ogmm) {

      ogm_vorbis_debug(("ogm_new_media: No memory\n"));
      return NULL;
   }

   memset(ogmm, 0, sizeof(struct ogm_media_s));

   ogmm->done = ogm_done_vorbis;
   ogmm->detect_media = ogm_detect_vorbis;
   ogmm->open_media = ogm_open_vorbis;
   ogmm->process_media = ogm_process_vorbis;
   
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
