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

#include "vorbis_info.h"

#define PROFILE_MIME "audio/vorbis"

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

struct global_const_s
{
   char media_type[6];
   char mime_type[13];
   char play_type[6];

} global_const = {"audio", "audio/vorbis", "rtp"};

typedef struct vorbis_sender_s vorbis_sender_t;
struct vorbis_sender_s
{
   struct media_transmit_s sender;

   /*int (*callback_on_ready) (void *user, media_player_t *player);*/
   void *callback_on_ready_user;
   int (*callback_on_ready)();

   /*int (*stop_media) (void *user);*/
   void *stop_media_user;
   int (*stop_media)();

   vorbis_info_t  *vorbis_info;
   int vorbis_info_signum;

   int64 recent_samplestamp;

   int group_packets;

   char *cname;
   int cnlen;

   xrtp_session_t *rtp_session;
   xrtp_media_t *rtp_media;

   uint8 profile_no;

   char *ipaddr;
   uint16 rtp_port;
   uint16 rtcp_port;
};

int vsend_set_callback (media_player_t *mp, int type, int (*call)(), void *user)
{
   vorbis_sender_t *playa = (vorbis_sender_t *)mp;


   switch(type)
   {
      case (CALLBACK_PLAYER_READY):

         playa->callback_on_ready = call;
         playa->callback_on_ready_user = user;
         vsend_log(("vorbis_set_callback: 'on_ready' callback added\n"));
         break;

      case (CALLBACK_STOP_MEDIA):
      
         playa->stop_media = call;
         playa->stop_media_user = user;
         vsend_log(("vorbis_set_callback: 'media_received' callback added\n"));
         break;
         
      default:

         vsend_log(("vorbis_set_callback: callback[%d] unsupported\n", type));
         return MP_EUNSUP;
   }

   return MP_OK;
}

int vsend_match_type (media_player_t * mp, char *mime, char *fourcc)
{
   /* FIXME: Due to no strncasecmp on win32 mime is case sensitive */
   if (mime && strncmp(mime, "audio/vorbis", 12) == 0)
      return 1;

   return 0;
}

int vsend_open_stream (media_player_t *mp, media_info_t *media_info)
{
   vorbis_sender_t *vs = NULL;
   
   vorbis_info_t *vinfo = (vorbis_info_t *)media_info;

   struct audio_info_s ai;
   
   if (!mp->device)
   {
      vsend_log (("vsend_open_stream: No device to play vorbis audio\n"));
      return MP_FAIL;
   }
   
   vs = (vorbis_sender_t *)mp;
   
   vs->vorbis_info = vinfo;
   
   ai.info.sample_rate = vinfo->vi.rate;
   ai.info.sample_bits = 16;
   ai.channels = vinfo->vi.channels;
   
   vs->rtp_media->set_rate(vs->rtp_media, ai.info.sample_rate);

   session_require_mediainfo(vs->rtp_session, vinfo, vs->vorbis_info_signum++);
   
   vsend_log (("vsend_open_stream: vorbis stream opened\n"));
   
   return MP_OK;
}

int vsend_close_stream (media_player_t * mp)
{
   vsend_debug (("vorbis_close_stream: Not implemented yet\n"));
   
   return MP_EIMPL;
}

int vsend_stop (media_player_t *mp)
{
   vsend_debug(("vorbis_stop: to stop vorbis sender\n"));
   mp->device->stop (mp->device);

   return MP_OK;
}

int vsend_receive_next (media_player_t *mp, void *vorbis_packet, int64 samplestamp, int last_packet)
{
   media_pipe_t *output = NULL;
   rtp_frame_t *rtpf = NULL;
   

   vorbis_sender_t *vs = (vorbis_sender_t *)mp;
   ogg_packet *pack = (ogg_packet*)vorbis_packet;

   //rtime_t usec_delta = 0;
   int new_group = 0;

   xclock_t *clock = session_clock(vs->rtp_session);

   /* varibles for samples counting */
   vorbis_info_t *vinfo;

   oggpack_buffer       *opb;
  
   int     mode;
   int     blocksize, shortsize;
   int     samples_half1, samples_half2;

   if (!mp->device)
   {
      vsend_debug(("vsend_receive_next: No device to send vorbis audio\n"));
      return MP_FAIL;
   }

   /* verify ogg/vorbis packet */

   if(samplestamp != vs->recent_samplestamp)
   {
       vsend_log(("....................................................\n"));
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

const char* vsend_play_type(media_player_t * mp)
{
   return global_const.play_type;
}

const char* vsend_media_type(media_player_t * mp)
{
   return global_const.media_type;
}

const char* vsend_codec_type(media_player_t * mp)
{
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

int vsend_set_options (media_player_t * mp, char *opt, void *value)
{
   vsend_log(("vorbis_set_options: NOP function\n"));

   return MP_OK;
}

capable_descript_t* vsend_capable(media_player_t * mp)
{
	rtpcap_descript_t *rtpcap;
	vorbis_sender_t *vs = (vorbis_sender_t *)mp;

	rtpcap = (rtpcap_descript_t*)rtp_capable_descript();
	if(rtpcap)
	{
		rtpcap->profile_mime = xstr_clone(global_const.mime_type);
		rtpcap->profile_no = vs->profile_no;
		rtpcap->ipaddr = xstr_clone(vs->ipaddr);
		rtpcap->rtp_portno = vs->rtp_port;
		rtpcap->rtcp_portno = vs->rtcp_port;
	}

	return (capable_descript_t*)rtpcap;
}

int vsend_match_capable(media_player_t * mp, capable_descript_t *cap)
{
   rtpcap_descript_t *rtpcap;

   if(!cap->match_type(cap, "rtp"))
      return 0;

   rtpcap = (rtpcap_descript_t*)cap;

	return !strncmp(global_const.mime_type, rtpcap->profile_mime, strlen(global_const.mime_type));
}

/**************************************************************/

int vsend_done_device ( void *gen ) {

   media_device_t* dev = (media_device_t*)gen;

   dev->done(dev);

   return MP_OK;
}

int vsend_on_member_update(void *gen, uint32 ssrc, char *cn, int cnlen)
{
   vorbis_sender_t *vs = (vorbis_sender_t*)gen;

   vsend_log(("vsend_on_member_update: dest[%s] connected\n", cn));
   
   if(vs->callback_on_ready)
	   vs->callback_on_ready(vs->callback_on_ready_user, (media_player_t*)vs);

   return MP_OK;
}

int vsend_set_device (media_player_t *mp, media_control_t *cont, module_catalog_t *cata)
{
   control_setting_t *setting = NULL;

   media_device_t *dev = NULL;
   dev_rtp_t * dev_rtp = NULL;
   rtp_setting_t *rtpset = NULL;

   vorbis_sender_t *vs = (vorbis_sender_t*)mp;

   uint8 profile_no;
   uint16 rtp_portno;
   uint16 rtcp_portno;
   int total_bw;
   int rtp_bw;

   int i;

   vsend_log(("vsend_set_device: need netcast device\n"));

   dev = cont->find_device(cont, "rtp");

   if(!dev) return MP_FAIL;

   dev_rtp = (dev_rtp_t*)dev;
   
   dev->start(dev, cont);

   setting = cont->fetch_setting(cont, "rtp", dev);
   if(!setting)
   {
	   vsend_debug(("vsend_set_device: Can't set rtp device properly\n"));
	   return MP_FAIL;
   }

   rtpset = (rtp_setting_t*)setting;

   rtp_portno = rtpset->default_rtp_portno;  
   rtcp_portno = rtpset->default_rtcp_portno;
   
   for(i=0; i<rtpset->nprofile; i++)
   {
	   if(strcmp(rtpset->profiles[i].profile_mime, PROFILE_MIME))
	   {
		   total_bw = rtpset->profiles[i].total_bw;
		   rtp_bw = rtpset->profiles[i].rtp_bw;
		   profile_no = rtpset->profiles[i].profile_no;

		   if(rtpset->profiles[i].rtp_portno)
			   rtp_portno = rtpset->profiles[i].rtp_portno;

		   if(rtpset->profiles[i].rtcp_portno)
			   rtp_portno = rtpset->profiles[i].rtcp_portno;
	   }
   }

   vs->cname = xstr_clone(rtpset->cname);
   vs->cnlen = rtpset->cnlen;

   vs->profile_no = profile_no;
   vs->ipaddr = xstr_clone(rtpset->ipaddr);
   vs->rtp_port = rtp_portno;
   vs->rtcp_port = rtcp_portno;

   vs->rtp_session = dev_rtp->rtp_session(dev_rtp, cata, cont,
								rtpset->cname, rtpset->cnlen,
								rtpset->ipaddr, rtp_portno, rtcp_portno,
								profile_no, PROFILE_MIME,
								total_bw, rtp_bw);

   vs->rtp_media = session_media(vs->rtp_session);

   session_set_callback(vs->rtp_session, CALLBACK_SESSION_MEMBER_UPDATE, vsend_on_member_update, vs);

   mp->device = dev;

   setting->done(setting);

   return MP_OK;
}

int vsend_add_destinate(media_transmit_t *send, char *cname, int cnlen, char *ipaddr, uint16 rtp_pno, uint16 rtcp_pno)
{
	return session_add_cname(((vorbis_sender_t*)send)->rtp_session, cname, cnlen, ipaddr, rtp_pno, rtcp_pno, NULL);
}

int vsend_delete_destinate(media_transmit_t *send, char *cname, int cnlen)
{
	return session_delete_cname(((vorbis_sender_t*)send)->rtp_session, cname, cnlen);
}

module_interface_t * media_new_sender()
{
   media_player_t *mp = NULL;
   media_transmit_t *mt = NULL;

   vorbis_sender_t *sender = malloc(sizeof(struct vorbis_sender_s));
   if(!sender)
   {
      vsend_debug(("vorbis.new_sender: No memory to allocate\n"));

      return NULL;
   }

   memset(sender, 0, sizeof(struct vorbis_sender_s));

   mp = (media_player_t*)sender;
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

   mp->capable = vsend_capable;
   mp->match_capable = vsend_match_capable;

   mp->stop = vsend_stop;

   mt = (media_transmit_t*)sender;

   mt->add_destinate = vsend_add_destinate;
   mt->delete_destinate = vsend_delete_destinate;

   return mp;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC
module_loadin_t mediaformat =
{
   "netcast",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   media_new_sender   /* Module initializer */
};
                   
