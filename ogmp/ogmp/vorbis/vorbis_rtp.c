/***************************************************************************
                          vorbis_rtp.c  -  vorbis profile for rtp
                             -------------------
    begin                : Mon May 17 2004
    copyright            : (C) 2004 by Heming
    email                : heming@bigfoot.com; heming@sf.net
 ***************************************************************************/
 
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "../devices/dev_rtp.h"
#include "vorbis_info.h"

#include <timedia/xthread.h>
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define VRTP_LOG
#define VRTP_DEBUG
 
#ifdef VRTP_LOG
 #define vrtp_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define vrtp_log(fmtargs)
#endif

#ifdef VRTP_DEBUG
 #define vrtp_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define vrtp_debug(fmtargs)
#endif

#define MAX_PAYLOAD_BYTES 1280 /* + udp header < 1500 bytes */
#define MAX_BUNDLE_PACKET_BYTES 256
#define MAX_PACKET_BUNDLE 32

const char vorbis_mime[] = "audio/vorbis";

char vrtp_desc[] = "Vorbis Profile for RTP";

int num_handler;

module_loadin_t xrtp_handler;

profile_class_t * vrtp;

typedef struct vrtp_handler_s vrtp_handler_t;
typedef struct vrtp_media_s vrtp_media_t;

struct vrtp_handler_s{

   struct profile_handler_s profile;

   void * master;

   vrtp_media_t * vorbis_media;

   xrtp_session_t * session;

   uint rtp_size;
   uint rtcp_size;

   uint32 seqno_next_unit;

   int new_group;
   rtp_frame_t *group_first_frame;

   /* a queue of vorbis packet to bundle into one rtp packet */
   xlist_t *packets;

   buffer_t *payload_buf; /* include paylaod head */
   uint32 usec_payload_timestamp; /* of current payload, frome randem value */
   uint32 usec_group_timestamp;

   /* thread attribute */
   xthread_t *thread;

   int64 recent_samplestamp;
   rtp_frame_t *recent_frame;

   int run;
   int idle;

   xthr_lock_t *packets_lock;
   xthr_cond_t *pending;
};

struct vrtp_media_s{

   struct xrtp_media_s rtp_media;

   vrtp_handler_t * vorbis_profile;

   //uint32 dolen;
   //media_data_t * data_out;
};

/**
 * Handle inward rtp packet
 */
int vrtp_rtp_in(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){

   vrtp_handler_t *vh = (vrtp_handler_t *)handler;
   
   vrtp_media_t *vmedia = vh->vorbis_media;
   xrtp_media_t *rtpmedia = (xrtp_media_t*)vmedia;

   uint16 seqno, src, rtpts;
   
   member_state_t * sender = NULL;
   session_connect_t *rtp_conn = NULL;
   session_connect_t *rtcp_conn = NULL;

   char *media_data;
   int media_dlen;
   xrtp_hrtime_t later = 0;

   rtp_unpack(rtp);

   seqno = rtp_packet_seqno(rtp);
   src = rtp_packet_ssrc(rtp);
   rtpts = rtp_packet_timestamp(rtp);

   vrtp_log(("audio/vorbis.vrtp_rtp_in: packet ssrc=%u, seqno=%u, timestamp=%u, rtp->connect=%d\n", src, seqno, rtpts, (int)rtp->connect));

   sender = session_member_state(rtp->session, src);

   if(sender && sender->valid && !connect_match(rtp->connect, sender->rtp_connect)){

      session_solve_collision(sender, src);

      rtp_packet_done(rtp);

      return XRTP_CONSUMED;
   }

   if(!sender){

      /* The rtp packet is recieved before rtcp arrived, so the participant is not identified yet */
      if(!vh->session->$state.receive_from_anonymous){

         vrtp_log(("audio/vorbis.vrtp_rtp_in: participant waiting for identifed before receiving media\n"));
         rtp_packet_done(rtp);
         return XRTP_CONSUMED;
      }

      /* Get connect of the participant */
      rtp_conn = rtp->connect;
      rtcp_conn = NULL;

      rtp->connect = NULL; /* Dump the connect from rtcp packet */

      sender = session_new_member(vh->session, src, NULL);
      if(!sender){

         /* Something WRONG!!! packet disposed */
         rtp_packet_done(rtp);

         return XRTP_CONSUMED;
      }

      if(vh->session->$callbacks.member_connects){

         vh->session->$callbacks.member_connects(vh->session->$callbacks.member_connects_user, src, &rtp_conn, &rtcp_conn);
         session_member_set_connects(sender, rtp_conn, rtcp_conn);

      }else{

         rtcp_conn = connect_rtp_to_rtcp(rtp->connect); /* For RFC1889 static port allocation: rtcp port = rtp port + 1 */
         session_member_set_connects(sender, rtp->connect, rtcp_conn);
      }

      if(vh->session->join_to_rtp_port && connect_from_teleport(rtp->connect, vh->session->join_to_rtp_port)){

         /* participant joined, clear the join desire */
         teleport_done(vh->session->join_to_rtp_port);
         vh->session->join_to_rtp_port = NULL;
         teleport_done(vh->session->join_to_rtcp_port);
         vh->session->join_to_rtcp_port = NULL;
      }
   }

   if(!sender->valid){

      /* The rtp packet is recieved before rtcp arrived, so the participant is not identified yet */
      if(!vh->session->$state.receive_from_anonymous){

         vrtp_log(("audio/vorbis.vrtp_rtp_in: participant waiting for validated before receiving media\n"));
         rtp_packet_done(rtp);
         return XRTP_CONSUMED;
      }

      if(!session_update_seqno(sender, seqno)){

         rtp_packet_done(rtp);

         return XRTP_CONSUMED;

      }else{

         if(!sender->rtp_connect){

            /* Get connect of the participant */
            rtp_conn = rtp->connect;
            rtcp_conn = NULL;

            rtp->connect = NULL; /* Dump the connect from rtcp packet */

            if(vh->session->$callbacks.member_connects){

               vh->session->$callbacks.member_connects(vh->session->$callbacks.member_connects_user, src, &rtp_conn, &rtcp_conn);
               session_member_set_connects(sender, rtp_conn, rtcp_conn);

            }else{

               rtcp_conn = connect_rtp_to_rtcp(rtp->connect); /* For RFC1889 static port allocation: rtcp port = rtp port + 1 */
               session_member_set_connects(sender, rtp->connect, rtcp_conn);
            }
         }

         sender->valid = 1;
      }
   }

   if(sender->n_rtp_received == 0)   vh->seqno_next_unit = seqno;

   session_member_update_rtp(sender, rtp);

   /* Calculate local playtime */
   later = time_nsec_now(vh->session->clock) - session_member_mapto_local_time(sender, rtp);

   /* FIXME: Bug if packet lost */
   while(seqno == vh->seqno_next_unit){

      /* Dummy rtp packet for time resynchronization */
      if(rtp->is_sync){

         xrtp_session_t * ses = vh->session;
         xrtp_hrtime_t hrt_local = sender->last_hrt_local - sender->last_hrt_rtpts + rtp->hrt_rtpts;

         ses->$callbacks.resync_timestamp(ses->$callbacks.resync_timestamp_user, ses,
                                              rtp->hi_ntp, rtp->lo_ntp, hrt_local);

         rtp_packet_done(rtp);

      }else{

         vrtp_log(("audip/vorbis.vrtp_rtp_in: media data assembled, deliver\n"));
         media_dlen = rtp_packet_dump_payload(rtp, &media_data);


         if(rtpmedia->$callbacks.media_arrived)
            rtpmedia->$callbacks.media_arrived(rtpmedia->$callbacks.media_arrived_user, media_data, media_dlen, src, later, 0);

         sender->last_hrt_local = rtp->hrt_local;
         sender->last_hrt_rtpts = rtp->hrt_rtpts;

         rtp_packet_done(rtp);
         rtp = NULL;

         vh->seqno_next_unit++;
      }

      rtp = session_member_next_rtp_withhold(sender);
      if(!rtp){

         return XRTP_CONSUMED;
      }

      seqno = rtp_packet_seqno(rtp);
   }

   session_member_hold_rtp(sender, rtp);

   return XRTP_CONSUMED;
}

/**
 * put payload into the outward rtp packet
 */
int vrtp_rtp_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp) {

   vrtp_handler_t * vh = (vrtp_handler_t *)handler;
   vrtp_media_t *vm = (vrtp_media_t *)vh->vorbis_media;

   /* Mark always '0', Audio silent suppression not used */
   rtp_packet_set_mark(rtp, 0);

   rtp_packet_set_payload(rtp, vh->payload_buf);

   vrtp_log(("audio/vorbis.vrtp_rtp_out: new payload %d bytes\n", buffer_datalen(vh->payload_buf)));

   if(!rtp_pack(rtp)){

      vrtp_log(("audio/vorbis.vrtp_rtp_out: Fail to pack rtp data\n"));

      return XRTP_FAIL;
   }

   rtp_packet_set_payload(rtp, NULL);
   
   return XRTP_OK;
}

/**
 * When a rtcp come in, need to do following:
 * - If a new src, add to member list, waiting to validate.
 * - If is a SR, do nothing before be validated, otherwise check
 * - If is a RR, check with session self
 */
int vrtp_rtcp_in(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){

   uint32 src_sender = 0, hintp_sender = 0, lontp_sender = 0;
   uint32 rtpts_sender = 0, packet_sent = 0,  octet_sent = 0;

   uint8 frac_lost = 0;
   uint32 total_lost = 0, full_seqno, jitter = 0;
   uint32 lsr_stamp = 0, lsr_delay = 0;

   member_state_t * sender = NULL;

   xrtp_session_t * ses = rtcp->session;

   vrtp_log(("audio/vorbis.vrtp_rtcp_in: rtcp size = %u\n", rtcp->bytes_received));

   /* Here is the implementation */
   ((vrtp_handler_t *)handler)->rtcp_size = rtcp->bytes_received;

   if(!rtcp->valid_to_get){

      rtcp_unpack(rtcp);
      rtcp->valid_to_get = 1;

      /* rtcp_show(rtcp); */
   }

   session_count_rtcp(ses, rtcp);

   src_sender = rtcp_sender(rtcp);
   vrtp_log(("audio/vorbis.vrtp_rtcp_in: packet sender ssrc is %d\n", src_sender));

   /* rtp_conn and rtcp_conn will be uncertain after this call */
   sender = session_update_member_by_rtcp(rtcp->session, rtcp);

   if(!sender){

      rtcp_compound_done(rtcp);

      vrtp_log(("audio/vorbis.vrtp_rtcp_in: Ssrc[%d] refused\n", src_sender));
      return XRTP_CONSUMED;
   }

   if(!sender->valid){

      vrtp_log(("audio/vorbis.vrtp_rtcp_in: Member[%d] is not valid yet\n", src_sender));
      rtcp_compound_done(rtcp);

      return XRTP_CONSUMED;

   }else{

      vrtp_log(("audio/vorbis.vrtp_rtcp_in: Member[%d] is valid now, check report\n", src_sender));
      if(rtcp_sender_info(rtcp, &src_sender, &hintp_sender, &lontp_sender,
                        &rtpts_sender, &packet_sent, &octet_sent) >= XRTP_OK){

         /* SR */
         /* Check the report */
         sender->lsr_hrt = rtcp->hrt_arrival;  /* FIXME: clarify the interface */
         sender->lsr_lrt = rtcp->lrt_arrival;
         session_member_check_senderinfo(sender, hintp_sender, lontp_sender,
                                            rtpts_sender, packet_sent, octet_sent);
      }else{

         /* RR */
         /* Nothing */
      }
   }

   /* find out minority report */
   if(rtcp_report(rtcp, ses->self->ssrc, &frac_lost, &total_lost,
                      &full_seqno, &jitter, &lsr_stamp, &lsr_delay) >= XRTP_OK)
   {

      session_member_check_report(ses->self, frac_lost, total_lost, full_seqno,
                                jitter, lsr_stamp, lsr_delay);

   }else{ /* Strange !!! */ }

   /* Check SDES with registered key by user */

   /* Check APP if carried */

   rtcp_compound_done(rtcp);

   return XRTP_CONSUMED;
}

/*
 * Following need to be sent by rtcp:
 * - SR/RR depends on the condition
 * - SDES:CNAME
 * - BYE if bye
 */
int vrtp_rtcp_out(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){

   vrtp_handler_t * h = (vrtp_handler_t *)handler;

   /* Profile specified */
   if(!rtcp_pack(rtcp)){

      vrtp_log(("text/rtp-test.tm_rtcp_out: Fail to pack rtcp data\n"));
      return XRTP_FAIL;
   }

   h->rtcp_size = rtcp_length(rtcp);
   vrtp_log(("text/rtp-test.tm_rtcp_out: rtcp size = %u\n", h->rtcp_size));

   return XRTP_OK;
}

int vrtp_rtp_size(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){

   return ((vrtp_handler_t *)handler)->rtp_size;
}

int vrtp_rtcp_size(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){

   return ((vrtp_handler_t *)handler)->rtcp_size;
}

/**
 * Methods for module class
 */
profile_class_t * rtp_vorbis_module(profile_handler_t * handler){

   return vrtp;
}

int rtp_vorbis_set_master(profile_handler_t *handler, void *master){

   vrtp_handler_t * _h = (vrtp_handler_t *)handler;
   _h->master = master;

   return XRTP_OK;
}

void * rtp_vorbis_master(profile_handler_t *handler){

   vrtp_handler_t * _h = (vrtp_handler_t *)handler;

   return _h->master;
}

const char * rtp_vorbis_mime(xrtp_media_t * media){

   return vorbis_mime;
}

int rtp_vorbis_done(xrtp_media_t * media){

   vrtp_handler_t * h = ((vrtp_media_t *)media)->vorbis_profile;
   h->vorbis_media = NULL;

   free(media);
   return XRTP_OK;
}

int rtp_vorbis_set_parameter(xrtp_media_t * media, int key, void * val){

   /* Do nothing */
   return XRTP_OK;
}

/**
 * Get the media clockrate when the stream is opened
 */
int rtp_vorbis_set_rate(xrtp_media_t * media, int rate){

   media->clockrate = rate;

   session_set_rtp_rate(media->session, rate);

   return XRTP_OK;
}

int rtp_vorbis_rate(xrtp_media_t * media){

   return media->clockrate;
}

uint32 rtp_vorbis_sign(xrtp_media_t * media){

   srand(rand());
   
   return rand();
}

int rtp_vorbis_done_frame(void *gen)
{
	rtp_frame_t *rtpf = (rtp_frame_t*)gen;
	rtpf->frame.owner->recycle_frame(rtpf->frame.owner, (media_frame_t*)rtpf);

	return MP_OK;
}

/**
 * Make rtp packet payload
 *
 * Several packets in same timestamp are bundled together according to rtp-profile-vorbis
 *
 * RFC3550 indicate: 
 * Applications transmitting stored data rather than data sampled in real time typically 
 * use a virtual presentation timeline derived from wallclock time to determine when the 
 * next frame or other unit of each medium in the stored data should be presented.
 * 
 * So, the timestamp is the usec frome the randem value when the stream start.
 *
 * Timestamp initial value is random, generate timestamp CAN NOT relay on stamp in the 
 * media storage, MUST generate on the fly according to the media clock that increase in 
 * a linear and monotonic fashion (except wrap-around); 
 *
 * The whole rtp packet maxmum size limited to MTU size, typically 1500 bytes include RTP 
 * and Payload header; Normal vorbis packet in average 800 bytes, but header packets are 
 * 4-12k, maximum header block is 64k;
 * 
 * Vorbis payload header:
 * [C|F|R=0|# of packets]+[(len of vorbis)|NULL]+[ vorbis data ]
 * [1|1| 1 |<----5----->]+[<--------8--------->]+[<-len--byte->] bits
 * c=1: the first packet is a continued fragment;
 * f=1: have a completed packet or the last fragment;
 * r=0: reserved as '0'
 *
 * For single complete vorbis, no len byte: 
 *  {rtp header}{[(C=0)|(F=1)|(0)|(1)]+[  vorbis  ]}
 *
 * For fragment, no len byte: 
 *  {rtp header}{[(C=1)|(F=?)|(0)|(0)]+[ fragment ]}
 *
 * For multi-packet: 
 *  {rtp header}{[(C=0)|(F=1)|(0)|n=(2...32)]+[len#1][vorbis#1]+[len#2][vorbis#2]+...+[len#n][vorbis#n]} 
 *
 * NOTE: 
 * Only complete packet no more than 256 bytes can be bundled together, 
 * so a Fragment or a Packet larger than 256 bytes must in a rtp packet by itself.
 *
 * If all packet on hold could be bundle into a payload, create payload ready to send
 * if not, put on hold.
 *
 * Param:
 * last - the last packet of the same samplestamp, all packet on hold will be send.
 **/
uint8 vorbis_rtp_mark_continue(uint8 h, int cont){

	if (cont) return h | 0x80; /* '10000000' */

	return h & 0x7F; /* '01111111' */
}
uint8 vorbis_rtp_mark_complete(uint8 h, int comp){

	if (comp) return h | 0x40;  /* '01000000' */

	return h & 0xBF; /* '10111111' */
}
uint8 vorbis_rtp_set_number(uint8 h, uint8 n){

	return ((h&0xE0) | (n&0x1F));  /* '11100000' '00011111' */
}

int rtp_vorbis_loop(void *gen)
{
	rtp_frame_t *rtpf = NULL;

	int eos=0, eots=0;

	int first_loop= 1;
	int first_payload = 1;

	vrtp_handler_t *profile = (vrtp_handler_t*)gen;

	xrtp_session_t *session = profile->vorbis_media->rtp_media.session;
   
	rtp_frame_t *remain_frame = NULL;
	char *packet_data = NULL;
	int packet_bytes = 0;
	uint8 n_packet = 0;

	int new_group = 0;
	int discard = 0;

	int in_group = 0;

	int current_group_end = 0;

	clock_t *clock = session_clock(profile->session);

	int64 stream_samples = 0;
	int group_samples = 0;
	int payload_samples = 0;

	int group_packets;
	int first_group_payload = 0;

	rtime_t usec_group_start;
	rtime_t usec_group_end;

	rtime_t usec_group_payload;
	rtime_t usec_payload_deadline;

	rtime_t usec_group = 0;

	xlist_t *discard_frames = xlist_new();
	if(!discard_frames)
	{
		vrtp_log(("rtp_vorbis_loop: no memory for discard_frames\n"));
		return MP_EMEM;
	}

	/* randem value from current nano second */
	vrtp_log(("rtp_vorbis_loop: FIXME - timestamp should start at randem value\n"));
   
	profile->run = 1;

	while(1)
	{
		uint8 vorbis_header;
		int space;
		int continue_packet = 0;
		int payload_done = 0;
		vorbis_info_t *vinfo;

		rtime_t usec_now;

		xthr_lock(profile->packets_lock);

		if (profile->run == 0 || eos)
		{			
			profile->run = 0;
			xthr_unlock(profile->packets_lock);
			break;
		}

		if (xlist_size(profile->packets) == 0) 
		{
			profile->idle = 1;
			/* vrtp_log(("rtp_vorbis_loop: no packet waiting, sleep!\n")); */

			xthr_cond_wait(profile->pending, profile->packets_lock);

			profile->idle = 0;
			/* vrtp_log(("rtp_vorbis_loop: wakeup! %d packets pending\n", xlist_size(profile->packets))); */
		}

		/* sometime it's still empty, weired? */
		if (xlist_size(profile->packets) == 0)
		{
			xthr_unlock(profile->packets_lock);
			continue;
		}

		if(discard)
		{
			xlist_trim_before(profile->packets, profile->group_first_frame, discard_frames);
		}

		new_group = profile->new_group;

		/* if already has a packet retrieved */
		if(remain_frame && !discard)
		{
			rtpf = remain_frame;
		}
		else
		{
			rtpf = (rtp_frame_t*)xlist_remove_first(profile->packets);
			packet_data = rtpf->media_unit;
			packet_bytes = rtpf->unit_bytes;
		}

		xthr_unlock(profile->packets_lock);

		/* check time when new group arrived */
		if(profile->new_group && !first_payload)
		{
			usec_now = time_usec_now(clock);
			if(in_group && usec_now > usec_group_end)
			{
				vrtp_log(("rtp_vorbis_loop: timeout for samplestamp[%lld]\n", profile->recent_samplestamp));
				discard = 1;

				/* new group notice consumed */
				profile->new_group = 0;
			}
		}

		/* discard frames when time is late */
		if(discard)
		{
			xlist_reset(discard_frames, rtp_vorbis_done_frame);

			if(remain_frame || rtpf->samplestamp == profile->recent_samplestamp)
			{
				rtpf->frame.owner->recycle_frame(rtpf->frame.owner, (media_frame_t*)rtpf);
				remain_frame = NULL;
			}
			
			discard = 0;
			continue;
		}
   
		vinfo = (vorbis_info_t*)rtpf->media_info;

		/* frame is retrieved, process ... */
		eots = rtpf->frame.eots;
		eos = rtpf->frame.eos;
		
		/* start to make payload */

		/* If a new timestamp coming */
		if(rtpf->samplestamp != profile->recent_samplestamp || first_loop)
		{
			int64 delta_samplestamp = rtpf->samplestamp - profile->recent_samplestamp;
				
			usec_group = (rtime_t)(1000000 * delta_samplestamp / vinfo->vi.rate);
				
			vrtp_log(("rtp_vorbis_loop: new group[%lld], ", rtpf->samplestamp));
			vrtp_log(("usec_group=%dus\n", usec_group));

			profile->recent_samplestamp = rtpf->samplestamp;
			profile->usec_group_timestamp = profile->usec_payload_timestamp;

			in_group = 1;
			first_group_payload = 1;

			usec_group_payload = 0;

			first_loop = 0;

			vorbis_header = 0;
			group_samples = 0;
			group_packets = 0;
		}

		if(n_packet == 0)
		{
			space = buffer_space(profile->payload_buf) - 1; /*one header byte*/
			payload_samples = rtpf->samples;
		}
		else
		{
			space = buffer_space(profile->payload_buf);
			payload_samples += rtpf->samples;
		}

		/* make payload now */
		
		/* rtpf is a flesh packet */
		if(!remain_frame)
		{
			stream_samples += rtpf->samples;
			group_samples += rtpf->samples;
			group_packets++;
		}

		/* Test purpose */
		//vrtp_log(("audio/vorbis.rtp_vorbis_loop: (%d), %dB, %d/%d/%d(p/g/s) samples\n", rtpf->grpno, rtpf->unit_bytes, rtpf->samples, group_samples, stream_samples));
		/* Test end */
		
		/* send small packet bundled up to 32 */
		if(rtpf->unit_bytes < MAX_BUNDLE_PACKET_BYTES)
		{
			/*
			vrtp_log(("rtp_vorbis_loop: bundle packet[%lld]...", rtpf->samplestamp));
			vrtp_log(("%d bytes\n", packet_bytes));
			*/

			if(n_packet == 0)
			{
				buffer_add_uint8(profile->payload_buf, vorbis_header);
			}
				
			if(space > rtpf->unit_bytes+1 && n_packet < MAX_PACKET_BUNDLE)
			{
				buffer_add_uint8(profile->payload_buf, (uint8)(packet_bytes-1));
				buffer_add_data(profile->payload_buf, packet_data, packet_bytes);
				n_packet++;

				/* frame is done, ask owner to recycle it */
				rtpf->frame.owner->recycle_frame(rtpf->frame.owner, (media_frame_t*)rtpf);
				remain_frame = NULL;

				if(eots)
				{
					vrtp_log(("rtp_vorbis_loop: Last packet of the stamp[%lld]\n", rtpf->samplestamp));
			
					buffer_seek(profile->payload_buf, 0);
					vorbis_header = vorbis_rtp_set_number(vorbis_header, (uint8)(n_packet-1));
					buffer_add_uint8(profile->payload_buf, vorbis_header);

					payload_done = 1;			
				}
			}
			else
			{			
				/* wait for next turn, if no more room */
				remain_frame = rtpf;
				packet_data = rtpf->media_unit;
				packet_bytes = rtpf->unit_bytes;

				payload_done = 1;
			}
		}
		else //if(remain_frame || packet_bytes > space)
		{
			/* If a continue packet, a fragment or a packet larger then 256 bytes, send individually */
			int complete;
			int payload_bytes = space < packet_bytes ? space : packet_bytes;

			if(n_packet > 0)
			{
				/* wait for next turn, bundle waiting to send */
				remain_frame = rtpf;
				packet_data = rtpf->media_unit;
				packet_bytes = rtpf->unit_bytes;
			}
			else
			{
				if(remain_frame && packet_bytes < remain_frame->unit_bytes)
				{
					vrtp_log(("audio/vorbis.rtp_vorbis_loop: continue packet\n"));
					vorbis_header = vorbis_rtp_mark_continue(vorbis_header, 1);
					continue_packet = 1;
				}
				else
				{
					vorbis_header = vorbis_rtp_mark_continue(vorbis_header, 0);
					continue_packet = 0;
				}

				if(payload_bytes == packet_bytes)
				{
					complete = 1;
					n_packet = 1;
				}
				else
				{
					complete = 0;
					n_packet = 0;
				}
				
				vorbis_header = vorbis_rtp_mark_complete(vorbis_header, complete);
				vorbis_header = vorbis_rtp_set_number(vorbis_header, n_packet);

				/* [C|F|R=0|# of packets]+[ vorbis data ] */
				buffer_add_uint8(profile->payload_buf, vorbis_header);
				buffer_add_data(profile->payload_buf, packet_data, packet_bytes);

				if(payload_bytes == packet_bytes)
				{
					remain_frame = NULL;
					packet_data = NULL;
					packet_bytes = 0;

					/* frame is done, ask owner to recycle it */
					rtpf->frame.owner->recycle_frame(rtpf->frame.owner, (media_frame_t*)rtpf);
					rtpf = NULL;
				}
				else
				{
					remain_frame = rtpf;
					packet_data += payload_bytes;
					packet_bytes -= payload_bytes;
				}
			}

			payload_done = 1;
		}

		/* send payload in a rtp packet */
		if(payload_done)
		{
			int64 usec_payload;
			rtime_t usec_payload_schedule;

			
			if(first_group_payload)
			{
				/* group usec range from the time send the first rtp payload of the samplestamp */
				usec_now = usec_group_start = time_usec_now(clock);
				usec_group_end = usec_group_start + usec_group;

				vrtp_log(("rtp_vorbis_loop: %dus...%dus\n", usec_group_start, usec_group_end));

				usec_payload_deadline = usec_group_start;
				usec_payload_schedule = 0;
			}
			else
			{
				usec_now = time_usec_now(clock);
				usec_payload_schedule = usec_payload_deadline - usec_now;
			}


			/* Test purpose */
			//vrtp_log(("audio/vorbis.rtp_vorbis_loop: payload %dP,%dS,%dus...%dus\n", n_packet, payload_samples, usec_payload, usec_payload_deadline));
			vrtp_log(("audio/vorbis.rtp_vorbis_loop: %dus now, payload @%dus...#%dus\n", usec_now, usec_payload_deadline, usec_payload_schedule));
			/* Test end */

			/* call for session sending 
			 * usec_payload_launch: 
			 * usec left before send rtp packet to the net
			 * deadline<=0, means ASAP, the quickness decided by bandwidth
			session_rtp_to_send(session, usec_payload_deadline, eots);  
			 */

			/* timestamp update */
			profile->usec_payload_timestamp += (int)usec_payload; 

			usec_group_payload += (rtime_t)usec_payload;

			if(continue_packet)
				usec_payload = 0;
			else
				usec_payload = (int64)payload_samples * 1000000 / vinfo->vi.rate;

			usec_payload_deadline += (rtime_t)usec_payload;

			/* buffer ready to next payload */
			buffer_clear(profile->payload_buf, BUFFER_QUICKCLEAR);
			n_packet = 0;
			payload_samples = 0;

			first_group_payload = 0;
			first_payload = 0;
		}

		/* group process on time */
		if(eots)
		{
			in_group = 0;
			vrtp_log(("audio/vorbis.rtp_vorbis_loop: end of group @%dus\n", time_usec_now(clock)));
		}

		/* when the stream ends, quit the thread */
		if(eos && remain_frame==NULL)
		{
			vrtp_log(("rtp_vorbis_loop: End of Media, quit\n"));
			break;
		}
	}

	return MP_OK;
}

/* the media enter point of the rtp session 
 * NOTE: '*data' is NOT what 'mlen' indicated.
 */
int rtp_vorbis_post(xrtp_media_t * media, media_data_t *frame, int len_useless, int last_useless){

   rtp_frame_t *rtpf = (rtp_frame_t*)frame;

   vrtp_handler_t *profile = ((vrtp_media_t*)media)->vorbis_profile;

   xthr_lock(profile->packets_lock);
   
   xlist_addto_last(profile->packets, rtpf);
   if(rtpf->samplestamp != profile->recent_samplestamp)
   {
	   /* when new group coming, old group will be discard 
	    * if could not catch up the time */
	   profile->group_first_frame = rtpf;
	   profile->new_group = 1;
   }

   xthr_unlock(profile->packets_lock);

   if(profile->idle) xthr_cond_signal(profile->pending);

   return MP_OK;
}

xrtp_media_t * rtp_vorbis(profile_handler_t *handler){

   xrtp_media_t * media = NULL;
   
   vrtp_handler_t * h = (vrtp_handler_t *)handler;

   vrtp_log(("audio/vorbis.rtp_vorbis: create media handler\n"));

   if(h->vorbis_media) return (xrtp_media_t*)h->vorbis_media;

   h->vorbis_media = (vrtp_media_t *)malloc(sizeof(struct vrtp_media_s));
   if(h->vorbis_media){

      memset(h->vorbis_media, 0, sizeof(struct vrtp_media_s));

	  h->packets = xlist_new();
	  if(!h->packets){

		vrtp_debug(("audio/vorbis.rtp_vorbis: no input buffer created\n"));

		free(h->vorbis_media);
		h->vorbis_media = NULL;
		return NULL;
	  }
	   
	  h->thread = xthr_new(rtp_vorbis_loop, h, XTHREAD_NONEFLAGS); 
	  if(!h->thread){
		
		  xlist_done(h->packets, NULL);
		  free(h->vorbis_media);
		  h->vorbis_media = NULL;
		  return NULL;
	  }

      h->vorbis_media->vorbis_profile = h;
      
      media = ((xrtp_media_t*)(h->vorbis_media));

      media->clockrate = 0;
      media->sampling_instance = 0;

      media->session = h->session;

      media->mime = rtp_vorbis_mime;
      media->done = rtp_vorbis_done;
      
      media->set_parameter = rtp_vorbis_set_parameter;
      media->set_rate = rtp_vorbis_set_rate;
      media->rate = rtp_vorbis_rate;
      
      media->sign = rtp_vorbis_sign;
      media->post = rtp_vorbis_post;
      vrtp_log(("audio/vorbis.rtp_vorbis: media handler created\n"));
   }

   return media;
}

const char * vrtp_id(profile_class_t * clazz){

   return vorbis_mime;
}

int vrtp_type(profile_class_t * clazz){

   return XRTP_HANDLER_TYPE_MEDIA;
}

char * vrtp_description(profile_class_t * clazz){
  
   return vrtp_desc;
}

int vrtp_capacity(profile_class_t * clazz){

   return XRTP_CAP_NONE;
}

int vrtp_handler_number(profile_class_t *clazz){

   return num_handler;
}

int vrtp_done(profile_class_t * clazz){

   free(clazz);
   return XRTP_OK;
}

profile_handler_t * vrtp_new_handler(profile_class_t * clazz, xrtp_session_t * ses){

   vrtp_handler_t * vh;
   profile_handler_t * h;

   vh = (vrtp_handler_t *)malloc(sizeof(vrtp_handler_t));
   memset(vh, 0, sizeof(vrtp_handler_t));

   vh->session = ses;

   /* thread oriented */
   vh->packets = xlist_new();
   vh->packets_lock = xthr_new_lock();
   vh->pending = xthr_new_cond(XTHREAD_NONEFLAGS);

   vh->payload_buf = buffer_new(MAX_PAYLOAD_BYTES, NET_ORDER);
   vrtp_log(("audio/vorbis.vrtp_new_handler: FIXME - error probe\n"));

   h = (profile_handler_t *)vh;

   vrtp_log(("audio/vorbis.vrtp_new_handler: New handler created\n"));

   h->module = rtp_vorbis_module;

   h->master = rtp_vorbis_master;
   h->set_master = rtp_vorbis_set_master;

   h->media = rtp_vorbis;

   h->rtp_in = vrtp_rtp_in;
   h->rtp_out = vrtp_rtp_out;
   h->rtp_size = vrtp_rtp_size;

   h->rtcp_in = vrtp_rtcp_in;
   h->rtcp_out = vrtp_rtcp_out;
   h->rtcp_size = vrtp_rtcp_size;

   ++num_handler;

   return h;
}

int vrtp_done_handler(profile_handler_t * h){

   free(h);

   num_handler--;

   return XRTP_OK;
}

/**
 * Methods for module initializing
 */
module_interface_t * module_init(){


   vrtp = (profile_class_t *)malloc(sizeof(profile_class_t));

   vrtp->id = vrtp_id;
   vrtp->type = vrtp_type;
   vrtp->description = vrtp_description;
   vrtp->capacity = vrtp_capacity;
   vrtp->handler_number = vrtp_handler_number;
   vrtp->done = vrtp_done;

   vrtp->new_handler = vrtp_new_handler;
   vrtp->done_handler = vrtp_done_handler;

   num_handler = 0;

   vrtp_log(("audio/vorbis.module_init: Module['audio/vorbis'] loaded.\n"));

   return vrtp;
}

/**
 * Plugin Infomation Block
 */
extern DECLSPEC
module_loadin_t mediaformat = {

   "rtp_profile",   /* Plugin ID */

   000001,         /* Plugin Version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   module_init     /* Module initializer */
};
