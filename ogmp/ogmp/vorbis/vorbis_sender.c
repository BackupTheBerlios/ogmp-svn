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

   int64 recent_samplestamp;

   int group_packets;

   char *cname;
   int cnlen;

   xrtp_session_t *rtp_session;
   xrtp_media_t *rtp_media;

   int session_id; /* unique in rtp device space */

   int (*play_media) (void * user);
   void * play_media_user;

   int (*stop_media) (void * user);
   void * stop_media_user;
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

int vsend_open_stream (media_player_t *mp, media_info_t *media_info) {

   vorbis_sender_t *vs = NULL;
   
   vorbis_info_t *vinfo = (vorbis_info_t *)media_info;
   
   struct audio_info_s ai;
   
   if (!mp->device) {
     
      vsend_log (("vsend_open_stream: No device to play vorbis audio\n"));
      return MP_FAIL;
   }
   
   vs = (vorbis_sender_t *)mp;
   
   vs->vorbis_info = vinfo;
   
   ai.info.sample_rate = vinfo->vi.rate;
   ai.info.sample_bits = 16;
   ai.channels = vinfo->vi.channels;
   
   vs->rtp_media->set_rate(vs->rtp_media, ai.info.sample_rate);
   
   return MP_OK;
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

   rtime_t usec_delta = 0;
   int new_group = 0;

   clock_t *clock = session_clock(vs->rtp_session);

   /* varibles for samples counting */
   vorbis_info_t *vinfo;

   oggpack_buffer       *opb;
  
   int                  mode;
   int					blocksize, shortsize;
   int					samples_half1, samples_half2;

   if (!mp->device)
   {
      vsend_debug(("vsend_receive_next: No device to send vorbis audio\n"));
      return MP_FAIL;
   }

   /* verify ogg/vorbis packet */


   if(samplestamp != vs->recent_samplestamp)
   {
	   vsend_log(("vsend_receive_next: samples(%lld) ", samplestamp));
	   vsend_log(("start @%dus\n", time_usec_now(clock)));

	   vs->recent_samplestamp = samplestamp;
	   vs->group_packets = 0;

	   new_group = 1;
   }

   output = mp->device->pipe(mp->device);

   /**
    * Prepare frame for packet sending
	*
    * NOTE: DATA IS CLONED!
    */
   rtpf = (rtp_frame_t *)output->new_frame(output, pack->bytes, pack->packet);

   rtpf->frame.eos = (last_packet == MP_EOS);
   rtpf->frame.eots = last_packet;

   /* Now samplestamp is 64 bits, for maximum media stamp possible
	* All param for sending stored in the frame
	*/
   rtpf->samplestamp = samplestamp;
   
   rtpf->media_info = vs->vorbis_info;

   /* compute sample advance in each packet */
   vinfo = (vorbis_info_t*)rtpf->media_info;

   opb=&(vinfo->vb.opb);
  
   oggpack_readinit(opb,rtpf->media_unit,rtpf->unit_bytes);

   /* Check the packet type */
   if(oggpack_read(opb,1)!=0)
   {
	   vsend_log(("vsend_receive_next: not vorbis type\n"));
	   rtpf->frame.owner->recycle_frame(rtpf->frame.owner, (media_frame_t*)rtpf);

	   return MP_EFORMAT;
   }

   /* read our mode and pre/post windowsize */
   mode=oggpack_read(opb,vinfo->mode_bits);
   if(mode==-1)
   {
	   vsend_log(("vsend_receive_next: wrong mode\n"));
	   rtpf->frame.owner->recycle_frame(rtpf->frame.owner, (media_frame_t*)rtpf);

	   return MP_EFORMAT;
   }

   vinfo->vb.mode=mode;
   vinfo->vb.W=vinfo->some_csi->mode_param[mode]->blockflag;
   
   blocksize = vinfo->some_csi->blocksizes[vinfo->vb.W];
   /*
   vsend_log(("audio/vorbis.rtp_vorbis_post: blocksize = %d\n", blocksize));
   */

   if(vinfo->audio_start == 0)
   {
	   /* 
	   vsend_log(("audio/vorbis.rtp_vorbis_post: first packet has no sample\n"));
	   */
	   rtpf->samples = 0;
   }
   else
   {
	   /* Window type of the previous and next */
	   if(vinfo->vb.W)
	   {
		   /* long block */
		   vinfo->vb.lW=oggpack_read(opb,1);	   
		   vinfo->vb.nW=oggpack_read(opb,1);
	   
		   if(vinfo->vb.nW==-1)
		   {
			   /*
			   vsend_log(("audio/vorbis.rtp_vorbis_post: wrong window flay\n"));
			   */
			   rtpf->frame.owner->recycle_frame(rtpf->frame.owner, (media_frame_t*)rtpf);
			   return MP_EFORMAT;
		   }

		   if(vinfo->vb.lW)
		   {
			   /* long/(long) 
			   vsend_log(("audio/vorbis.rtp_vorbis_post: long/(long): %d/(%d)\n", blocksize, blocksize));
			   */
			   rtpf->samples = blocksize/2;
		   }
		   else
		   {
			   shortsize = vinfo->some_csi->blocksizes[vinfo->vb.lW];
			   samples_half1 = blocksize/4 + shortsize/4;
			   /*
			   vsend_log(("audio/vorbis.rtp_vorbis_post: short/(long): %d/(%d)\n", shortsize, blocksize));
			   */
			   if(vinfo->vb.nW)
			   {
				   /* (long)/long */
				   samples_half2 = 0;
				   /*
				   vsend_log(("audio/vorbis.rtp_vorbis_post: (long)/long: (%d)/%d\n", blocksize, blocksize));
				   */
			   }
			   else
			   {
				   /* (long)/short */
				   samples_half2 = blocksize/4 - shortsize/4;
				   /*
				   vsend_log(("audio/vorbis.rtp_vorbis_post: (long)/short: (%d)/%d\n", blocksize, shortsize));
				   */
			   }
		   
			   rtpf->samples = samples_half1 + samples_half2;
		   }
	   }
	   else
	   {
		   /* (short) 
		   vsend_log(("audio/vorbis.rtp_vorbis_post: short/(short): %d/(%d)\n", blocksize, blocksize));
		   */
		   blocksize = vinfo->some_csi->blocksizes[vinfo->vb.W];
		   rtpf->samples = blocksize/2;
	   }
   }

   vinfo->audio_start = 1;

   rtpf->grpno = ++vs->group_packets;
   //vsend_log(("audio/vorbis.rtp_vorbis_post: (%d) packets\n", rtpf->grpno));

   vs->rtp_media->post (vs->rtp_media, (media_data_t*)rtpf, pack->bytes, last_packet);

   if(last_packet)
   {
	   vsend_log(("vsend_receive_next: samples(%lld) ", samplestamp));
	   vsend_log(("end @%dus\n", time_usec_now(clock)));
   }

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

   media_device_t *dev = NULL;
   dev_rtp_t * dev_rtp = NULL;
   rtp_setting_t *rtpset = NULL;

   vorbis_sender_t *vs = (vorbis_sender_t*)mp;

   vsend_log(("vsend_set_device: need netcast device\n"));

   dev = cont->find_device(cont, "rtp");

   if(!dev) return MP_FAIL;

   dev_rtp = (dev_rtp_t*)dev;
   
   dev->start(dev);

   setting = cont->fetch_setting(cont, "rtp", dev);
   if(!setting)
   {
	   vsend_debug(("vsend_set_device: Can't set rtp device properly\n"));
	   return MP_FAIL;
   }

   rtpset = (rtp_setting_t*)setting;
        
   rtpset->profile_mime = "audio/vorbis";

   vs->cname = xstr_clone(rtpset->cname);
   vs->cnlen = rtpset->cnlen;

   vs->rtp_session = dev_rtp->new_session(dev_rtp, rtpset, cata);
   vs->rtp_media = session_media(vs->rtp_session);

   /* Get an unique id for search */
   vs->session_id = dev_rtp->session_id(dev_rtp, vs->rtp_session);

   mp->device = dev;

   setting->done(setting);

   return MP_OK;
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
                   
