/***************************************************************************
                          ogm_dshow.c  -  Ogg/DirectShow stream
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

#define OGM_DSHOW_LOG
#define OGM_DSHOW_DEBUG

#ifdef OGM_DSHOW_LOG
   const int ogm_dshow_log = 1;
#else
   const int ogm_dshow_log = 0;
#endif 
#define ogm_dshow_log(fmtargs)  do{if(ogm_dshow_log) printf fmtargs;}while(0)

#ifdef OGM_DSHOW_DEBUG
   const int ogm_dshow_debug = 1;
#else
   const int ogm_dshow_debug = 0;
#endif
#define ogm_dshow_debug(fmtargs)  do{if(ogm_dshow_debug) printf fmtargs;}while(0)

/* Open a new 'Ditect Show' stream in the ogm file */
int ogm_open_directshow(ogm_format_t *ogm, ogm_media_t * handler, ogg_stream_state *sstate, int sno, stream_header *sth){

   /* 'Direct Show' stream detected */
   if ((*(int32*)(ogm->packet.packet+96) == 0x05589f80) &&
      (ogm->packet.bytes >= 184)) {

      ogm_dshow_log(("ogm_open_directshow: 's%d' Found old video header. Not " \
                              "supported.\n", (sno+1)));

   } else if (*(int32*)(ogm->packet.packet+96) == 0x05589F81) {

      ogm_dshow_log(("ogm_open_directshow: 's%d' Found old audio header. Not " \
                              "supported.\n", (sno+1)));

   } else {

      ogm_dshow_log(("ogm_open_file: OGG stream 's%d' has an old header with an " \
                              "unknown type.", (sno+1)));
   }

   return MP_OK;
}

int ogm_detect_directshow (ogg_packet *packet) {

   if ( (packet->bytes >= 142) &&
        strncmp(&(packet->packet[1]), "Direct Show Samples embedded in Ogg", 35) == 0 ) {

        return 1;
   }

   return 0;
}

/* Handle DirectShow packet */
int ogm_process_directshow(ogm_format_t * ogm, ogm_stream_t *ogm_strm, ogg_page *page, ogg_packet *pack, int hdrlen, int64 lenbytes, int64 samplestamp, int last_packet, int stream_end){

   return MP_EIMPL;
}

int ogm_done_directshow (ogm_media_t * ogmm) {

   free(ogmm);

   return MP_OK;
}

module_interface_t * ogm_new_media() {

   ogm_media_t * ogmm = malloc(sizeof(struct ogm_media_s));
   if (!ogmm) {

      ogm_dshow_log(("ogm_new_media: No memory\n"));
      return NULL;
   }

   memset(ogmm, 0, sizeof(struct ogm_media_s));

   ogmm->done = ogm_done_directshow;
   ogmm->detect_media = ogm_detect_directshow;
   ogmm->open_media = ogm_open_directshow;
   ogmm->process_media = ogm_process_directshow;

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
