/***************************************************************************
                          ogm_text.c  -  Ogg/Text stream
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

#define OGM_TEXT_LOG
#define OGM_TEXT_DEBUG

#ifdef OGM_TEXT_LOG
   const int ogm_text_log = 1;
#else
   const int ogm_text_log = 0;
#endif 
#define ogm_text_log(fmtargs)  do{if(ogm_text_log) printf fmtargs;}while(0)

#ifdef OGM_VIDEO_DEBUG
   const int ogm_text_debug = 1;
#else
   const int ogm_text_debug = 0;
#endif
#define ogm_text_debug(fmtargs)  do{if(ogm_text_debug) printf fmtargs;}while(0)

/* Detect a text media */
int ogm_detect_text (ogg_packet *packet) {

   stream_header sth;

   if ( ((*(packet->packet) & PACKET_TYPE_BITS ) == PACKET_TYPE_HEADER) &&
       (packet->bytes >= (int)(sizeof(old_stream_header) + 1)) ) {

      /* Other type stream */
      copy_headers(&sth, (old_stream_header *)&(packet->packet[1]), packet->bytes);

      if (!strncmp(sth.streamtype, "text", 4)) return 1;
   }

   return 0;
}

/* Open a new text stream in the ogm file */
int ogm_open_text(ogm_format_t *ogm, ogm_media_t * handler, ogg_stream_state *sstate, int sno, stream_header *sth){

   ogm_stream_t *ogm_strm = (ogm_stream_t *)malloc(sizeof(ogm_stream_t));
   media_stream_t *stream = (media_stream_t *)ogm_strm;

   if (stream == NULL) {

      ogm_text_log(("ogm_open_text: malloc failed\n"));
      exit(1);
   }

   memcpy(&(ogm_strm->header), &sth, ogm->packet.bytes);
   ogm->format.add_stream((media_format_t*)ogm, stream, sno, ogm_strm->stype);

   //nstrm++;
   //t_nstrm++;

   stream->handler = handler;
   
   ogm_strm->sno = ogm->format.ntstreams;
   ogm_strm->stype = 't';
   stream->sample_rate = 10000000 / (long)get_uint64(&sth->time_unit);

   ogm_strm->serial = sno;
   ogm_strm->instate = sstate;

   ogm_text_log(("ogm_open_text: (t%d/%d) text/subtitle stream\n", ogm->format.ntstreams, ogm->format.numstreams));

   return MP_OK;
}

/* Handle Text packet */
int ogm_process_text(ogm_format_t * ogm, ogm_stream_t *ogm_strm, ogg_page *page, ogg_packet *pack, int hdrlen, int64 lenbytes, int ustamp, int last_packet, int stream_end){

   char *sub;

   ogg_int64_t sst;
   ogg_int64_t pgp;

   int end;

   media_stream_t *stream = (media_stream_t *)ogm_strm;

   if ((*pack->packet & 3) == PACKET_TYPE_HEADER){

      ogm_text_log(("ogm_process_text: Text stream[%d:t%d]-header %ld bytes\n", ogm_strm->serial, ogm_strm->sno, pack->bytes));
      return MP_OK;
   }

   if ((*pack->packet & 3) == PACKET_TYPE_COMMENT){

      ogm_text_log(("ogm_process_text: Text stream[%d:t%d]-comment %ld bytes\n", ogm_strm->serial, ogm_strm->sno, pack->bytes));
      return MP_OK;
   }

   /* Output the Text media
   if (xraw) {

      i = write(stream->fd, (char *)&pack->packet[hdrlen + 1],
                  pack->bytes - 1 - hdrlen);

      demux_ogm_log("handle_packet: x/t%d: %d written\n",
                  stream->sno, i);

      return XRTP_OK;
   }
   */

   sub = (char *)&pack->packet[hdrlen + 1];

   if ((strlen(sub) > 1) || ((strlen(sub) > 0) && (*sub != ' '))) {

      sst = (pack->granulepos / stream->sample_rate) * 1000;
      pgp = sst + lenbytes;

      /* Output the Subtitle media

      sprintf(out, "Subtitle['%d\r\n%02d:%02d:%02d,%03d --> " \
                "%02d:%02d:%02d,%03d']\r\n", stream->subnr + 1,
                (int)(sst / 3600000),
                (int)(sst / 60000) % 60,
                (int)(sst / 1000) % 60,
                (int)(sst % 1000),
                (int)(pgp / 3600000),
                (int)(pgp / 60000) % 60,
                (int)(pgp / 1000) % 60,
                (int)(pgp % 1000));

      i = write(stream->fd, out, strlen(out));
      */

      end = strlen(sub) - 1;

      while ((end >= 0) && ((sub[end] == '\n') || (sub[end] == '\r'))) {

               sub[end] = 0;
               end--;
      }

      /*
      i += write(stream->fd, sub, strlen(sub));
      i += write(stream->fd, "\r\n\r\n", 4);
       */

      ogm_strm->subnr++;

      ogm_text_log(("ogm_process_text: Subtitle stream[%d:t%d]-data %ld bytes, \n", ogm_strm->serial, ogm_strm->sno, pack->bytes));
   }

   return MP_OK;
}

int ogm_done_text (ogm_media_t * ogmm) {

   free(ogmm);

   return MP_OK;
}

module_interface_t * ogm_new_media() {

   ogm_media_t * ogmm = malloc(sizeof(struct ogm_media_s));
   if (!ogmm) {

      ogm_text_debug(("ogm_new_media: No memory\n"));
      return NULL;
   }

   memset(ogmm, 0, sizeof(struct ogm_media_s));

   ogmm->done = ogm_done_text;
   ogmm->detect_media = ogm_detect_text;
   ogmm->open_media = ogm_open_text;
   ogmm->process_media = ogm_process_text;

   return ogmm;
}

/**
 * Loadin Infomation Block
 */
module_loadin_t mediaformat = {

   "ogm",   /* Label */
   
   000001,         /* Plugin Version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */
   
   ogm_new_media   /* Module initializer */
};
