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


#include "../media_format.h"

#include <timedia/timer.h>

#define PIPE_NBUF 3  /* write-read-recycle */

typedef struct sample_buffer_s sample_buffer_t;
struct sample_buffer_s {

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

#define PIPE_STATE_IDLE       0
#define PIPE_STATE_BUFFERING  1
#define PIPE_STATE_PIPING    2

typedef struct timed_pipe_s timed_pipe_t;
struct timed_pipe_s {

   struct media_pipe_s pipe;

   xrtp_clock_t *clock;

   xrtp_lrtime_t lts_last;
   
   int eots;
   
   int pulsing;

   int switch_buffer;   /* buffer number plus 1. zero value means no jump */
   int buffered;        /* enough data to play */

   int bufn_write;
   int bufn_read;
   int bufn_recyc;

   struct sample_buffer_s buffer[PIPE_NBUF];

   media_frame_t *last_played_frame;
   media_frame_t *frame_read_now;
   
   int bytes_allbuf;
   int bytes_freed;

   media_frame_t *freed_frame_head; /* The frame to reuse, avoid malloc/free ops */
   int n_freed_frame;

   int usec_pulse;      /* constant pickup interval */

   int usec_per_buf;

   int nsample_read_left;
   int nsample_write_left;

   int samples_last_buffered;

   int buffer_dyna_usec;

   char *fillgap;

   int sample_rate;
   int dad_min_nsample, dad_max_nsample;
};

media_pipe_t * timed_pipe_new(int sample_rate, int usec_pulse, int us_min, int us_max);
