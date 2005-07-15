/***************************************************************************
                          timed_outpipe.h  -  Pipe out on time
                             -------------------
    begin                : Wed Feb 4 2004
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

#include <timedia/timer.h>

typedef struct sample_buffer_s sample_buffer_t;
struct sample_buffer_s
{
   #define WRITE_AUDIO        1
   #define READ_AUDIO         2
   #define CRITICAL_HANDLE    3
   #define RECYCLE_BUFFER     4

   int state;           /* The buffer state: WRITE_AUDIO, READ_AUDIO, RECYCLE_BUFFER */

   int usec_in;         /* buffer duration, in microsecond resolution */
   int usec_out;        /* buffer played */
   int usec_mute;       /* mute played */

   int nsample_prepick;
   int usec_prepick;

   int samples_in;      /* my own sample */
   int samples_send;    /* send back to read buffer */
   int samples_out;     /* samples played */
   int samples_mute;    /* mute played */

   /* double buffer */

   media_frame_t *oldhead;

   media_frame_t *frame_head;
   media_frame_t *frame_tail;
};
