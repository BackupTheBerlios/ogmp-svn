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

/* copy from struct codec_setup_info in vorbis codebase codec_internal.h */

/* codec_setup_info contains all the setup information specific to the
   specific compression/decompression mode in progress (eg,
   psychoacoustic settings, channel setup, options, codebook
   etc).  
*********************************************************************/

/* mode ************************************************************/
typedef struct
{
  int blockflag;
  int windowtype;
  int transformtype;
  int mapping;

} vorbis_info_mode;

typedef struct some_codec_setup_info
{
  /* Vorbis supports only short and long blocks, but allows the
     encoder to choose the sizes */

  long blocksizes[2];

  /* modes are the primary means of supporting on-the-fly different
     blocksizes, different channel mappings (LR or M/A),
     different residue backends, etc.  Each mode consists of a
     blocksize flag and a mapping (along with the mapping setup */

  int        modes;

  int        maps;
  int        floors;
  int        residues;
  int        books;
  int        psys;    /* encode only */

  vorbis_info_mode       *mode_param[64];

  /* ... following ignored info removed */ 

} some_codec_setup_info;

typedef struct vorbis_info_s vorbis_info_t;
struct vorbis_info_s {

   struct audio_info_s audioinfo;
   
   vorbis_info       vi;
   vorbis_comment    vc;
   vorbis_dsp_state  vd; /* central working state for packet->PCM decoder */
   vorbis_block      vb; /* local working state for packet->PCM decoder */

   some_codec_setup_info *some_csi; /* point to vi->codec_setp */
   int mode_bits;  /* when compute samples in packet */

   int head_packets;

   /* judge if audio packet started, as first audio packet has not ready samples */
   int audio_start;

   /* cache three header */
   char *header_identification;
   int   header_identification_len;

   char *header_comment;
   int   header_comment_len;

   char *header_setup;
   int   header_setup_len;
};
/*
int vorbis_capable_to_sdp(rtpcap_descript_t* rtpcap, sdp_message_t *sdp, media_info_t *info);
int vorbis_capable_from_sdp(rtpcap_descript_t* rtpcap, sdp_message_t *sdp, media_info_t *info);
*/
