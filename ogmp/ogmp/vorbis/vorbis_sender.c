/***************************************************************************
                          vorbis_sender.c  -  Send Vorbis rtp stream
                             -------------------
    begin                : Thu May 13 2004
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
 
#include <xrtp/xrtp.h>

#include <timedia/xstring.h>
#include <timedia/list.h>
#include <stdlib.h>

#include "../devices/dev_rtp.h"
#include "../media_format.h"

#include "vorbis_info.h"

#define VSENDER_LOG
#define VSENDER_DEBUG

#ifdef VSENDER_LOG
   const int vsender_log = 1;
#else
   const int vsender_log = 0;
#endif

#define vsend_log(fmtargs)  do{if(vsender_log) printf fmtargs;}while(0)

#ifdef VSENDER_DEBUG
   const int vsender_debug = 1;
#else
   const int vsender_debug = 0;
#endif
#define vsend_debug(fmtargs)  do{if(vsender_debug) printf fmtargs;}while(0)

struct global_const_s {

   const char media_type[6];
   const char mime_type[13];
   const char play_type[6];

} global_const = {"audio", "audio/vorbis", "cast"};

typedef struct vorbis_sender_s vorbis_sender_t;
struct vorbis_sender_s{

   struct media_player_s player;

   vorbis_info_t  *vorbis_info;

   int stream_opened;

   int last_msec_sync;

   /* Failure occurred */
   int fail;

   /* Delay Adapt Detect: if the buffer fall into this range
    * special process will activated to smooth the effect
    */
   int dad_min_ms;
   int dad_max_ms;

   int (*play_media) (void * user);
   void * play_media_user;

   int (*stop_media) (void * user);
   void * stop_media_user;

   /* parallel with demux thread */
   xthread_t *thread;
   /* stop the thread */
   int stop;
   /* queue protected by lock*/
   xlist_t *pending_queue;
   xthr_lock_t *pending_lock;
   /* thread running condiftion */
   xthr_cond_t *packet_pending;
};

int vsend_send (vorbis_sender_t *sender, ogg_packet * packet);

int vsend_set_callback (media_player_t * mp, int type, int(*call)(void*), void * user) {

   vorbis_sender_t *playa = (vorbis_sender_t *)mp;

   switch(type){

      case (CALLBACK_PLAY_MEDIA):

         playa->play_media = call;
         playa->play_media_user = user;
         vsend_log(("vorbis_set_callback: 'media_sent' callback added\n"));
         break;

      case (CALLBACK_STOP_MEDIA):
      
         playa->stop_media = call;
         playa->stop_media_user = user;
         vsend_log(("vorbis_set_callback: 'media_received' callback added\n"));
         break;
         
      default:

         vsend_log(("< vorbis_set_callback: callback[%d] unsupported >\n", type));
         return MP_EUNSUP;
   }

   return MP_OK;
}

int vsend_match_type (media_player_t * mp, char *mime, char *fourcc) {

   /* FIXME: Due to no strncasecmp on win32 mime is case sensitive */
   if (mime && strncmp(mime, "audio/vorbis", 12) == 0)
      return 1;

   return 0;
}

int vsend_loop(void *gen){

	vorbis_sender_t *vs = (vorbis_sender_t*)gen;
	media_player_t *mp = (media_player_t*)vs;

	rtp_frame_t *rtpf = NULL;

    media_pipe_t *output = mp->device->pipe(mp->device);
	vs->stop = 0;
	while(1)
	{
		xthr_lock(vs->pending_lock);

		if (vs->stop)
		{			
			xthr_unlock(vs->pending_lock);
			break;
		}

		if (xlist_size(vs->pending_queue) == 0) 
		{
			//vsend_log(("vsend_loop: no packet waiting, sleep!\n"));
			xthr_cond_wait(vs->packet_pending, vs->pending_lock);
			//vsend_log(("vsend_loop: wakeup! %d packets pending\n", xlist_size(vs->pending_queue)));
		}

		/* sometime it's still empty, weired? */
		if (xlist_size(vs->pending_queue) == 0)
		{
			xthr_unlock(vs->pending_lock);
			continue;
		}

		rtpf = (rtp_frame_t*)xlist_remove_first(vs->pending_queue);

		xthr_unlock(vs->pending_lock);

		output->put_frame (output, (media_frame_t*)rtpf, rtpf->rtp_flag & MP_RTP_LAST);
		output->recycle_frame(output, (media_frame_t*)rtpf);
	}

	return MP_OK;
}

int vsend_open_stream (media_player_t *mp, media_info_t *media_info) {

   vorbis_sender_t *vs = NULL;
   
   vorbis_info_t *vinfo = (vorbis_info_t *)media_info;
   
   struct audio_info_s ai;
   
   int ret;
   
   if (!mp->device) {
     
      vsend_log (("vsend_open_stream: No device to play vorbis audio\n"));
      return MP_FAIL;
   }
   
   vs = (vorbis_sender_t *)mp;
   
   vs->vorbis_info = vinfo;
   
   ai.info.sample_rate = vinfo->vi.rate;
   ai.info.sample_bits = 16;
   ai.channels = vinfo->vi.channels;
   
   ret = mp->device->start (mp->device, &ai, vs->dad_min_ms, vs->dad_max_ms);
   
   if (ret < MP_OK) {

      vsend_log (("vorbis_open_stream: vorbis stream fail to open\n"));

      return ret;
   }

   /* thread start */
   vs->pending_queue = xlist_new();
   vs->pending_lock = xthr_new_lock();
   vs->packet_pending = xthr_new_cond(XTHREAD_NONEFLAGS);
   vs->thread = xthr_new(vsend_loop, vs, XTHREAD_NONEFLAGS);
   
   return ret;
}

int vsend_close_stream (media_player_t * mp) {

   vsend_debug (("vorbis_close_stream: Not implemented yet\n"));
   
   return MP_EIMPL;
}

int vsend_stop (media_player_t *mp) {

   vsend_debug(("vorbis_stop: to stop vorbis sender\n"));
   mp->device->stop (mp->device);

   return MP_OK;
}

int vsend_receive_next (media_player_t *mp, void * vorbis_packet, int64 samplestamp, int last_packet) {

   media_pipe_t * output = NULL;
   rtp_frame_t * rtpf = NULL;
   
   vorbis_sender_t *vs = (vorbis_sender_t *)mp;
   ogg_packet *pack = (ogg_packet*)vorbis_packet;

   if (!mp->device) {
   
      vsend_debug(("vsend_receive_next: No device to send vorbis audio\n"));
      return MP_FAIL;
   }
   
   output = mp->device->pipe(mp->device);

   /**
    * Prepare frame for packet sending
	*
    * NOTE: DATA IS CLONED!
    */
   rtpf = (rtp_frame_t *)output->new_frame(output, pack->bytes, pack->packet);

   /* Now samplestamp is 64 bits, for maximum media stamp possible
	* All param for sending stored in the frame
	*/
   rtpf->samplestamp = samplestamp; 
   rtpf->media_info = vs->vorbis_info;

   if(last_packet) rtpf->rtp_flag |= MP_RTP_LAST;

   xthr_lock(vs->pending_lock);
   xlist_addto_last(vs->pending_queue, rtpf);
   xthr_unlock(vs->pending_lock);
   
   xthr_cond_signal(vs->packet_pending);

   /* job done by new_frame
   rtpf->unit_bytes = pack->bytes;
   memcpy(rtpf->media_unit, pack->packet, pack->bytes);

   output->put_frame (output, (media_frame_t*)rtpf, last_packet);
   output->recycle_frame(output, (media_frame_t*)rtpf);
   */
   
   return MP_OK;
}

const char* vsend_play_type(media_player_t * mp){

   return global_const.play_type;
}

const char* vsend_media_type(media_player_t * mp){

   return global_const.media_type;
}

const char* vsend_codec_type(media_player_t * mp){

   return "";
}

media_pipe_t * vsend_pipe(media_player_t * p) {

   if (!p->device) return NULL;

   return p->device->pipe (p->device);
}

int vsend_done(media_player_t *mp) {

   mp->device->done (mp->device);

   free((vorbis_sender_t *)mp);

   return MP_OK;
}

int vsend_set_options (media_player_t * mp, char *opt, void *value) {

   vsend_log(("vorbis_set_options: NOP function\n"));

   return MP_OK;
}

int vsend_done_device ( void *gen ) {

   media_device_t* dev = (media_device_t*)gen;

   dev->done(dev);

   return MP_OK;
}

int vsend_set_device (media_player_t * mp, media_control_t *cont, module_catalog_t *cata) {

   control_setting_t *setting = NULL;

   xrtp_list_user_t $lu;
   xrtp_list_t * devs = xrtp_list_new();

   media_device_t *dev = NULL;

   vsend_log(("vsend_set_device: need netcast device\n"));

   if (catalog_create_modules(cata, "device", devs) <= 0) {

      vsend_log(("vsend_set_device: no device module found\n"));

      xrtp_list_free(devs, NULL);

      return MP_FAIL;
   }

   dev = xrtp_list_first(devs, &$lu);
   while (dev) {

      if (dev->match_type(dev, "rtp")) {

         vsend_log(("vsend_set_device: found rtp device\n"));

         xrtp_list_remove_item(devs, dev);

         break;
      }

      dev = xrtp_list_next(devs, &$lu);
   }

   xrtp_list_free(devs, vsend_done_device);

   if(!dev) return MP_FAIL;

   setting = cont->fetch_setting(cont, "rtp", dev);
   if(setting){
        
      ((rtp_setting_t*)setting)->profile_mime = "audio/vorbis";

      dev->setting(dev, setting, cata);
      mp->device = dev;

      setting->done(setting);

      return MP_OK;
   }
      
   vsend_debug(("vsend_set_device: Can't set rtp device properly\n"));

   return MP_FAIL;
}

module_interface_t * media_new_sender(){

   media_player_t *mp = NULL;

   vorbis_sender_t *sender = malloc(sizeof(struct vorbis_sender_s));
   if(!sender){

      vsend_debug(("vorbis.new_sender: No memory to allocate\n"));

      return NULL;
   }

   memset(sender, 0, sizeof(struct vorbis_sender_s));

   mp = (media_player_t *)sender;

   mp->done = vsend_done;
   
   mp->play_type = vsend_play_type;
   mp->media_type = vsend_media_type;
   mp->codec_type = vsend_codec_type;

   mp->set_callback = vsend_set_callback;
   mp->pipe = vsend_pipe;

   mp->match_type = vsend_match_type;
   
   mp->open_stream = vsend_open_stream;
   mp->close_stream = vsend_close_stream;

   mp->receive_media = vsend_receive_next;
   mp->set_options = vsend_set_options;

   mp->set_device = vsend_set_device;

   mp->stop = vsend_stop;

   return mp;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC
module_loadin_t mediaformat = {

   "netcast",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   media_new_sender   /* Module initializer */
};
                   
