/***************************************************************************
                          ogm_vorbis.h  -  Ogg/Vorbis stream
                             -------------------
    begin                : Fri Jan 16 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#define OGM_VORBIS_H

#include "../media_format.h"

#ifdef OGG_TREMOR_CODEC
   #include "Tremor/ivorbiscodec.h"
#else
   #include <vorbis/codec.h>
#endif

typedef struct vorbis_info_s vorbis_info_t;
struct vorbis_info_s {

   struct audio_info_s audioinfo;
   
   vorbis_info       vi;
   vorbis_comment    vc;
   vorbis_dsp_state  vd; /* central working state for packet->PCM decoder */
   vorbis_block      vb; /* local working state for packet->PCM decoder */

   int head_packets;

   char *codebook;
   int   codebook_len;
};
