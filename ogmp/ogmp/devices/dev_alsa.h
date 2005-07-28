/*
  The mediastreamer library aims at providing modular media processing and I/O
	for linphone, but also for any telephony application.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org
  										
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <alsa/asoundlib.h>

#include <timedia/xthread.h>

#include "media_format.h"
#include "sndcard.h"

#define PIPE_NBUF 2

#define ALSA_PCM_NEW_HW_PARAMS_API
typedef struct _AlsaCard
{
	SndCard parent;
   
	char *pcmdev;
	char *mixdev;
   
	snd_pcm_t *read_handle;
	snd_pcm_t *write_handle;
   
	int frame_size;
	int frames;
	char *readbuf;
	int readpos;
	char *writebuf;
	int writepos;
   
	snd_mixer_t *mixer;

} AlsaCard;

SndCard *alsa_card_new (int dev_id);
int alsa_card_manager_init(SndCardManager *m, int index);
void alsa_card_manager_set_default_pcm_device(char *pcmdev);

typedef struct alsa_device_s
{
   struct media_device_s dev;
   
   /* SoundCard Manager */
   SndCardManager *sndcard_manager;
   SndCard *sndcard;
   int devid;

   int usec_pulse;
   
   /* Input */
   audio_info_t *ai_input;
   
   int input_npcm_once;
   int input_nbyte_once;
   int npcm_input;
   char *pcm_input;

   int input_stop;
   
   xthread_t *input_thread;
   media_receiver_t* receiver;

   /* Output */
   audio_info_t *ai_output;
   int output_npcm_once;
   int output_nbyte_once;
   
   int outframe_npcm_done;
   int outframe_w;
   int outframe_r;
   int picking;
   
   media_frame_t *outframes[PIPE_NBUF];
   
   media_pipe_t * src_pipe;

} alsa_device_t;

typedef struct alsa_pipe_s alsa_pipe_t;
struct alsa_pipe_s
{
   struct media_pipe_s pipe;

   alsa_device_t *alsa;

   int bytes_allbuf;
   int bytes_freed;

   int n_frame;

   media_frame_t *freed_frame_head; /* The frame to reuse, avoid malloc/free ops */
   int n_freed_frame;
};

media_pipe_t * alsa_new_pipe(alsa_device_t* pa);

#ifdef HAVE_ALSA_ASOUNDLIB_H
#endif
