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

#include <portaudio.h>
#include <string.h>

#define PA_SAMPLE_TYPE  paInt16

#define DEFAULT_SAMPLE_FACTOR  16

#define DELAY_WHILE 1000
/*
#define PORTAUDIO_LOG
#define PORTAUDIO_DEBUG
*/
#ifdef PORTAUDIO_LOG
 #define pa_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define pa_log(fmtargs)
#endif

#ifdef PORTAUDIO_DEBUG
 #define pa_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define pa_debug(fmtargs)
#endif

typedef struct portaudio_device_s {

   struct media_device_s dev;

   media_pipe_t * out;

   PortAudioStream *pa_stream;
   int time_adjust;     /* adjust PortAudio time */
   int nbuf_internal;   /* PortAudio Internal Buffer number, for performance tuning */

   int sample_factor;
   int usec_pulse;

   int online;

   audio_info_t ai;

   xrtp_clock_t *clock;

   int time_match;      /* try delay to match pulse time */
   int last_pick;

   xrtp_hrtime_t while_ns;

} portaudio_device_t;

static int pa_callback( void *inbuf, void *outbuf,
                        unsigned long npcm_once,
                        PaTimestamp outtime, void *udata ){

   portaudio_device_t* pa = (portaudio_device_t*)udata;

   int pulse_us = 0;
   int itval = 0;

   int pick_us_start = time_usec_now(pa->clock);
   int ret = pa->out->pick_content(pa->out, (media_info_t*)&pa->ai, outbuf, npcm_once);
   int pick_us = time_usec_spent(pa->clock, pick_us_start);

   if (pa->dev.running == 0) pa->dev.running = 1;
   else {

      /* time adjustment */
      pulse_us = time_usec_spent(pa->clock, pa->last_pick);
   
      if (!pa->time_match) {

         pa_log(("pa_callback: %dus picking, %dus interval %dus pulse, no more delay\n"
            , pick_us, pulse_us, pa->usec_pulse));

      } else {
   
         int delay_ns = (pa->usec_pulse - pulse_us) * 1000;

         int delay = delay_ns  / pa->while_ns;

         if ( delay > 0 ) {

            pa_log(("pa_callback: %dus picking, %dus interval, need %dns delay (%d loop), %dns delay unit\n"
                  , pick_us, pulse_us, delay_ns, delay, pa->while_ns));

            while (delay) delay--;
         }

         itval = time_usec_spent(pa->clock, pa->last_pick);
         pa_log(("pa_callback: %dus match %dus audio\n", itval, pa->usec_pulse));
      }

   }
   
   pa->last_pick = pick_us_start;

   return ret;
}

int pa_done_setting(control_setting_t *gen){

   free(gen);

   return MP_OK;
}

control_setting_t* pa_new_setting(media_device_t *dev){

   pa_setting_t * set = malloc(sizeof(struct pa_setting_s));
   if(!set){

      pa_debug(("rtp_new_setting: No memory"));

      return NULL;
   }
   memset(set, 0, sizeof(struct pa_setting_s));

   set->setting.done = pa_done_setting;

   return (control_setting_t*)set;
}

media_pipe_t * pa_pipe(media_device_t *dev) {

   return ((portaudio_device_t *)dev)->out;
}

int pa_online (media_device_t * dev) {

   PaError err;

   portaudio_device_t *pa_dev = (portaudio_device_t *)dev;

   if (pa_dev->online) return MP_OK;

   err = Pa_Initialize();
   if ( err != paNoError ) {

      pa_debug(("pa_online: %s\n", Pa_GetErrorText(err) ));
      Pa_Terminate();

      return MP_FAIL;
   }

   pa_dev->online = 1;

   pa_debug(("pa_online: portaudio initialized\n"));
   
   return MP_OK;
}

int pa_sample_type (int sample_bits) {

   switch (sample_bits) {

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

int pa_set_media_info(media_device_t *dev, media_info_t *info){

   portaudio_device_t *pa = (portaudio_device_t *)dev;
   
   int nsp = 0;
   int nsample_pulse = 0;
   int sample_type = 0;

   PaError err = 0;
   int pa_min_nbuf = 0;

   if(!pa->online && pa_online (dev) < MP_OK) {

      pa_debug (("pa_start: Can't activate PortAudio device\n"));

      return MP_FAIL;
   }
   
   pa->ai = *((audio_info_t*)info);
   
   nsp = pa->ai.info.sample_rate / pa->sample_factor;
   
   nsample_pulse = 1;
   while (nsample_pulse * 2 <= nsp) nsample_pulse *= 2;
   
   if(pa->ai.info.sample_rate > nsample_pulse)
      pa->usec_pulse = (int)(1000000 / (pa->ai.info.sample_rate / (double)nsample_pulse));
   else
      pa->usec_pulse = (int)((double)nsample_pulse / pa->ai.info.sample_rate * 1000000);
      
   pa_log(("pa_start: %d channels, %d rate, %d sample per pulse (%dus)\n"
         , pa->ai.channels, pa->ai.info.sample_rate, nsample_pulse, pa->usec_pulse));
   
   pa->ai.channels_bytes = pa->ai.channels * pa->ai.info.sample_bits / BYTE_BITS;

   pa->out = timed_pipe_new(pa->ai.info.sample_rate, pa->usec_pulse);
         
   sample_type = pa_sample_type(pa->ai.info.sample_bits);
   
   err = Pa_OpenDefaultStream
                  (  &pa->pa_stream
                     , 0                        /* no input channels */
                     , pa->ai.channels          /* output channels */
                     , sample_type              /* paInt16: 16 bit integer output, etc */
                     , pa->ai.info.sample_rate  /* sample rate, cd is 44100hz */
                     , nsample_pulse
                     , pa->nbuf_internal        /* use default minimum audio buffers in portaudio */
                     , pa_callback
                     , pa
                  );

   if ( err != paNoError ) {

      pa_debug(("pa_start: %s\n", Pa_GetErrorText(err) ));
      Pa_Terminate();
      pa_debug(("pa_start: here\n"));

      return MP_FAIL;
   }

   pa_min_nbuf = Pa_GetMinNumBuffers (nsample_pulse, pa->ai.info.sample_rate);
   pa_log(("\n[pa_start: stream opened, PortAudio use %d internal buffers]\n\n", pa_min_nbuf));

   /* Start PortAudio */
   err = Pa_StartStream ( pa->pa_stream );

   if( err != paNoError ) {

      Pa_Terminate();
      pa_debug(("pa_start: An error occured while using the portaudio stream\n"));

      pa->online = 0;
      return MP_FAIL;
   }

   pa_debug(("pa_start: PortAudio started\n"));

   return MP_OK;
}

int pa_start (media_device_t * dev, media_control_t *ctrl){

	pa_log(("pa_start: pa_set_media_info() to start portaudio\n"));
	return MP_OK;
}

int pa_stop (media_device_t * dev) {

   portaudio_device_t *pa_dev = NULL;
   PaError err = 0;

   pa_debug(("pa_stop: to stop PortAudio\n"));

   if (!dev->running) {

      pa_debug(("pa_stop: PortAudio is idled\n"));

      return MP_OK;
   }
   
   pa_dev = (portaudio_device_t *)dev;

   err = Pa_StopStream ( pa_dev->pa_stream );
   if( err != paNoError ) {

      Pa_Terminate();
      pa_debug(("pa_stop: An error occured while stop the portaudio\n"));

      pa_dev->online = 0;
      return MP_FAIL;
   }

   dev->running = 0;
   
   pa_dev->out->done (pa_dev->out);
   pa_dev->out = NULL;

   pa_debug(("pa_stop: PortAudio stopped\n"));

   return MP_OK;
}

int pa_offline (media_device_t * dev) {

   portaudio_device_t *pa_dev = NULL;

   if (dev->running) pa_stop (dev);

   Pa_Terminate();
   pa_debug(("pa_stop: portaudio terminated.\n"));

   pa_dev = (portaudio_device_t *)dev;

   pa_dev->online = 0;

   return MP_OK;
}

int pa_done (media_device_t * dev) {

   portaudio_device_t *pa_dev = (portaudio_device_t *)dev;
   
   if (pa_dev->online) pa_offline (dev);

   if (pa_dev->out) pa_dev->out->done (pa_dev->out);

   if(pa_dev->clock) time_end(pa_dev->clock);

   free(pa_dev);
   
   return MP_OK;
}

int pa_setting (media_device_t * dev, control_setting_t *setting, module_catalog_t *cata) {

   /* module_catalog is not used, as no more module needed */
   
   portaudio_device_t * pa = (portaudio_device_t *)dev;
   
   pa->sample_factor = 0;
   
   if (setting) {

      pa_setting_t *pa_setting = (pa_setting_t *)setting;
      
      pa->time_match = pa_setting->time_match;
      
      pa->nbuf_internal = pa_setting->nbuf_internal;

      pa->sample_factor = pa_setting->sample_factor;
   }

   if(pa->sample_factor == 0) pa->sample_factor = DEFAULT_SAMPLE_FACTOR;
   
   return MP_OK;
}

int pa_match_type(media_device_t *dev, char *type) {

   pa_log(("portaudio.pa_match_type: I am audio in/out device\n"));

   if( !strcmp("audio_out", type) ) return 1;
   if( !strcmp("audio_in", type) ) return 1;
   if( !strcmp("audio_io", type) ) return 1;
   
   return 0;
}

module_interface_t* media_new_device () {

   media_device_t *dev = NULL;
   int delay = 0;
   xrtp_hrtime_t hz_start = 0;
   xrtp_hrtime_t hz_passed = 0;

   portaudio_device_t * pa = malloc (sizeof(struct portaudio_device_s));
   if (!pa) {

      pa_debug(("media_new_device: No enough memory\n"));

      return NULL;
   }
   
   memset(pa, 0, sizeof(struct portaudio_device_s));

   pa->time_match = 1;

   pa->sample_factor = DEFAULT_SAMPLE_FACTOR;

   pa->clock = time_start();

   dev = (media_device_t *)pa;

   dev->pipe = pa_pipe;

   dev->start = pa_start;
   dev->stop = pa_stop;
   
   dev->done = pa_done;

   dev->set_media_info = pa_set_media_info;
   
   dev->new_setting = pa_new_setting;
   dev->setting = pa_setting;
   
   dev->match_type = pa_match_type;

   /* test speed */

   delay = DELAY_WHILE;

   hz_start = time_nsec_now(pa->clock);
   while (delay) delay--;
   hz_passed = time_nsec_spent(pa->clock, hz_start);

   pa->while_ns = hz_passed / DELAY_WHILE;

   if ( pa_online(dev) >= MP_OK) pa->online = 1;

   pa_log(("media_new_device: PortAudio device created, with %dus pulse\n", pa->usec_pulse));

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
