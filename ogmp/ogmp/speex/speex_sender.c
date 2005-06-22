/***************************************************************************
                          speex_sender.c  -  Send Speex rtp stream
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
#include <ogg/ogg.h>

#include <timedia/xmalloc.h>
#include <timedia/xstring.h>
#include <timedia/list.h>
#include <timedia/ui.h>

#include <stdlib.h>

/*#include "dev_rtp.h"*/
#include "rtp_format.h"

#include "speex_info.h"

#define PROFILE_MIME "audio/speex"
/*
*/
#define SPXSENDER_LOG
#define SPXSENDER_DEBUG

#ifdef SPXSENDER_LOG
 #define spxs_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define spxs_log(fmtargs)
#endif

#ifdef SPXSENDER_DEBUG
 #define spxs_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define spxs_debug(fmtargs)
#endif

struct global_const_s
{
   char media_type[6];
   char mime_type[13];
   char play_type[6];

} global_const = {"audio", "audio/speex", "rtp"};

typedef struct speex_sender_s speex_sender_t;
struct speex_sender_s
{
   struct media_transmit_s transmit;

   /*int (*callback_on_ready) (void *user, media_player_t *player);*/
   void *callback_on_ready_user;
   int (*callback_on_ready)();

   /*int (*stop_media) (void *user);*/
   void *callback_on_media_stop;
   int (*callback_on_media_stop_user)();

   speex_info_t  *speex_info;

   int64 recent_samplestamp;

   int group_packets;

   char *cname;
   int cnlen;

   xrtp_session_t *rtp_session;
   xrtp_media_t *rtp_media;

   uint8 profile_no;

   char *nettype;
   char *addrtype;
   char *netaddr;

   uint16 rtp_port;
   uint16 rtcp_port;
};

int spxs_set_callback (media_player_t *mp, int type, int (*call)(), void *user)
{
   speex_sender_t *playa = (speex_sender_t *)mp;

   switch(type)
   {
      case (CALLBACK_PLAYER_READY):

         playa->callback_on_ready = call;
         playa->callback_on_ready_user = user;
         spxs_log(("spxs_set_callback: 'on_ready' callback added\n"));
         break;

      case (CALLBACK_MEDIA_STOP):
      
         playa->callback_on_media_stop = call;
         playa->callback_on_media_stop_user = user;
         spxs_log(("spxs_set_callback: 'on_media_stopped' callback added\n"));
         break;
         
      default:

         spxs_log(("spxs_set_callback: callback[%d] unsupported\n", type));
         return MP_EUNSUP;
   }

   return MP_OK;
}

int spxs_match_play_type (media_player_t * mp, char *play)
{
    /* FIXME: due to no strncasecmp on win32 mime is case sensitive */
	if (strcmp(play, "netcast") == 0)
		return 1;

	return 0;
}

int spxs_open_stream (media_player_t *mp, media_info_t *media_info)
{
   speex_sender_t *ss = NULL;
   
   speex_info_t *spxinfo = (speex_info_t *)media_info;

   struct audio_info_s ai;
   
   if (!mp->device)
   {
      spxs_log (("spxs_open_stream: No device to play speex voice\n"));
      return MP_FAIL;
   }
   
   ss = (speex_sender_t *)mp;
   
   ss->speex_info = spxinfo;
   
   ai.info.sample_bits = 16;
   ai.info.sample_rate = spxinfo->audioinfo.info.sample_rate;
   ai.channels = spxinfo->audioinfo.channels;
         
   ss->rtp_media->set_coding(ss->rtp_media, ai.info.sample_rate, ai.channels);
   ss->rtp_media->set_parameter(ss->rtp_media, "nframe_per_packet", (void*)spxinfo->nframe_per_packet);
   
   spxs_debug (("spxs_open_stream: channels[%d]\n", spxinfo->audioinfo.channels));
   spxs_log (("spxs_open_stream: speex stream opened\n"));
   
   return MP_OK;
}

int spxs_close_stream (media_player_t * mp)
{
   spxs_debug (("spxs_close_stream: Not implemented yet\n"));
   
   return MP_EIMPL;
}

const char* spxs_play_type(media_player_t * mp)
{
   return global_const.play_type;
}

char* spxs_media_type(media_player_t * mp)
{
   return global_const.media_type;
}

char* spxs_codec_type(media_player_t * mp)
{
   return "";
}

media_pipe_t * spxs_pipe(media_player_t * p)
{
   if (!p->device) return NULL;

   return p->device->pipe (p->device);
}

int spxs_done(media_player_t *mp)
{
	speex_sender_t *ss = (speex_sender_t *)mp;

	spxs_log(("spxs_done: FIXME - How to keep the pre-opened device\n"));

	if(mp->device)
		mp->device->done(mp->device);

	xstr_done_string(ss->nettype);
	xstr_done_string(ss->addrtype);
	xstr_done_string(ss->netaddr);

	xfree(mp);

	return MP_OK;
}

int spxs_set_options (media_player_t * mp, char *opt, void *value)
{
   spxs_log(("spxs_set_options: NOP function\n"));

   return MP_OK;
}

capable_descript_t* spxs_capable(media_player_t* mp, void* cap_info)
{
   rtpcap_sdp_t *sdp = (rtpcap_sdp_t*)cap_info;

   speex_sender_t *ss = (speex_sender_t *)mp;
   speex_info_t *spxinfo = ss->speex_info;
   rtpcap_descript_t *rtpcap;

   int ptime_max = 0;

   int penh = 0;

   spxs_log(("spxs_capable: #%d %s:%d/%d '%s' %dHz\n", ss->profile_no, ss->netaddr, ss->rtp_port, ss->rtcp_port, global_const.mime_type, spxinfo->audioinfo.info.sample_rate));
   
   ss->rtp_media->parameter(ss->rtp_media, "ptime_max", &ptime_max);
   ss->rtp_media->parameter(ss->rtp_media, "penh", &penh);

   spxinfo->ptime = ptime_max / (SPX_FRAME_MSEC * spxinfo->nframe_per_packet) * SPX_FRAME_MSEC;
   spxinfo->penh = penh;

   spxs_log(("spxs_capable: %d frames per packet, %dms per rtp packet, penh=%d\n", spxinfo->nframe_per_packet, spxinfo->ptime, spxinfo->penh));

   rtpcap = rtp_capable_descript(ss->profile_no, ss->nettype, ss->addrtype, ss->netaddr, ss->rtp_port, ss->rtcp_port, global_const.mime_type, spxinfo->audioinfo.info.sample_rate, spxinfo->audioinfo.channels, sdp->sdp_message);

   if(speex_info_to_sdp((media_info_t*)spxinfo, rtpcap, sdp->sdp_message, sdp->sdp_media_pos) >= MP_OK)
	   sdp->sdp_media_pos++;

   return (capable_descript_t*)rtpcap;
}

int spxs_match_capable(media_player_t * mp, capable_descript_t *cap)
{
   return cap->match_value(cap, "mime", global_const.mime_type, strlen(global_const.mime_type));
}

int spxs_done_device ( void *gen )
{
   media_device_t* dev = (media_device_t*)gen;

   dev->done(dev);

   return MP_OK;
}

int spxs_on_member_update(void *gen, uint32 ssrc, char *cn, int cnlen)
{
   speex_sender_t *vs = (speex_sender_t*)gen;

   spxs_debug(("spxs_on_member_update: dest[%s] connected\n\n\n", cn));

   if(vs->callback_on_ready)
      vs->callback_on_ready(vs->callback_on_ready_user, (media_player_t*)vs);

   return MP_OK;
}

int spxs_set_device(media_player_t *mp, media_control_t *cont, module_catalog_t *cata, void* extra)
{
	control_setting_t *setting = NULL;

	/* extra is cast to rtpcapset for rtp device */
	rtpcap_set_t *rtpcapset = (rtpcap_set_t*)extra;
	user_profile_t *user;

	rtpcap_descript_t *rtpcap;


	media_device_t *dev = NULL;

	dev_rtp_t * dev_rtp = NULL;
	rtp_setting_t *rtpset = NULL;
	speex_setting_t *spxset = NULL;

	speex_sender_t *ss = (speex_sender_t*)mp;

	int total_bw;

	user = rtpcapset->user_profile;

	rtpcap = rtp_get_capable(rtpcapset, global_const.mime_type);
	if(!rtpcap)
	{
		spxs_debug(("spxs_set_device: No speex capable found\n"));
		return MP_FAIL;
	}       

	dev = cont->find_device(cont, "rtp", "output");
	if(!dev)
	{
		spxs_debug(("spxs_set_device: No rtp device found\n"));
		return MP_FAIL;
	}

	dev_rtp = (dev_rtp_t*)dev;
   
	dev->start(dev, DEVICE_OUTPUT);

	setting = cont->fetch_setting(cont, "rtp", dev);

	if(!setting)
	{
		spxs_debug(("spxs_set_device: Can't set rtp device properly\n"));
		return MP_FAIL;
	}

	rtpset = (rtp_setting_t*)setting;

	spxset = speex_setting(cont);

	total_bw = cont->book_bandwidth(cont, 0);
   
	ss->profile_no = rtpcap->profile_no;

	ss->nettype = xstr_clone(user->user->nettype);
	ss->addrtype = xstr_clone(user->user->addrtype);
	ss->netaddr = xstr_clone(user->user->netaddr);

	ss->rtp_port = rtpcap->local_rtp_portno;
	ss->rtcp_port = rtpcap->local_rtcp_portno;

	ss->cname = xstr_clone(user->cname);
	ss->cnlen = strlen(user->cname)+1;

	ss->rtp_session = dev_rtp->rtp_session(dev_rtp, cata, cont,
								ss->cname, ss->cnlen,
								user->user->nettype, user->user->addrtype, user->user->netaddr, 
								(uint16)rtpcap->local_rtp_portno, (uint16)rtpcap->local_rtcp_portno,
								rtpcap->profile_no, rtpcap->profile_mime,
								rtpcap->clockrate, rtpcap->coding_param,
								total_bw, rtpcap);

	ss->rtp_media = session_media(ss->rtp_session);

	ss->rtp_media->set_parameter(ss->rtp_media, "ptime_max", (void*)spxset->ptime_max);
	ss->rtp_media->set_parameter(ss->rtp_media, "penh", (void*)spxset->penh);

   spxs_debug(("spxs_set_device: rtp_session@%x\n", (int)ss->rtp_session));
   spxs_debug(("spxs_set_device: spxs_on_member_update@%x\n\n\n", (int)spxs_on_member_update));

	session_set_callback(ss->rtp_session, CALLBACK_SESSION_MEMBER_UPDATE, spxs_on_member_update, ss);

	mp->device = dev;

	setting->done(setting);

	spxs_debug(("spxs_set_device: mp->device@%x ok\n", (int)mp->device));

	return MP_OK;
}

/*  */
int spxs_link_stream(media_player_t *mp, media_stream_t* strm, media_control_t *cont, void* extra)
{
	control_setting_t *setting = NULL;
 
	/* extra is cast to rtpcapset for rtp device */
	rtpcap_set_t *rtpcapset = (rtpcap_set_t*)extra;
	user_profile_t *user;

	media_device_t *dev = NULL;

	dev_rtp_t * dev_rtp = NULL;
	rtp_setting_t *rtpset = NULL;
	speex_setting_t *spxset = NULL;

	speex_sender_t *ss = (speex_sender_t*)mp;
   rtp_stream_t *rtpstrm = (rtp_stream_t*)strm;

	int total_bw;

	user = rtpcapset->user_profile;
   
   dev_rtp = (dev_rtp_t*)cont->find_device(cont, "rtp", "output");
	/*dev_rtp = rtpstrm->rtp_format->rtp_in;*/

   dev = (media_device_t*)dev_rtp;
   
	setting = cont->fetch_setting(cont, "rtp", dev);
	if(!setting)
	{
		spxs_debug(("spxs_set_device: Can't set rtp device properly\n"));
		return MP_FAIL;
	}

	rtpset = (rtp_setting_t*)setting;

	spxset = speex_setting(cont);

	total_bw = cont->book_bandwidth(cont, 0);

	ss->profile_no = rtpstrm->rtp_cap->profile_no;

	ss->nettype = xstr_clone(user->user->nettype);
	ss->addrtype = xstr_clone(user->user->addrtype);
	ss->netaddr = xstr_clone(user->user->netaddr);

	ss->rtp_port = rtpstrm->rtp_cap->local_rtp_portno;
	ss->rtcp_port = rtpstrm->rtp_cap->local_rtcp_portno;

	ss->cname = xstr_clone(user->cname);

	ss->cnlen = strlen(user->cname)+1;

   ss->rtp_session = rtpstrm->session;
   ss->rtp_media = session_media(ss->rtp_session);

	ss->rtp_media->set_parameter(ss->rtp_media, "ptime_max", (void*)spxset->ptime_max);
	ss->rtp_media->set_parameter(ss->rtp_media, "penh", (void*)spxset->penh);
	/*
   session_set_callback(ss->rtp_session, CALLBACK_SESSION_MEMBER_UPDATE, spxs_on_member_update, ss);
   */
	mp->device = dev;

	setting->done(setting);

	strm->player = mp;

   spxs_open_stream (mp, strm->media_info);

	return MP_OK;
}

int spxs_init (media_player_t * mp, media_control_t *control, void* data)
{
	speex_sender_t *ss = (speex_sender_t*)mp;

   ss->rtp_session = (xrtp_session_t*)data;

	return MP_OK;
}

int spxs_stop (media_player_t *mp)
{
   spxs_debug(("spxs_stop: to stop speex sender\n"));
   mp->device->stop (mp->device, DEVICE_OUTPUT);

   return MP_OK;
}

/**************************************************************/
int spxs_match_type (media_receiver_t *recvr, char *mime, char *fourcc)
{
   /* FIXME: Due to no strncasecmp on win32 mime is case sensitive */
   if (mime && strncmp(mime, "audio/speex", 12) == 0)
      return 1;

   return 0;
}

int spxs_receive_next(media_receiver_t *recvr, media_frame_t* spxf, int64 samplestamp, int last_packet)
{
   media_pipe_t *output = NULL;
   rtp_frame_t *rtpf = NULL;
   media_player_t *mp = (media_player_t*)recvr;
   
   speex_sender_t *ss = (speex_sender_t *)mp;

   media_frame_t* mf = (media_frame_t*)spxf;

   int new_group = 0;

   /* varibles for samples counting */
   speex_info_t *spxinfo;

   /*
    * packetno from 0,1 is head (maybe more if has extra header), then increase by 1 to last packet
	 * always granulepos[-1] in same spx file.
    */
   if (!mp->device)
   {
      spxs_debug(("spxs_receive_next: No device to send speex voice\n"));
      return MP_FAIL;
   }

   if(samplestamp != ss->recent_samplestamp)
   {
      spxs_log(("....................................................\n"));
	   spxs_log(("spxs_receive_next: samples(%lld) ", samplestamp));

	   ss->recent_samplestamp = samplestamp;
	   ss->group_packets = 0;

	   new_group = 1;
   }

   output = mp->device->pipe(mp->device);

   /**
    * Prepare frame for packet sending
    * NOTE: DATA IS CLONED!
   spxs_debug(("spxs_receive_next: output->new_frame@%x\n", (int)output->new_frame));
    */
   
   rtpf = (rtp_frame_t *)output->new_frame(output, mf->bytes, mf->raw);

   rtpf->frame.eos = (last_packet == MP_EOS);

   rtpf->frame.eots = last_packet;

   /* Now samplestamp is 64 bits, for maximum media stamp possible
	 * All param for sending stored in the frame
	 */
    
   spxinfo = ss->speex_info;

   rtpf->samples = spxinfo->nframe_per_packet * spxinfo->nsample_per_frame;

   rtpf->media_info = ss->speex_info;

   rtpf->samplestamp = samplestamp;
   
   spxinfo->audio_start = 1;

   rtpf->grpno = ++ss->group_packets;

   spxs_debug(("spxs_receive_next: rtpf samplestamp[%lld], bytes[%d]\n", rtpf->samplestamp, rtpf->frame.bytes));
   ss->rtp_media->post(ss->rtp_media, (media_data_t*)rtpf, rtpf->frame.bytes, last_packet);

   if(last_packet)
   {
      spxs_log(("spxs_receive_next: last samples(%lld) ", samplestamp));
   }                 

   return MP_OK;
}

/**************************************************************/

int spxs_add_destinate(media_transmit_t *send, char *cname, char *nettype, char *addrtype, char *netaddr, int rtp_pno, int rtcp_pno)
{
	/* media info for sending stored in session.self */

	return session_add_cname(((speex_sender_t*)send)->rtp_session, cname, strlen(cname)+1, netaddr, (uint16)rtp_pno, (uint16)rtcp_pno, /* (rtpcap_descript_t*) */NULL, NULL);
}

int spxs_remove_destinate(media_transmit_t *send, char *cname, char *nettype, char *addrtype, char *netaddr, int rtp_pno, int rtcp_pno)
{
	return session_delete_cname(((speex_sender_t*)send)->rtp_session, cname, strlen(cname)+1);
}

xrtp_session_t* spxs_session(media_transmit_t *send)
{
	return ((speex_sender_t*)send)->rtp_session;
}

module_interface_t * media_new_sender()
{
   media_player_t *mp = NULL;

   speex_sender_t *sender = xmalloc(sizeof(struct speex_sender_s));
   if(!sender)
   {
      spxs_debug(("speex.new_sender: No memory to allocate\n"));
      return NULL;
   }

   memset(sender, 0, sizeof(struct speex_sender_s));

   mp = (media_player_t*)sender;
   mp->done = spxs_done;

   mp->media_type = spxs_media_type;
   mp->codec_type = spxs_codec_type;

   mp->set_callback = spxs_set_callback;
   mp->pipe = spxs_pipe;

   mp->match_play_type = spxs_match_play_type;
   
   mp->init = spxs_init;

   mp->open_stream = spxs_open_stream;
   mp->close_stream = spxs_close_stream;
   mp->set_options = spxs_set_options;

   mp->set_device = spxs_set_device;
   mp->link_stream = spxs_link_stream;

   mp->capable = spxs_capable;
   mp->match_capable = spxs_match_capable;

   mp->stop = spxs_stop;

   mp->receiver.match_type = spxs_match_type;
   mp->receiver.receive_media = spxs_receive_next;

   sender->transmit.add_destinate = spxs_add_destinate;
   sender->transmit.remove_destinate = spxs_remove_destinate;

   sender->transmit.session = spxs_session;

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
                   
