/***************************************************************************
                          dev_portaudio.c  -  PortAudio Device
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
 
#include "dev_portaudio.h"
#include "timed_pipe.h"

#include <string.h>

#define PA_SAMPLE_TYPE  paInt16
#define PA_MUTE  0

#define DEFAULT_SAMPLE_FACTOR  2

#define DEFAULT_OUTPUT_NSAMPLE_PULSE 2048
#define DEFAULT_INPUT_NSAMPLE_PULSE  2048

#define PA_DEFAULT_INBUF_N 2
#define PA_DEFAULT_OUTBUF_N 2

/*
#define PORTAUDIO_LOG
*/
#define PORTAUDIO_DEBUG

#ifdef PORTAUDIO_LOG
 #define pa_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define pa_log(fmtargs)
#endif

#ifdef PORTAUDIO_DEBUG
 #define pa_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define pa_debug(fmtargs)
#endif

static int pa_input_callback( void *inbuf, void *outbuf, unsigned long npcm_in,
								      PaTimestamp intime, void *udata )
{
   int next;

   portaudio_device_t* pa = (portaudio_device_t*)udata;

   pa->input_samplestamp += npcm_in;

   next = (pa->inbuf_w+1) % pa->inbuf_n;
   if(next == pa->inbuf_r)
   {
	   /* encoding slower than input */
	   return 0;
   }

   memcpy(pa->inbuf[pa->inbuf_w].pcm, inbuf, npcm_in);

   pa->inbuf[pa->inbuf_w].stamp = pa->input_samplestamp;
   pa->inbuf[pa->inbuf_w].npcm_write = npcm_in;
   pa->inbuf_w = next;

   return 0;
}

int pa_input_loop(void *gen)
{
	int err;
	int sample_bytes;
	pa_input_t* inbufr;

   media_frame_t auf;
	portaudio_device_t* pa = (portaudio_device_t*)gen;

	if (!pa->receiver)
      return MP_FAIL;

	sample_bytes = pa->ai_input.info.sample_bits / OS_BYTE_BITS;
	pa_debug(("pa_input_loop: PortAudio started, %d bytes/sample\n\n", sample_bytes));

	pa->input_stop = 0;

	while(!pa->input_stop)
	{
		rtime_t us_left;

		inbufr = &pa->inbuf[pa->inbuf_r];

		if(inbufr->npcm_write == 0)
		{
			/* encoding catch up input */
			time_usec_sleep(pa->clock, (pa->usec_pulse / 2), &us_left);
			continue;
		}

		auf.ts = (int32)inbufr->stamp;
		auf.samplestamp = inbufr->stamp;

		auf.raw = inbufr->pcm;
		auf.nraw = pa->io_npcm_once;
		auf.bytes = pa->io_nbyte_once;
      
		auf.eots = 1;
      auf.sno = inbufr->tick;

		pa->receiver->receive_media(pa->receiver, &auf, (int64)(auf.samplestamp), 0);

		memset(inbufr->pcm, 0, pa->io_nbyte_once);
      
		inbufr->npcm_write = 0;

		pa->inbuf_r = (pa->inbuf_r + 1) % pa->inbuf_n;
	}

	auf.bytes = 0;
	auf.nraw = 0;
	auf.raw = pa->inbuf[pa->inbuf_r].pcm;

	pa->receiver->receive_media(pa->receiver, &auf, (int64)(pa->inbuf[pa->inbuf_r].stamp * 1000000), MP_EOS);

	err = Pa_StopStream ( pa->pa_outstream );

	if( err != paNoError )
	{
		Pa_Terminate();
		pa_debug(("pa_input_loop: An error occured while stop the portaudio\n"));

		return MP_FAIL;
	}

	return MP_OK;
}

static int pa_io_callback( void *inbuf, void *outbuf, unsigned long npcm, PaTimestamp intime, void *udata )
{
   portaudio_device_t* pa = (portaudio_device_t*)udata;

	pa_input_t* inbufw;
	pa_output_t* outbufr;

   int end = 1;
   
   /* Record input data */
   if(inbuf)
   {
		pa->input_samplestamp += npcm;

      inbufw = &pa->inbuf[pa->inbuf_w];

      if(inbufw->npcm_write != 0)
		{
			/* encoding slower than input */
			pa_log(("\rpa_io_callback: inbuf#%d ... input full\n", pa->inbuf_w));
		}
		else
		{
		   pa_debug(("\rpa_io_callback: inbuf#%d ... record\n", pa->inbuf_w));
			memcpy(inbufw->pcm, inbuf, pa->io_nbyte_once);

         inbufw->stamp = pa->input_samplestamp;
			inbufw->npcm_write = npcm;
         inbufw->tick = pa->input_tick;
         
			pa->inbuf_w = (pa->inbuf_w+1) % pa->inbuf_n;
		}

      pa->input_tick++;
   }

   if(outbuf)
   {
      outbufr = &pa->outbuf[pa->outbuf_r];

      if(outbufr->nbyte_wrote != pa->io_nbyte_once)
		{
			/* encoding slower than input */
			pa_debug(("\rpa_io_callback: outbuf#%d ... output empty\n", pa->outbuf_r));
			memset(outbuf, PA_MUTE, pa->io_nbyte_once);
         end = 0;
		}
		else
		{
		   pa_debug(("\rpa_io_callback: outbuf#%d ... play\n", pa->outbuf_r));
			memcpy(outbuf, outbufr->pcm, pa->io_nbyte_once);

			outbufr->nbyte_wrote = 0;
         end = outbufr->last;

			pa->outbuf_r = (pa->outbuf_r+1) % pa->outbuf_n;
		}
   }

   return end;
}

int pa_done_setting(control_setting_t *gen)
{
   xfree(gen);

   return MP_OK;
}

control_setting_t* pa_new_setting(media_device_t *dev)
{
   pa_setting_t * set = xmalloc(sizeof(struct pa_setting_s));
   if(!set){

      pa_debug(("rtp_new_setting: No memory"));

      return NULL;
   }
   memset(set, 0, sizeof(struct pa_setting_s));

   set->setting.done = pa_done_setting;

   return (control_setting_t*)set;
}

media_pipe_t * pa_pipe(media_device_t *dev) 
{
   return ((portaudio_device_t *)dev)->out;
}

int pa_online (media_device_t * dev)
{
   PaError err;

   portaudio_device_t *pa_dev = (portaudio_device_t *)dev;

   if (pa_dev->online) return MP_OK;

   err = Pa_Initialize();
   if ( err != paNoError )
   {
      pa_debug(("pa_online: %s\n", Pa_GetErrorText(err) ));
      Pa_Terminate();

      return MP_FAIL;
   }

   pa_dev->online = 1;

   pa_log(("pa_online: portaudio initialized\n"));
   
   return MP_OK;
}

int pa_sample_type (int sample_bits)
{
   switch (sample_bits)
   {
      case 16:
      
         pa_log(("pa_sample_type: 16 bits integer value\n"));
         return paInt16;

      case 32:

         pa_log(("pa_sample_type: 32 bits float value\n"));
         return paFloat32;
   }

   pa_log(("pa_sample_type: apply default 16 bits integer value\n"));
   
   return paInt16;

}

int pa_make_inbuf(portaudio_device_t *pa, audio_info_t *ai_input, int inbuf_n)
{
   int i;

   pa->inbuf_n = inbuf_n;

   pa->input_samplestamp = 0;
   pa->inbuf = xmalloc(sizeof(pa_input_t) * pa->inbuf_n);
   memset(pa->inbuf, 0, sizeof(pa_input_t) * pa->inbuf_n);

   for(i=0; i<pa->inbuf_n; i++)
   {
      pa->inbuf[i].pcm = xmalloc(pa->io_nbyte_once);
      memset(pa->inbuf[i].pcm, 0, pa->io_nbyte_once);
   }

   return MP_OK;   
}

int pa_done_inbuf(portaudio_device_t *pa)
{
   int i;

   for(i=0; i<pa->inbuf_n; i++)
      xfree(pa->inbuf[i].pcm);

   xfree(pa->inbuf);

   return MP_OK;
}

int pa_make_outbuf(portaudio_device_t *pa, audio_info_t *ai_output, int outbuf_n)
{
   int i;

   pa->outbuf_n = outbuf_n;

   pa->outbuf = xmalloc(sizeof(pa_output_t) * pa->outbuf_n);
   memset(pa->outbuf, 0, sizeof(pa_output_t) * pa->outbuf_n);

   for(i=0; i<pa->outbuf_n; i++)
   {
      pa->outbuf[i].pcm = xmalloc(pa->io_nbyte_once);
      memset(pa->outbuf[i].pcm, 0, pa->io_nbyte_once);
   }

   pa->outbuf_nbyte_read = 0;
   
   return MP_OK;
}

int pa_done_outbuf(portaudio_device_t *pa)
{
   int i;

   for(i=0; i<pa->outbuf_n; i++)
      xfree(pa->outbuf[i].pcm);

   xfree(pa->outbuf);

   return MP_OK;
}

int pa_set_input_media(media_device_t *dev, media_receiver_t* recvr, media_info_t *in_info)
{
   portaudio_device_t *pa = (portaudio_device_t *)dev;
   
   //int nsp = 0;
   int nsample_pulse = 0;

   int sample_type = 0;

   PaError err = 0;

   audio_info_t* ai = (audio_info_t*)in_info;

   if(!pa->online && pa_online (dev) < MP_OK)
   {

      pa_debug (("pa_set_input_media: Can't activate PortAudio device\n"));
      return MP_FAIL;
   }
   
   pa->ai_input = *ai;

   /*
   nsp = pa->ai_input.info.sample_rate / pa->sample_factor;
   nsp = pa->ai_input.info.sample_rate / pa->sample_factor;
   nsample_pulse = 1;
   while (nsample_pulse * 2 <= nsp)
	   nsample_pulse *= 2;
   */
   nsample_pulse = DEFAULT_OUTPUT_NSAMPLE_PULSE;

   if(pa->ai_input.info.sample_rate > nsample_pulse)
      pa->input_usec_pulse = (int)(1000000 / (pa->ai_input.info.sample_rate / (double)nsample_pulse));
   else

      pa->input_usec_pulse = (int)((double)nsample_pulse / pa->ai_input.info.sample_rate * 1000000);
      
   pa_log(("pa_set_input_media: %d channels, %d rate, %d sample per pulse (%dus)\n", 
			pa->ai_input.channels, pa->ai_input.info.sample_rate, nsample_pulse, pa->usec_pulse));
   
   pa->ai_input.channels_bytes = pa->ai_input.channels * pa->ai_input.info.sample_bits / OS_BYTE_BITS;

   pa->io_npcm_once = ai->info.sampling_constant;
   pa->io_nbyte_once = pa->ai_input.channels_bytes * ai->info.sampling_constant;
   
   sample_type = pa_sample_type(pa->ai_input.info.sample_bits);
   
   err = Pa_OpenStream 
					(	&pa->pa_instream,
						Pa_GetDefaultInputDeviceID(),
						pa->ai_input.channels,
						sample_type,
						NULL,				/* void *inputDriverInfo */
						paNoDevice,
						0,					/* int numOutputChannels */
						sample_type,		/* PaSampleFormat outputSampleFormat */
						NULL,
						pa->ai_input.info.sample_rate,
						nsample_pulse,		/* unsigned long framesPerBuffer */
						pa->nbuf_internal,	/* unsigned long numberOfBuffers */
						0,					/* PaStreamFlags streamFlags: paDitherOff */
						pa_input_callback,
						pa 
					);

   if ( err != paNoError )
   {
      pa_debug(("pa_set_input_media: %s\n", Pa_GetErrorText(err) ));
      Pa_Terminate();

      return MP_FAIL;
   }

   pa->receiver = recvr;
   
   pa_make_inbuf(pa, ai, PA_DEFAULT_INBUF_N);
      
   pa_log(("\n[pa_set_input_media: PortAudio use %d internal buffers]\n\n", Pa_GetMinNumBuffers (nsample_pulse, pa->ai_input.info.sample_rate)));

   /* Start PortAudio */
   return MP_OK;
}

int pa_set_output_media(media_device_t *dev, media_info_t *out_info)
{
   portaudio_device_t *pa = (portaudio_device_t *)dev;


   /*int nsp = 0;*/
   int nsample_pulse = 0;
   int sample_type = 0;

   PaError err = 0;
   int pa_min_nbuf = 0;

   audio_info_t* ai = (audio_info_t*)out_info;

   if(!pa->online && pa_online (dev) < MP_OK)
   {
      pa_debug (("pa_set_input_media: Can't activate PortAudio device\n"));

      return MP_FAIL;
   }

   pa->ai_output = *ai;

   /*
   nsp = pa->ai_input.info.sample_rate / pa->sample_factor;
   nsample_pulse = 1;
   while (nsample_pulse * 2 <= nsp)
	   nsample_pulse *= 2;
   */

   /* *
    * It looks to set 2048 per pulse will get a more smooth audio out result
    * for both 16000 and 8000 rate.
    */

   nsample_pulse = DEFAULT_OUTPUT_NSAMPLE_PULSE;

   if(pa->ai_output.info.sample_rate > nsample_pulse)
      pa->usec_pulse = (int)(1000000 / (pa->ai_output.info.sample_rate / (double)nsample_pulse));
   else
      pa->usec_pulse = (int)((double)nsample_pulse / pa->ai_output.info.sample_rate * 1000000);

   pa_debug(("pa_set_output_media: %d channels, %d rate, %d sample per pulse (%dus)\n",
			pa->ai_output.channels, pa->ai_output.info.sample_rate, nsample_pulse, pa->usec_pulse));

   pa->ai_output.channels_bytes = pa->ai_output.channels * pa->ai_output.info.sample_bits / OS_BYTE_BITS;

   pa->out = pa_pipe_new(pa);

   sample_type = pa_sample_type(pa->ai_output.info.sample_bits);

   err = Pa_OpenDefaultStream
                  (  &pa->pa_outstream,

                     0,                        /* no input channels */
                     pa->ai_output.channels,   /* output channels */
                     sample_type,              /* paInt16: 16 bit integer output, etc */
                     pa->ai_output.info.sample_rate,  /* sample rate, cd is 44100hz */
                     nsample_pulse,
                     pa->nbuf_internal,        /* use default minimum audio buffers in portaudio */
                     pa_io_callback,
                     pa
                  );

   if ( err != paNoError )
   {
      pa_debug(("pa_set_output_media: %s\n", Pa_GetErrorText(err) ));
      Pa_Terminate();

      return MP_FAIL;
   }

	pa_min_nbuf = Pa_GetMinNumBuffers (nsample_pulse, pa->ai_output.info.sample_rate);

	pa_debug(("\n[pa_set_output_media: stream opened, PortAudio use %d internal buffers]\n\n", pa_min_nbuf));

	err = Pa_StartStream ( pa->pa_outstream );

	if( err != paNoError )
	{
		Pa_Terminate();
		pa_debug(("pa_set_output_media: An error occured while using the portaudio stream\n"));

		pa->online = 0;
		return MP_FAIL;
	}

	return MP_OK;

}

int pa_set_io(media_device_t *dev, media_info_t *minfo, media_receiver_t* recvr)
{
   portaudio_device_t *pa = (portaudio_device_t *)dev;

   /*int nsp = 0;*/
   int nsample_pulse = 0;
   int sample_type = 0;

   PaError err = 0;
   int pa_min_nbuf = 0;              

   audio_info_t* ai = (audio_info_t*)minfo;

   if(!pa->receiver && recvr)
      pa->receiver = recvr;

   if(pa->io_ready)
      return MP_OK;

   if(!pa->online && pa_online (dev) < MP_OK)
   {
      pa_debug (("pa_set_io: Can't activate PortAudio device\n"));

      return MP_FAIL;
   }

   nsample_pulse = DEFAULT_OUTPUT_NSAMPLE_PULSE;
   ai->info.sampling_constant = DEFAULT_OUTPUT_NSAMPLE_PULSE;

   if(ai->info.sample_rate > nsample_pulse)
      pa->usec_pulse = (int)(1000000 / (ai->info.sample_rate / (double)nsample_pulse));
   else
      pa->usec_pulse = (int)((double)nsample_pulse / ai->info.sample_rate * 1000000);

   pa_debug(("pa_set_io: %d channels, %d rate, %d sample per pulse (%dus)\n",
			ai->channels, ai->info.sample_rate, nsample_pulse, pa->usec_pulse));

   ai->channels_bytes = ai->channels * ai->info.sample_bits / OS_BYTE_BITS;

   pa->io_npcm_once = nsample_pulse;
   pa->io_nbyte_once = ai->channels_bytes * nsample_pulse;

	pa_debug(("pa_set_io: %d bytes once\n", pa->io_nbyte_once));

   pa->out = pa_pipe_new(pa);

   sample_type = pa_sample_type(ai->info.sample_bits);

   pa->ai_output = *ai;
   pa->ai_input = *ai;
   
   pa_make_inbuf(pa, &pa->ai_input, PA_DEFAULT_INBUF_N);
   pa_make_outbuf(pa, &pa->ai_output, PA_DEFAULT_OUTBUF_N);

   err = Pa_OpenDefaultStream
                  (  &pa->pa_iostream,
                     ai->channels,   /* input channels */
                     ai->channels,   /* output channels */
                     sample_type,              /* paInt16: 16 bit integer output, etc */
                     ai->info.sample_rate,  /* sample rate, cd is 44100hz */
                     nsample_pulse,
                     pa->nbuf_internal,        /* use default minimum audio buffers in portaudio */
                     pa_io_callback,
                     pa
                  );
#if 0
   err = Pa_OpenStream 
					(	&pa->pa_iostream,
						Pa_GetDefaultInputDeviceID(),
						pa->ai_input.channels,
						sample_type,
						NULL,				/* void *inputDriverInfo */
						Pa_GetDefaultOutputDeviceID(),
						pa->ai_output.channels,	/* int numOutputChannels */
						sample_type,		/* PaSampleFormat outputSampleFormat */
						NULL,				/* void *outputDriverInfo */
						pa->ai_input.info.sample_rate,
						nsample_pulse,		/* unsigned long framesPerBuffer */
						pa->nbuf_internal,	/* unsigned long numberOfBuffers */
						0,					/* PaStreamFlags streamFlags: paDitherOff */

						pa_io_callback,
						pa 

					);
#endif

   if ( err != paNoError )
   {
      pa_debug(("pa_set_io: %s\n", Pa_GetErrorText(err) ));
      Pa_Terminate();

      return MP_FAIL;
   }

	pa_min_nbuf = Pa_GetMinNumBuffers (nsample_pulse, ai->info.sample_rate);

   pa->io_ready = 1;

	pa_debug(("\n[pa_set_io: stream opened, PortAudio use %d internal buffers]\n\n", pa_min_nbuf));

   return MP_OK;
}

int pa_init(media_device_t * dev, media_control_t *ctrl)
{
   return MP_OK;
}

int pa_start_io(media_device_t * dev, int mode)
{
	int err;

	portaudio_device_t *pa = (portaudio_device_t *)dev;

   if((mode & DEVICE_INPUT) && pa->input_thread)
      return MP_OK;

   if((mode & DEVICE_INPUT) && dev->running)
      mode = DEVICE_INPUT;
   
	if (!pa->pa_iostream)
	{
		pa_debug(("pa_start_io: No Audio I/O\n"));
		return MP_FAIL;
	}

	err = Pa_StartStream (pa->pa_iostream);


	if( err != paNoError )
	{
	   Pa_Terminate();
	   pa_debug(("pa_start_io: An error occured while using the portaudio stream\n"));

	   pa->online = 0;
	   return MP_FAIL;
	}
      
	pa_debug(("pa_start_io: PortAudio io started\n"));

	/* Start Audio Input */
	if((mode & DEVICE_INPUT) && pa->receiver)
   {
		pa->input_thread = xthr_new (pa_input_loop, pa, XTHREAD_NONEFLAGS);
   }

   dev->running = 1;

	return MP_OK;
}

int pa_stop_io(media_device_t * dev, int mode)
{
   int inloop_ret;

   PaError err = 0;
	portaudio_device_t *pa_dev = (portaudio_device_t *)dev;

	if (!dev->running || !pa_dev->pa_iostream)
	{
      pa_debug(("pa_stop: PortAudio is idled\n"));
		return MP_OK;
	}

   if((mode == DEVICE_INPUT) && !pa_dev->input_thread)
      return MP_OK;

   if((mode == DEVICE_OUTPUT) && !dev->running)
      return MP_OK;

	/* Stop PortAudio Input */
	if(mode & DEVICE_INPUT && pa_dev->input_thread)
   {
	   pa_dev->input_stop = 1;
      xthr_wait(pa_dev->input_thread, &inloop_ret);
   }

	if(mode & DEVICE_OUTPUT && dev->running)
	{
		err = Pa_StopStream (pa_dev->pa_iostream);
		if( err != paNoError )
		{
			Pa_Terminate();

			pa_debug(("pa_stop: An error occured while stop the portaudio\n"));

			pa_dev->online = 0;
			return MP_FAIL;
		}

      dev->running = 0;
	}
   
	pa_debug(("pa_stop: PortAudio stopped\n"));

	return MP_OK;
}

int pa_offline (media_device_t * dev) {

   portaudio_device_t *pa_dev = NULL;

   if (dev->running)
      pa_stop_io(dev, DEVICE_INPUT|DEVICE_OUTPUT);

   Pa_Terminate();
   pa_debug(("pa_stop: portaudio terminated.\n"));

   pa_dev = (portaudio_device_t *)dev;

   pa_dev->online = 0;

   return MP_OK;
}

int pa_done (media_device_t * dev) 
{
   portaudio_device_t *pa_dev = (portaudio_device_t *)dev;
   
   if (pa_dev->online)
      pa_offline (dev);

   if (pa_dev->out)
      pa_dev->out->done (pa_dev->out);
   
   if(pa_dev->clock)
      time_end(pa_dev->clock);

   pa_done_inbuf(pa_dev);

   
   xfree(pa_dev);
   
   return MP_OK;
}

int pa_setting (media_device_t * dev, control_setting_t *setting, module_catalog_t *cata) {

   /* module_catalog is not used, as no more module needed */
   
   portaudio_device_t * pa = (portaudio_device_t *)dev;
   
   if (setting)
   {
      pa_setting_t *pa_setting = (pa_setting_t *)setting;      
      pa->nbuf_internal = pa_setting->nbuf_internal;
      pa->inbuf_n = pa_setting->inbuf_n;
   }

   return MP_OK;
}

int pa_match_mode(media_device_t *dev, char *mode)
{
   pa_log(("portaudio.pa_match_type: input/output device\n"));

   if( !strcmp("output", mode) ) return 1;
   if( !strcmp("input", mode) ) return 1;
   if( !strcmp("inout", mode) ) return 1;

   return 0;
}

int pa_match_type(media_device_t *dev, char *type)
{
   pa_log(("portaudio.pa_match_type: I am audio device\n"));

   if( !strcmp("audio", type) ) return 1;

   return 0;
}

module_interface_t* media_new_device ()
{
   media_device_t *dev = NULL;

   portaudio_device_t * pa = xmalloc (sizeof(struct portaudio_device_s));
   if (!pa)
   {

      pa_debug(("media_new_device: No enough memory\n"));

      return NULL;
   }
   
   memset(pa, 0, sizeof(struct portaudio_device_s));

   pa->clock = time_start();

   dev = (media_device_t *)pa;

   dev->pipe = pa_pipe;

   dev->init = pa_init;   

   dev->start = pa_start_io;
   dev->stop = pa_stop_io;
   
   dev->done = pa_done;

   dev->set_io = pa_set_io;
   dev->set_input_media = pa_set_input_media;
   dev->set_output_media = pa_set_output_media;
   
   dev->new_setting = pa_new_setting;
   dev->setting = pa_setting;
   
   dev->match_type = pa_match_type;
   dev->match_mode = pa_match_mode;

   if ( pa_online(dev) >= MP_OK) 
	   pa->online = 1;

   return dev;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC module_loadin_t mediaformat = 
{
   "device",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   media_new_device   /* Module initializer */
};
