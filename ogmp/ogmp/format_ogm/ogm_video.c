/***************************************************************************
                          ogm_video.c  -  Ogg/Video stream
                             -------------------
    begin                : Sun May 8 2004
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
 
/* some code took from ogmtool */

#include "string.h"
#include "ogm_format.h"

#define OGM_VIDEO_LOG
#define OGM_VIDEO_DEBUG

#ifdef OGM_VIDEO_LOG
   const int ogm_video_log = 1;
#else
   const int ogm_video_log = 0;
#endif 
#define ogm_video_log(fmtargs)  do{if(ogm_video_log) printf fmtargs;}while(0)

#ifdef OGM_VIDEO_DEBUG
   const int ogm_video_debug = 1;
#else
   const int ogm_video_debug = 0;
#endif
#define ogm_video_debug(fmtargs)  do{if(ogm_video_debug) printf fmtargs;}while(0)

/* Detect a video media */
int ogm_detect_video (ogg_packet *packet) {

   stream_header sth;

   if ( ((*(packet->packet) & PACKET_TYPE_BITS ) == PACKET_TYPE_HEADER) &&
       (packet->bytes >= (int)(sizeof(old_stream_header) + 1)) ) {

      /* Other type stream */
      copy_headers(&sth, (old_stream_header *)&(packet->packet[1]), packet->bytes);

      if (!strncmp(sth.streamtype, "video", 5)) return 1;
   }

   return 0;
}

/* Open a new video stream in the ogm file */
int ogm_open_video(ogm_media_t * handler, ogm_format_t *ogm, media_control_t *ctrl, ogg_stream_state *sstate, int sno, stream_header *sth){

   unsigned long codec;
   char ccodec[5];
   ogm_stream_t *ogm_strm = NULL;
   media_stream_t *stream = NULL;

   strncpy(ccodec, sth->subtype, 4);
   ccodec[4] = 0;
   codec = (sth->subtype[0] << 24) + (sth->subtype[1] << 16) +
            (sth->subtype[2] << 8) + sth->subtype[3];

   /* stream type ccodec */
   ogm_strm = (ogm_stream_t *)malloc(sizeof(ogm_stream_t));
   stream = (media_stream_t *)ogm_strm;

   if (stream == NULL) {

      ogm_video_log(("ogm_open_video: malloc failed.\n"));
      exit(1);
   }

   memcpy(&(ogm_strm->header), sth, ogm->packet.bytes);

   //nstrm++;
   //v_nstrm++;

   stream->handler = handler;
   
   ogm_strm->stype = 'v';
   ogm_strm->serial = sno;
   stream->sample_rate = 10000000 / (long)get_uint64(&sth->time_unit);

   ogm_strm->sno = (ogm->format.nvstreams+1);
   ogm_strm->instate = sstate;

   ogm_video_log(("ogm_open_video: (v%d/%d) fps: %ld width height: %dx%d " \
                  "codec: %p (%s)\n", (ogm->format.nvstreams+1), (ogm->format.numstreams+1),
                  stream->sample_rate,
                  get_uint32(&sth->sh.video.width),
                  get_uint32(&sth->sh.video.height), (void *)codec,
                  ccodec));

   ogm->format.add_stream((media_format_t*)ogm, stream, sno, ogm_strm->stype);
   /* End of video stream head parsing */

   return MP_OK;
}

/* Handle Audio packet */
int ogm_process_video(ogm_format_t * ogm, ogm_stream_t *ogm_strm, ogg_page *page, ogg_packet *pack, int hdrlen, int64 lenbytes, int64 samplestamp, int last_packet, int stream_end){

      /* Video packet */
      if ((*pack->packet & 3) == PACKET_TYPE_HEADER){

         ogm_video_log(("ogm_process_video: Video stream[%d:v%d]-header %ld bytes\n", ogm_strm->serial, ogm_strm->sno, pack->bytes));
      }

      if ((*pack->packet & 3) == PACKET_TYPE_COMMENT){

         ogm_video_log(("ogm_process_video: Video stream[%d:v%d]-comment %ld bytes\n", ogm_strm->serial, ogm_strm->sno, pack->bytes));
      }

      /* Output the Video media

      if (!xraw)

         i = AVI_write_frame(stream->avi, (char *)&pack->packet[hdrlen + 1],
                            pack->bytes - 1 - hdrlen,
                            (pack->packet[0] & PACKET_IS_SYNCPOINT) ? 1 : 0);
      else

         i = write(stream->fd, (char *)&pack->packet[hdrlen + 1],
                  pack->bytes - 1 - hdrlen);

      */
      //ogm_video_log("demux_ogm_handle_packet: Video Stream[%d:v%d]-data %ld bytes\n", stream->serial, stream->sno, pack->bytes);

   return MP_OK;
}

int ogm_done_video (ogm_media_t * ogmm) {

   free(ogmm);

   return MP_OK;
}

module_interface_t * ogm_new_media() {

   ogm_media_t * ogmm = malloc(sizeof(struct ogm_media_s));
   if (!ogmm) {

      ogm_video_debug(("ogm_new_media: No memory\n"));
      return NULL;
   }

   memset(ogmm, 0, sizeof(struct ogm_media_s));

   ogmm->done = ogm_done_video;
   ogmm->detect_media = ogm_detect_video;
   ogmm->open_media = ogm_open_video;
   ogmm->process_media = ogm_process_video;

   return ogmm;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC
module_loadin_t mediaformat = {

   "ogm",   /* Label */

   000001,         /* Plugin Version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   ogm_new_media   /* Module initializer */
};
