/***************************************************************************
                          fifo.h  -  media packet buffer
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

#include "media_format.h"
#include <timedia/xthread.h>

#define MP_FIFO_NONEFLAG		0x0
#define MP_FIFO_END_GROUP		0x1
#define MP_FIFO_END_ALL			0x2

typedef struct fifo_s fifo_t;
typedef struct fifo_frame_s fifo_frame_t;

struct fifo_frame_s {

   struct media_frame_s frame;

   uint		flag;
   int64	samplestamp;
   void		*media_info;
   
   long		unit_bytes;
   char		*media_unit;
};

struct fifo_s {

   struct media_pipe_s pipe;

   int no_more;

   /* queue protected by lock*/
   xlist_t *queue;
   xthr_lock_t *queue_lock;
   /* thread running condiftion */
   xthr_cond_t *in_queue;
};
