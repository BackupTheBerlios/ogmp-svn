/***************************************************************************
                          ogm_format.h  -  Ogg Media Format definition
                             -------------------
    begin                : Mon Jan 19 2004
    copyright            : (C) 2004 by Heming Ling
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*
  The code is the inspiration to ogmtool. Please visit
  the origional project site for the detail:

  http://www.bunkus.org/videotools/ogmtools/

  ogmdemux -- utility for extracting raw streams from an OGG media file

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef OGM_FORMAT_H

#define OGM_FORMAT_H

#include "../media_format.h"

#include <stdio.h>
#include <ogg/ogg.h>

#include "ogm_streams.h"
#include "ogm_common.h"

#include <timedia/timer.h>

#define BLOCK_SIZE 4096

#define ACVORBIS 0xffff
#define ACPCM    0x0001
#define ACMP3    0x0055
#define ACAC3    0x2000

#define CHUNKSIZE                8500
#define PACKET_TYPE_HEADER       0x01
#define PACKET_TYPE_COMMENT      0x03
#define PACKET_TYPE_CODEBOOK     0x05
#define PACKET_TYPE_BITS	 0x07
#define PACKET_LEN_BITS01        0xc0
#define PACKET_LEN_BITS2         0x02
#define PACKET_IS_SYNCPOINT      0x08

typedef struct ogm_stream_s ogm_stream_t;
typedef struct ogm_format_s ogm_format_t;
typedef struct ogm_media_s ogm_media_t;

struct ogm_stream_s
{
   struct  media_stream_s  stream;
   
   int                     serial;   /* stream no */

   struct stream_header    header;
   int                     header_granulepos;

   int                     millisec;
   int                     last_microsec;
   ogg_int64_t             last_granulepos;

   int                     factor;  /* Video 90000; Audio: 8000; etc */

   int                     comment;

   int                     sno;      /* subtype no */
   char                    stype;

   ogg_stream_state        *instate;
   ogg_stream_state        outstate;

   int                     acodec;

   ogg_int64_t             bwritten;

   int                     packetno;
   ogg_int64_t             max_granulepos;

   int                     subnr;
};

struct ogm_format_s
{
   struct  media_format_s  format;

   ogg_sync_state       sync;

   ogg_page             page;
   int                  page_ready;

   ogg_packet           packet;
   int                  packet_ready;

   uint64               last_pts;
};

struct ogm_media_s
{
   int (*done) (ogm_media_t * ogmm);

   /* Detect a media type from ogg packet */
   int (*detect_media) (ogg_packet *packet);

   int (*open_media) (ogm_media_t * handler, ogm_format_t *ogm, media_control_t *ctrl, ogg_stream_state *sstate, int sno, stream_header *sth);

   int (*process_media) (ogm_format_t * ogm, ogm_stream_t *ogm_strm, ogg_page *page, ogg_packet *pack, int hdrlen, int64 lenbytes, int64 samplestamp, int last_packet, int stream_end);
};

/* New detected stream add to group */
int ogm_add_stream(media_format_t * ogm, media_stream_t *strm, int strmno, unsigned char type);

#endif
