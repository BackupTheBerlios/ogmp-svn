/***************************************************************************
                          ogm_audio.c  -  Ogg/Audio stream
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

#define OGM_AUDIO_LOG
#define OGM_AUDIO_DEBUG

#ifdef OGM_AUDIO_LOG
   const int ogm_audio_log = 1;
#else
   const int ogm_audio_log = 0;
#endif 
#define ogm_audio_log(fmtargs)  do{if(ogm_audio_log) printf fmtargs;}while(0)

#ifdef OGM_AUDIO_DEBUG
   const int ogm_audio_debug = 1;
#else
   const int ogm_audio_debug = 0;
#endif
#define ogm_audio_debug(fmtargs)  do{if(ogm_audio_debug) printf fmtargs;}while(0)

/* Detect a Audio media */
int ogm_detect_audio (ogg_packet *packet) {

   stream_header sth;

   if ( ((*(packet->packet) & PACKET_TYPE_BITS ) == PACKET_TYPE_HEADER) &&
       (packet->bytes >= (int)(sizeof(old_stream_header) + 1)) ) {

      /* Other type stream */
      copy_headers(&sth, (old_stream_header *)&(packet->packet[1]), packet->bytes);

      if (!strncmp(sth.streamtype, "audio", 5)) return 1;
   }

   return 0;
}

/* Open a new audio stream in the ogm file */
int ogm_open_audio(ogm_format_t *ogm, ogm_media_t * handler, ogg_stream_state *sstate, int sno, stream_header *sth){

   int codec;
   char buf[5];

   ogm_stream_t *ogm_strm = NULL;
   media_stream_t *stream = NULL;

   memcpy(buf, sth->subtype, 4);
   buf[4] = 0;
   codec = strtoul(buf, NULL, 16);

   /* stream type determined by codec */

   ogm_strm = (ogm_stream_t *)malloc(sizeof(ogm_stream_t));
   stream = (media_stream_t *)ogm_strm;

   if (stream == NULL) {

      fprintf(stderr, "malloc failed.\n");
      exit(1);
   }

   memcpy(&(ogm_strm->header), sth, ogm->packet.bytes);

   ogm_add_stream((media_format_t*)ogm, (media_stream_t*)stream, sno, ogm_strm->stype);

   ogm_strm->sno = ogm->format.nastreams;
   ogm_strm->stype = 'a';
   stream->sample_rate = (long)(get_uint64(&sth->samples_per_unit) *
                         get_uint16(&sth->sh.audio.channels));

   ogm_strm->serial = sno;
   ogm_strm->acodec = codec;

   ogm_strm->instate = sstate;

   ogm_audio_log(("ogm_open_file: (a%d/%d) codec: %d (0x%04x) (%s), bits per "
                 "sample: %d channels: %hd  samples per second: %lld",
                  ogm->format.nastreams, ogm->format.numstreams, codec, codec,
                  codec == ACPCM ? "PCM" : codec == 55 ? "MP3" :
                  codec == ACMP3 ? "MP3" :
                  codec == ACAC3 ? "AC3" : "unknown",
                  get_uint16(&sth->bits_per_sample),
                  get_uint16(&sth->sh.audio.channels),
                  get_uint64(&sth->samples_per_unit)
                ));

   ogm_audio_log(("ogm_open_file: avgbytespersec: %d blockalign: %hd\n",
                  get_uint32(&sth->sh.audio.avgbytespersec),
                  get_uint16(&sth->sh.audio.blockalign)
                ));

   return MP_OK;
}

/* Handle Audio packet */
int ogm_process_audio(ogm_format_t * ogm, ogm_stream_t *ogm_strm, ogg_page *page, ogg_packet *pack, int hdrlen, int64 lenbytes, int ustamp, int last_packet, int stream_end){

   switch (ogm_strm->acodec) {

      default:

         if ((*pack->packet & 3) == PACKET_TYPE_HEADER){

            ogm_audio_log(("demux_ogm_process_packet: Audio stream[%d:a%d]-header %ld bytes\n", ogm_strm->serial, ogm_strm->sno, pack->bytes));

            break;
         }

         if ((*pack->packet & 3) == PACKET_TYPE_COMMENT){

            ogm_audio_log(("demux_ogm_process_packet: Audio stream[%d:a%d]-comment %ld bytes\n", ogm_strm->serial, ogm_strm->sno, pack->bytes));

            break;
         }

         /*
         i = write(stream->fd, pack->packet + 1 + hdrlen,
                pack->bytes - 1 - hdrlen);

         ogm_strm->bwritten += i;
         */

         ogm_audio_log(("demux_ogm_process_packet: Audio stream[%d:a%d]-data %ld bytes\n", ogm_strm->serial, ogm_strm->sno, pack->bytes));

         break;
   }

   return MP_OK;

}

int ogm_done_audio (ogm_media_t * ogmm) {

   free(ogmm);

   return MP_OK;
}

module_interface_t * ogm_new_media() {

   ogm_media_t * ogmm = malloc(sizeof(struct ogm_media_s));
   if (!ogmm) {

      ogm_audio_log(("ogm_new_media: No memory\n"));
      return NULL;
   }

   memset(ogmm, 0, sizeof(struct ogm_media_s));

   ogmm->done = ogm_done_audio;
   ogmm->detect_media = ogm_detect_audio;
   ogmm->open_media = ogm_open_audio;
   ogmm->process_media = ogm_process_audio;

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
