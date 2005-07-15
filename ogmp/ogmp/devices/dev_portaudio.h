/***************************************************************************
                          device.h  -  Input/Output
                             -------------------
    begin                : Tue Feb 3 2004
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
#include <portaudio.h>
#include <timedia/os.h>
#include <timedia/ui.h>
#include <timedia/xthread.h>
#include <timedia/xmalloc.h>

#define PIPE_NBUF 3  /* write-read-recycle */

typedef struct pa_setting_s pa_setting_t;
struct pa_setting_s
{
   struct control_setting_s setting;
   
   int time_match;
   int sample_factor;    /* the factor b/w sample rate & nsample_pulse, power of 2 */
   int nbuf_internal;    /* PortAudio Internal Buffer number, for performance tuning */
   
   int inbuf_n;          /* input buffer number */
};

typedef struct pa_input_s pa_input_t;
struct pa_input_s
{
	int64 stamp;
	int npcm_write;
	char *pcm;
   int tick;
};

typedef struct pa_output_s pa_output_t;
struct pa_output_s
{
   int last;
   int nbyte_wrote;
	char *pcm;
};

typedef struct portaudio_device_s
{
   struct media_device_s dev;

   PortAudioStream *pa_iostream;
   int io_ready;

   /* Input */
   audio_info_t ai_input;
   media_receiver_t* receiver;

   int inbuf_n;
   int inbuf_r;
   int inbuf_w;

   pa_input_t *inbuf;
   int input_tick;

   int input_stop;
   xthread_t *input_thread;
   PortAudioStream *pa_instream;

   int input_usec_pulse;

   /* Output */
   int outbuf_n;
   int outbuf_r;
   int outbuf_w;
   int outbuf_nbyte_cache;
   char *outbuf_cache;
   pa_output_t *outbuf;

   media_pipe_t * out;
   audio_info_t ai_output;

   PortAudioStream *pa_outstream;

   xclock_t *clock;
   int nbuf_internal;   /* PortAudio Internal Buffer number, for performance tuning */

   int usec_pulse;

   int online;

   int io_npcm_once;
   int io_nbyte_once;

   int64 input_samplestamp;

} portaudio_device_t;

typedef struct pa_pipe_s pa_pipe_t;
struct pa_pipe_s
{
   struct media_pipe_s pipe;

   portaudio_device_t *pa;

   int bytes_allbuf;
   int bytes_freed;

   int n_frame;

   media_frame_t *freed_frame_head; /* The frame to reuse, avoid malloc/free ops */
   int n_freed_frame;   
};

media_pipe_t * pa_pipe_new(portaudio_device_t* pa);

