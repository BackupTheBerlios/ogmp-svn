/***************************************************************************
                          vorbis_player.c  -  Vorbis audio stream
                             -------------------
    begin                : Tue Jan 20 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org; heming@bigfoot.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include "vorbis_decoder.h"

#include <string.h>
#include <stdlib.h>

#define VORBIS_PLAYER_LOG
#define VORBIS_PLAYER_DEBUG

#ifdef VORBIS_PLAYER_LOG
   const int vorbis_pa_log = 1;
#else
   const int vorbis_pa_log = 0;
#endif

#define vorbis_player_log(fmtargs)  do{if(vorbis_pa_log) printf fmtargs;}while(0)

#ifdef VORBIS_PLAYER_DEBUG
   const int vorbis_pa_debug = 1;
#else
   const int vorbis_pa_debug = 0;
#endif
#define vorbis_player_debug(fmtargs)  do{if(vorbis_pa_debug) printf fmtargs;}while(0)

struct global_const_s {

   const char media_type[6];
   const char mime_type[13];
   const char play_type[6];

} global_const = {"audio", "audio/vorbis", "local"};

int vorbis_set_callback (media_player_t * mp, int type, int(*call)(void*), void * user) {

   vorbis_decoder_t *playa = (vorbis_decoder_t *)mp;

   switch(type){

      case (CALLBACK_PLAY_MEDIA):

         playa->play_media = call;
         playa->play_media_user = user;
         vorbis_player_log(("vorbis_set_callback: 'media_sent' callback added\n"));
         break;

      case (CALLBACK_STOP_MEDIA):
      
         playa->stop_media = call;
         playa->stop_media_user = user;
         vorbis_player_log(("vorbis_set_callback: 'media_received' callback added\n"));
         break;
         
      default:

         vorbis_player_log(("< vorbis_set_callback: callback[%d] unsupported >\n", type));
         return MP_EUNSUP;
   }

   return MP_OK;
}

int vorbis_match_type (media_player_t * mp, char *mime, char *fourcc) {

   if (mime && strncasecmp(mime, "audio/vorbis", 12) == 0) {

      vorbis_player_log(("vorbis_match_type: mime = 'audio/vorbis'\n"));
      return 1;
   }
   
   return 0;
}

int vorbis_open_stream (media_player_t *mp, media_info_t *media_info) {

   vorbis_decoder_t *vp = NULL;
   
   vorbis_info_t *vinfo = (vorbis_info_t *)media_info;
   
   struct audio_info_s ai;
   
   int ret;
   
   if (!mp->device) {
     
      vorbis_player_log (("vorbis_open_stream: No device to play vorbis audio\n"));
      return MP_FAIL;
   }
   
   vp = (vorbis_decoder_t *)mp;
   
   vp->vorbis_info = vinfo;
   
   ai.info.sample_rate = vinfo->vi.rate;
   ai.info.sample_bits = VORBIS_SAMPLE_BITS;
   ai.channels = vinfo->vi.channels;
   
   ret = mp->device->start (mp->device, &ai, vp->dad_min_ms, vp->dad_max_ms);
   
   if (ret < MP_OK) {

      vorbis_player_log (("vorbis_open_stream: vorbis stream fail to open\n"));

      return ret;
   }
   
   return ret;
}

int vorbis_close_stream (media_player_t * mp) {

   vorbis_player_debug (("vorbis_close_stream: Not implemented yet\n"));
   
   return MP_EIMPL;
}

int vorbis_stop (media_player_t *mp) {

   vorbis_player_debug(("vorbis_stop: to stop vorbis player\n"));
   mp->device->stop (mp->device);

   ((vorbis_decoder_t*)mp)->receiving_media = 0;

   return MP_OK;
}

int vorbis_receive_next (media_player_t *mp, void *vorbis_packet, int samplestamp, int last_packet) {

   media_pipe_t * output = NULL;
   media_frame_t * auf = NULL;
   
   vorbis_decoder_t *vp = (vorbis_decoder_t *)mp;
   int sample_rate = vp->vorbis_info->vi.rate;
   
   if (!mp->device) {

      vorbis_player_debug(("vorbis_receive_next: No device to play vorbis audio\n"));
      return MP_FAIL;
   }

   output = mp->device->pipe(mp->device);

   /* decode and submit */
   auf = vorbis_decode (vp->vorbis_info, (ogg_packet*)vorbis_packet, output);
   if(!auf){

      vorbis_player_log(("vorbis_receive_next: No audio samples decoded\n"));
      return MP_FAIL;
   }

   if (!vp->receiving_media) {

      vp->ts_usec_now = 0;
      vp->last_samplestamp = samplestamp;
   }

   if (vp->last_samplestamp != samplestamp) {

      vp->ts_usec_now += (samplestamp - vp->last_samplestamp) / (double)sample_rate * 1000000;
      vp->last_samplestamp = samplestamp;
   }
   
   auf->ts = vp->ts_usec_now;
   
   /*
   if ( last_packet || (vp->last_msec_sync == 0 && microstamp != 0) )
      vp->last_msec_sync = microstamp;
   */
   output->put_frame(output, auf, last_packet);

   vp->receiving_media = 1;

   return MP_OK;
}

char* vorbis_media_type(media_player_t * mp){

   return global_const.media_type;
}

char* vorbis_play_type(media_player_t * mp){

   return global_const.play_type;
}

media_pipe_t * vorbis_pipe(media_player_t * p) {

   if (!p->device) return NULL;

   return p->device->pipe (p->device);
}

int vorbis_done(media_player_t *mp) {

   vorbis_stop (mp);

   mp->device->done (mp->device);

   free((vorbis_decoder_t *)mp);

   return MP_OK;
}

int vorbis_set_options (media_player_t * mp, char *opt, void *value) {

    vorbis_decoder_t *vp = (vorbis_decoder_t *)mp;
    
    if(!value){
      
        vorbis_player_debug(("vorbis_set_options: param 'value' is NULL point\n"));
        return MP_FAIL;
    }
    
    if (strcmp(opt, "dad_min_ms") == 0) { 

        vorbis_player_log(("vorbis_set_options: %s = %d\n", opt, *((int*)value)));
        vp->dad_min_ms = *((int*)value);
        
    } else if(strcmp(opt, "dad_max_ms") == 0) {

        vorbis_player_log(("vorbis_set_options: %s = %d\n", opt, *((int*)value)));
        vp->dad_max_ms = *((int*)value);

    } else {

        vorbis_player_log(("vorbis_set_options: the option is not supported\n"));
        return MP_EUNSUP;
    }
    
    return MP_OK;
}

int vorbis_done_device ( void *gen ) {

   media_device_t* dev = (media_device_t*)gen;

   dev->done(dev);

   return MP_OK;
}

int vorbis_set_device (media_player_t * mp, media_control_t *cont, module_catalog_t *cata) {

   control_setting_t *setting = NULL;

   xrtp_list_user_t $lu;
   xrtp_list_t * devs = xrtp_list_new();

   media_device_t *dev = NULL;
   
   vorbis_player_log(("vorbis_set_device: need audio device\n"));
   
   if (catalog_create_modules(cata, "device", devs) <= 0) {
   
      vorbis_player_log(("vorbis_set_device: no device module found\n"));

      xrtp_list_free(devs, NULL);

      return MP_FAIL;
   }

   dev = xrtp_list_first(devs, &$lu);
   while (dev) {

      if (dev->match_type(dev, "audio")) {

         vorbis_player_log(("vorbis_set_device: found audio device\n"));

         xrtp_list_remove_item(devs, dev);

         break;
      }

      dev = xrtp_list_next(devs, &$lu);
   }

   xrtp_list_free(devs, vorbis_done_device);

   if(!dev) return MP_FAIL;

   vorbis_player_log(("vorbis_set_device: here\n"));
   setting = cont->fetch_setting(cont, "audio", dev);
   vorbis_player_log(("vorbis_set_device: here2\n"));
   if(!setting){
     
      vorbis_player_log(("vorbis_set_device: use default setting for audio device\n"));

   }else{

      dev->setting(dev, setting, cata);     
   }
   
   mp->device = dev;

   return MP_OK;
}

module_interface_t * media_new_player(){

   media_player_t *mp = NULL;

   vorbis_decoder_t *playa = malloc(sizeof(struct vorbis_decoder_s));
   if(!playa){

      vorbis_player_debug(("vorbis_new_player: No memory to allocate\n"));

      return NULL;
   }

   memset(playa, 0, sizeof(struct vorbis_decoder_s));

   mp = (media_player_t *)playa;

   mp->done = vorbis_done;

   mp->play_type = vorbis_play_type;
   mp->media_type = vorbis_media_type;
   mp->codec_type = vorbis_media_type;

   mp->set_callback = vorbis_set_callback;
   mp->pipe = vorbis_pipe;
   
   mp->match_type = vorbis_match_type;
   
   mp->open_stream = vorbis_open_stream;
   mp->close_stream = vorbis_close_stream;

   mp->receive_media = vorbis_receive_next;
   mp->set_options = vorbis_set_options;

   mp->set_device = vorbis_set_device;
   
   mp->stop = vorbis_stop;

   return mp;
}

/**
 * Loadin Infomation Block
 */
module_loadin_t mediaformat = {

   "playback",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   media_new_player   /* Module initializer */
};
                   
