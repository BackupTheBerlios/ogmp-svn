/***************************************************************************
                          dev_ao.c  - libao Device
                             -------------------
    begin                : Fri Feb 6 2004
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
#include "timed_pipe.h"
#include "dev_ao.h"

#include <timedia/xthread.h>

#include <ao/ao.h>
#include <string.h>

typedef struct ao_device_s {

   struct media_device_s dev;

   media_pipe_t * pipe;

   ao_device *ao_device;
   
   int running;
   
   int online;

   audio_info_t ai;

   int nsample_pulse;
   
   char *pulse;
   int pulse_size;
   int usec_pulse;

   xrtp_thread_t * audio_writer;

   xrtp_clock_t *clock;
   
} ao_device_t;

#define AO_LOG
#define AO_DEBUG

#ifdef AO_LOG
   const int ao_log_flag = 1;
#else
   const int ao_log_flag = 0;
#endif

#define ao_log(fmtargs)  do{if(ao_log_flag) printf fmtargs;}while(0)

#ifdef AO_DEBUG
   const int ao_debug_flag = 1;
#else
   const int ao_debug_flag = 0;
#endif
#define ao_debug(fmtargs)  do{if(ao_debug_flag) printf fmtargs;}while(0)

#define DEFAULT_NSAMPLE_PULSE  1024
#define MIN_NSAMPLE_PULSE       64

int audio_loop(void *gen) {

   ao_device_t *ao = (ao_device_t *)gen;
   
   ao_log(("audio_thread: audio thread start\n"));

   ao->running = 1;

   while (ao->running) {

      int end = ao->pipe->pick_frame((media_pipe_t*)ao->pipe, (media_info_t*)&ao->ai, ao->pulse, ao->nsample_pulse);
      
      ao_play(ao->ao_device, ao->pulse, ao->pulse_size);

      if (end) break;
   }

   ao_log(("audio_thread: audio thread stopped\n"));

   return MP_OK;
}

media_pipe_t * ao_pipe(media_device_t *dev) {

   return ((ao_device_t*)dev)->pipe;
}

int ao_online (media_device_t * dev) {

   ao_device_t *ao = (ao_device_t *)dev;

   if (ao->online) return MP_OK;

	ao_initialize();
   
   ao->online = 1;

   ao_log(("ao_online: ao device online\n"));

   int ndrv;

   /* list available audio driver info */
   ao_log(("ao_online: audio driver available on the system:\n"));
   ao_info** drvs = ao_driver_info_list (&ndrv);
   int i;
   for (i=0; i<ndrv; i++) {

      ao_log(("ao_online: '%s': '%s' (%s): prefer "
            , drvs[i]->short_name, drvs[i]->name, drvs[i]->comment));
      
      switch (drvs[i]->preferred_byte_format) {

         case AO_FMT_LITTLE: ao_log(("little endian\n")); break;
         case AO_FMT_BIG: ao_log(("big endian\n")); break;
         case AO_FMT_NATIVE: ao_log(("native endian\n")); break;
      }
   }
   ao_log(("\n"));
   /* end of drivers' info section */

   return MP_OK;
}

int ao_sample_type (int media_sample_type) {

   switch (media_sample_type) {

      case SAMPLE_TYPE_NATIVE: return AO_FMT_NATIVE;

      case SAMPLE_TYPE_BIGEND: return AO_FMT_BIG;

      case SAMPLE_TYPE_LITTLEEND: return AO_FMT_LITTLE;
   }

   return AO_FMT_NATIVE;
}

int ao_start (media_device_t * dev, void * info) {

   ao_device_t *ao = (ao_device_t *)dev;

   if(!ao->online)  ao_online (dev);

   ao->ai = *((audio_info_t*)info);

   if(ao->ai.info.sample_rate > ao->nsample_pulse)
      ao->usec_pulse = 1000000 / (ao->ai.info.sample_rate / (double)ao->nsample_pulse);
   else
      ao->usec_pulse = (double)ao->nsample_pulse / ao->ai.info.sample_rate * 1000000;

   ao_log(("ao_start: %d channels, %d rate, %d sample each pulse (%dus)\n"
         , ao->ai.channels, ao->ai.info.sample_rate, ao->nsample_pulse, ao->usec_pulse));

   /* allocate pulse */
   ao->ai.channels_bytes = ao->ai.channels * ao->ai.info.sample_bits / BYTE_BITS;
   ao->pulse_size = ao->ai.channels_bytes * ao->nsample_pulse;
   ao_log(("ao_online: %d bytes each all-channel sample, pulse_size = %d bytes\n"
            , ao->ai.channels_bytes, ao->pulse_size));

   ao->pulse = malloc(ao->pulse_size);
   if(!ao->pulse) {

      ao_debug(("ao_start: No memery for samples bpulse\n"));
      ao->pulse_size = 0;
      return MP_FAIL;
   }

   /* -- Setup for default driver -- */

   int driver_id = ao_default_driver_id();
   if (driver_id == -1) {

#ifdef AO_SIMULATE

      ao_log(("ao_start: Use 'null' audio device instead\n"));
      driver_id = ao_driver_id("null");

#else

      ao_log(("ao_start: No audio device found\n"));
      return MP_FAIL;

#endif

   }
   
   /* audio driver info */
   ao_info* drv_info = ao_driver_info (driver_id);
   ao_log(("ao_online: driver '%s' in use now\n", drv_info->short_name));

   /* -- Open driver -- */
   ao_sample_format format;
   format.channels = ao->ai.channels;
   format.rate = ao->ai.info.sample_rate;
   format.bits = ao->ai.info.sample_bits;
   format.byte_format = ao_sample_type(ao->ai.info.sample_byteorder); /* AO_FMT_LITTLE, etc */

   ao->ao_device = ao_open_live(driver_id, &format, NULL /* no options */);

   if (ao->ao_device == NULL) {
     
      ao_debug(("ao_start: Fail to open ao device.\n"));
      
      switch (errno) {
           
         case AO_ENODRIVER:

            ao_debug(("ao_start: Device not available.\n"));

            break;

         case AO_ENOTLIVE:

            ao_debug(("ao_start: Not a audio device\n"));
            
            break;
            
         case AO_EBADOPTION:
         
            ao_debug(("ao_start: Unsupported option value to device.\n"));
            
            break;
            
         case AO_EOPENDEVICE:
         
            ao_debug(("ao_start: Cannot open device\n"));
            
            break;
            
         case AO_EFAIL:
         
            ao_debug(("ao_start: Device failure.\n"));

            break;
            
         case AO_ENOTFILE:
         
            ao_debug(("ao_start: An output file cannot be given for device.\n"));
            
            break;
            
         case AO_EOPENFILE:

            ao_debug(("ao_start: Cannot open file for writing.\n"));
            
            break;
            
         case AO_EFILEEXISTS:

            ao_debug(("ao_start: File exists.\n"));
            
            break;
            
         default:
         
            ao_debug(("ao_start: This error should never happen (%d).  Panic!\n"
                     , errno));
            break;
      }

      return MP_FAIL;
   }

   ao_log(("ao_start: ao device open successfully\n"));

   ao->pipe = timed_pipe_new(ao->usec_pulse);

   ao->audio_writer = xthr_new(audio_loop, ao, XTHREAD_NONEFLAGS);

   ao_log(("ao_start: stream started\n"));

   return MP_OK;
}

int ao_stop (media_device_t * dev) {

   ao_device_t *ao = (ao_device_t *)dev;

   int th_ret;
   ao->running = 0;
   xthr_wait(ao->audio_writer, &th_ret);
   
   ao_log(("ao_stop: audio thread stopped\n"));

   dev->running = 0;

   ao->pipe->done (ao->pipe);
   ao->pipe = NULL;

   free(ao->pulse);
   
   return MP_OK;
}

int ao_offline (media_device_t * dev) {

   if (dev->running) ao_stop (dev);

   ao_shutdown();

   ao_device_t *ao = (ao_device_t *)dev;

   ao->pipe->done (ao->pipe);
   ao->pipe = NULL;

   ao->online = 0;

   ao_log(("ao_offline: ao device offlined\n"));

   return MP_OK;
}

int ao_done (media_device_t * dev) {

   ao_device_t *ao = (ao_device_t *)dev;

   if (ao->online) ao_offline (dev);

   if (ao->pipe) ao->pipe->done (ao->pipe);

   free(ao);

   ao_log(("ao_done: ao device released\n"));

   return MP_OK;
}

media_device_t * media_new_device (void * setting) {

   ao_device_t * ao = malloc (sizeof(struct ao_device_s));
   if (!ao) {

      ao_debug(("media_new_device: No enough memory\n"));
      
      return NULL;
   }
   memset(ao, 0, sizeof(struct ao_device_s));

   if (setting) {

      ao_setting_t *ao_setting = (ao_setting_t *)setting;

      if (ao_setting->nsample_pulse >= MIN_NSAMPLE_PULSE)
         ao->nsample_pulse = ao_setting->nsample_pulse;
   }

   if (!ao->nsample_pulse)
      ao->nsample_pulse = DEFAULT_NSAMPLE_PULSE;

   ao->clock = time_begin(0,0);

   media_device_t *dev = (media_device_t *)ao;

   dev->pipe = ao_pipe;

   dev->online = ao_online;
   dev->offline = ao_offline;

   dev->start = ao_start;
   dev->stop = ao_stop;

   dev->done = ao_done;

   if ( ao_online(dev) >= MP_OK) ao->online = 1;
   
   ao_log(("media_new_device: ao device created\n"));
   
   return dev;
}

