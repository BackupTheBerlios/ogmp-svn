/***************************************************************************
                          vorbis_player.h  -  Vorbis stream player
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

#include "vorbis_info.h"
#include <timedia/xthread.h>

#define VORBIS_SAMPLE_BITS 16  /* See vorbis_decode() */

typedef struct vorbis_decoder_s vorbis_decoder_t;
struct vorbis_decoder_s{

   struct media_player_s player;

   vorbis_info_t *vorbis_info;

   int receiving_media;

   int stream_opened;

   int ts_usec_now;
   int64 last_samplestamp;

   /* Failure occurred */
   int fail;

   /* Delay Adapt Detect: if the buffer fall into this range
    * special process will activated to smooth the effect
	*
	* No in use yet.
    */
   int dad_min_ms;
   int dad_max_ms;

   int (*callback_on_ready) (void *user, ...);
   void *callback_on_ready_user;

   int (*stop_media) (void * user);
   void * stop_media_user;

   /* parallel with demux thread */
   xthread_t *thread;
   /* stop the thread */
   int stop;
   /* queue protected by lock*/
   xlist_t *pending_queue;
   xthr_lock_t *pending_lock;
   /* thread running condiftion */
   xthr_cond_t *packet_pending;
};

media_frame_t * vorbis_decode (vorbis_info_t *vinfo, ogg_packet * packet, media_pipe_t * output);
