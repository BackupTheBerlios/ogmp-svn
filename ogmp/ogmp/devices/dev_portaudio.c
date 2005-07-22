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

#define DEFAULT_OUTPUT_NSAMPLE_PULSE 1024
#define DEFAULT_INPUT_NSAMPLE_PULSE  1024

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

int pa_input_loop(void *gen)
{
	int err;
	int sample_bytes;
	pa_input_t* inbufr;

   media_frame_t auf;
	portaudio_device_t* pa = (portaudio_device_t*)gen;

	if (!pa->receiver)
      return MP_FAIL;

	sample_bytes = pa->ai_input->info.sample_bits / OS_BYTE_BITS;
	pa_debug(("pa_input_loop: PortAudio started, %d bytes/sample\n\n", sample_bytes));

	pa->input_stop = 0;

	while(!pa->input_stop)
	{
		rtime_t us_left;

		inbufr = &pa->inbuf[pa->inbuf_r];

		if(inbufr->nbyte_wrote != pa->input_nbyte_once)
		{
			/* encoding catch up input */
			time_usec_sleep(pa->clock, (pa->usec_pulse / 2), &us_left);
			continue;
		}

		auf.ts = (int32)inbufr->stamp;
		auf.samplestamp = inbufr->stamp;

		auf.raw = inbufr->pcm;
		auf.nraw = pa->input_npcm_once;
		auf.bytes = pa->input_nbyte_once;
      
		auf.eots = 1;
      auf.sno = inbufr->tick;

	   pa_debug(("\rpa_input_loop: in[%d] - frame#%lld[%lld] bytes[%d]\n", pa->inbuf_r, auf.sno, auf.samplestamp, auf.bytes));
		pa->receiver->receive_media(pa->receiver, &auf, (int64)(auf.samplestamp), 0);
	   pa_debug(("\rpa_input_loop: in[%d] ========================= done\n", pa->inbuf_r));

		memset(inbufr->pcm, 0, pa->input_nbyte_once);      
		inbufr->nbyte_wrote = 0;

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

static int pa_io_callback (const void *inbuf, void *outbuf,
                           unsigned long npcm,
                           const PaStreamCallbackTimeInfo* intime,
                           PaStreamCallbackFlags statusFlags,
                           void *udata )
{
   portaudio_device_t* pa = (portaudio_device_t*)udata;

   pa_input_t* inbufw;
   int nbyte_tofill;

   int pcm_nbyte = pa->ai_input->info.sample_bits / OS_BYTE_BITS;
   int nbyte_npcm = npcm * pcm_nbyte;

   /* Record input data */
   if(!inbuf)
   	pa_debug(("\rpa_io_callback: no input\n"));
   else if(0 != (statusFlags & paInputUnderflow))
   	pa_debug(("\rpa_io_callback: inbuf#%d input underflow\n", pa->inbuf_w));
   else if(0 != (statusFlags & paInputOverflow))
   	pa_debug(("\rpa_io_callback: inbuf#%d input overflow\n", pa->inbuf_w));
   else
   {
      while(nbyte_npcm)
      {
         inbufw = &pa->inbuf[pa->inbuf_w];

         if(inbufw->nbyte_wrote == pa->input_nbyte_once)
         {
			   pa_log(("\rpa_io_callback: inbuf#%d ... input overflow, discard\n", pa->inbuf_w));
            break;
         }
         else
         {
            nbyte_tofill = pa->input_nbyte_once - inbufw->nbyte_wrote;
            
            if(nbyte_tofill > nbyte_npcm)
               nbyte_tofill = nbyte_npcm;

            memcpy(&inbufw->pcm[inbufw->nbyte_wrote], inbuf, nbyte_tofill);

            inbufw->nbyte_wrote += nbyte_tofill;
            nbyte_npcm -= nbyte_npcm;
            
            if(inbufw->nbyte_wrote == pa->input_nbyte_once)
            {
               inbufw->tick = pa->input_tick++;
               pa->inbuf_w = (pa->inbuf_w+1) % pa->inbuf_n;

               inbufw = &pa->inbuf[pa->inbuf_w];

		         pa->input_samplestamp += pa->input_npcm_once;
               inbufw->stamp = pa->input_samplestamp;
            }
         }
      }
   }
   
   if(!outbuf)
   	pa_debug(("\rpa_io_callback: no output\n"));
   else if(0 != (statusFlags & paOutputUnderflow))
   	pa_debug(("\rpa_io_callback: output underflow\n"));
   else if(0 != (statusFlags & paOutputOverflow))
   	pa_debug(("\rpa_io_callback: output overflow\n"));
   else
      pa->out->pick_content(pa->out, (media_info_t*)pa->ai_output, outbuf, (int)npcm);

   return 0;
}

int pa_done_setting(control_setting_t *gen)
{
   xfree(gen);

   return MP_OK;
}

control_setting_t* pa_new_setting(media_device_t *dev)
{
   pa_setting_t * set = xmalloc(sizeof(struct pa_setting_s));
   if(!set)
   {
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
      pa->inbuf[i].pcm = xmalloc(pa->input_nbyte_once);
      memset(pa->inbuf[i].pcm, 0, pa->input_nbyte_once);
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

int pa_done_outbuf(portaudio_device_t *pa)
{
   int i;

   for(i=0; i<pa->outbuf_n; i++)
      xfree(pa->outbuf[i].pcm);

   xfree(pa->outbuf);

   return MP_OK;
}

int pa_set_input(media_device_t *dev, media_info_t *minfo, media_receiver_t* recvr)
{
   portaudio_device_t *pa = (portaudio_device_t *)dev;

   audio_info_t* ai = (audio_info_t*)minfo;

   if(pa->pa_instream)
      return MP_OK;

   if(!pa->online && pa_online (dev) < MP_OK)
   {
      pa_debug (("pa_set_input: Can't activate PortAudio device\n"));
      return MP_FAIL;
   }

   ai->channels_bytes = ai->channels * ai->info.sample_bits / OS_BYTE_BITS;
   ai->info.sampling_constant = DEFAULT_OUTPUT_NSAMPLE_PULSE;

   pa->usec_pulse = 1000000 / ai->info.sample_rate * DEFAULT_OUTPUT_NSAMPLE_PULSE;

   pa->input_npcm_once = DEFAULT_OUTPUT_NSAMPLE_PULSE;
   pa->input_nbyte_once = ai->channels_bytes * DEFAULT_OUTPUT_NSAMPLE_PULSE;

   pa->ai_input = ai;   
   pa_make_inbuf(pa, ai, PA_DEFAULT_INBUF_N);

   pa->receiver = recvr;
   
   return MP_OK;
}

int pa_set_io(media_device_t *dev, media_info_t *minfo, media_receiver_t* recvr)
{
   portaudio_device_t *pa = (portaudio_device_t *)dev;
   audio_info_t* ai = (audio_info_t*)minfo;

   if(!pa->ai_input && !pa->ai_output)
   {
      ai->info.sampling_constant = DEFAULT_OUTPUT_NSAMPLE_PULSE;
      ai->channels_bytes = ai->channels * ai->info.sample_bits / OS_BYTE_BITS;

      pa->usec_pulse = 1000000 / ai->info.sample_rate * DEFAULT_OUTPUT_NSAMPLE_PULSE;

      pa->input_npcm_once = DEFAULT_INPUT_NSAMPLE_PULSE;
      pa->input_nbyte_once = ai->channels_bytes * DEFAULT_INPUT_NSAMPLE_PULSE;
      
      pa->out = pa_pipe_new(pa);
      
      pa_make_inbuf(pa, ai, PA_DEFAULT_INBUF_N);

      pa->ai_input = ai;
      pa->ai_output = ai;

      pa_debug(("\rpa_set_io: %d channels, %d rate, pulse[%dus], input bytes[%d]\n", ai->channels, ai->info.sample_rate, pa->usec_pulse, pa->input_nbyte_once));
   }

   if(recvr) pa->receiver = recvr;

   return MP_OK;
}

int pa_init(media_device_t * dev, media_control_t *ctrl)
{
   return MP_OK;
}

#if !defined (PA_HALF_DUPLEX)

int pa_start_io(media_device_t * dev, int mode)
{
	int err;
   PaStreamParameters  inputParameters, *in=NULL;
   PaStreamParameters  outputParameters, *out=NULL;
   
	portaudio_device_t *pa = (portaudio_device_t *)dev;

   if(pa->pa_iostream && Pa_IsStreamActive(pa->pa_iostream))
      return MP_OK;
   
   if(pa->ai_input)
   {
      inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
      inputParameters.channelCount = pa->ai_input->channels;
      inputParameters.sampleFormat = PA_SAMPLE_TYPE;
      inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
      inputParameters.hostApiSpecificStreamInfo = NULL;

      in = &inputParameters;
   }

   if(pa->ai_output)
   {
      outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
      outputParameters.channelCount = pa->ai_output->channels;
      outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
      outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
      outputParameters.hostApiSpecificStreamInfo = NULL;

      out = &outputParameters;
   }

   err = Pa_OpenStream
					(	&pa->pa_iostream,
                  in,
                  out,
						pa->ai_input->info.sample_rate * 1.0,
						paFramesPerBufferUnspecified,    /* default framesPerBuffer */
						(paDitherOff),                   /* PaStreamFlags streamFlags: paDitherOff */
						pa_io_callback,
						pa
					);

   if ( err != paNoError )
   {
      pa_debug(("pa_start_io: %s\n", Pa_GetErrorText(err) ));
      exit(1);
      return MP_FAIL;
   }

	err = Pa_StartStream (pa->pa_iostream);

	if( err != paNoError )
	{
	   pa_debug(("pa_start_io: An error occured while using the portaudio stream\n"));
	   return MP_FAIL;
	}
      
	pa_debug(("pa_start_io: PortAudio io started\n"));

	/* Start Audio Input */
	if(pa->receiver)
		pa->input_thread = xthr_new (pa_input_loop, pa, XTHREAD_NONEFLAGS);

   dev->running = 1;

	return MP_OK;
}

#else

int pa_start_io(media_device_t * dev, int mode)
{
	int err;
   PaStreamParameters  inputParameters, *in=NULL;
   PaStreamParameters  outputParameters, *out=NULL;

	portaudio_device_t *pa = (portaudio_device_t *)dev;

   if(pa->pa_iostream && Pa_IsStreamActive(pa->pa_iostream))
      return MP_OK;

   if(0)//pa->ai_input)
   {
      inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
      inputParameters.channelCount = pa->ai_input->channels;
      inputParameters.sampleFormat = PA_SAMPLE_TYPE;
      inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
      inputParameters.hostApiSpecificStreamInfo = NULL;

      in = &inputParameters;

      err = Pa_OpenStream
					(	&pa->pa_instream,
                  in,
                  NULL,
						pa->ai_input->info.sample_rate * 1.0,
						DEFAULT_INPUT_NSAMPLE_PULSE, /* unsigned long framesPerBuffer */
						paDitherOff,                 /* PaStreamFlags streamFlags: paDitherOff */
						pa_input_callback,
						pa
					);
               
      if ( err != paNoError )
      {
         pa_debug(("pa_start_io: %s\n", Pa_GetErrorText(err) ));
         return MP_FAIL;
      }

      err = Pa_StartStream (pa->pa_instream);

	   if( err != paNoError )
	   {
	      pa_debug(("pa_start_io: An error occured while using the portaudio stream\n"));
	      return MP_FAIL;
	   }
   }

   if(pa->ai_output)
   {
      outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
      outputParameters.channelCount = pa->ai_output->channels;
      outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
      outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
      outputParameters.hostApiSpecificStreamInfo = NULL;

      out = &outputParameters;
      
      err = Pa_OpenStream
					(	&pa->pa_outstream,
                  NULL,
                  out,
						pa->ai_input->info.sample_rate * 1.0,
						DEFAULT_OUTPUT_NSAMPLE_PULSE, /* unsigned long framesPerBuffer */
						paDitherOff,                  /* PaStreamFlags streamFlags: paDitherOff */
						pa_output_callback,
						pa
					);
               
      if ( err != paNoError )
      {
         pa_debug(("pa_start_io: %s\n", Pa_GetErrorText(err) ));
         return MP_FAIL;
      }
	   err = Pa_StartStream (pa->pa_outstream);

	   if( err != paNoError )
	   {
	      pa_debug(("pa_start_io: An error occured while using the portaudio stream\n"));
	      return MP_FAIL;
	   }
   }

	pa_debug(("pa_start_io: PortAudio io started\n"));

	/* Start Audio Input */
	if(pa->receiver)
		pa->input_thread = xthr_new (pa_input_loop, pa, XTHREAD_NONEFLAGS);

   dev->running = 1;

	return MP_OK;
}

#endif

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
   dev->set_input_media = pa_set_input;
   
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
