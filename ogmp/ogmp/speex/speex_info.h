/***************************************************************************
                          speex_info.h  -  Speex media info
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

#define OGM_SPEEX_H

#include "../media_format.h"

#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
//#include <speex/speex_callbacks.h>

typedef struct speex_info_s speex_info_t;

struct speex_info_s
{
   struct audio_info_s audioinfo;
   
   SpeexBits bits;

   SpeexHeader *header;

   SpeexStereoState *stereo;

   const SpeexMode *mode;

   void *st;

   int head_packets;

   int nheader;

   /* judge if audio packet started, as first audio packet has not ready samples */
   int audio_start;
};
