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
#include <ogg/ogg.h>
 
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

#define JITTER_ADJUSTMENT_LEVEL 3

#define VORBIS_MIME "audio/vorbis"

char vorbis_mime[] = "audio/vorbis";

char vrtp_desc[] = "Vorbis Profile for RTP";

int num_handler;

module_loadin_t xrtp_handler;

profile_class_t * vrtp;

typedef struct vrtp_handler_s vrtp_handler_t;
typedef struct vrtp_media_s vrtp_media_t;

struct vrtp_handler_s
{
   struct profile_handler_s profile;

   void * master;

   vrtp_media_t *vorbis_media;
   xrtp_session_t * session;

   uint rtp_size;
   uint rtcp_size;

   int new_group;
   rtp_frame_t *group_first_frame;

   /* a queue of vorbis packet to bundle into one rtp packet */
   xlist_t *packets;

   buffer_t *payload_buf; /* include paylaod head */
   uint32 usec_payload_timestamp; /* of current payload, frome randem value */
   uint32 usec_group_timestamp;

   uint32 timestamp_send;

   /* thread attribute */
   xthread_t *thread;

   int64 recent_samplestamp;
   rtp_frame_t *recent_frame;

   int run;
   int idle;

   xthr_lock_t *packets_lock;
   xthr_cond_t *pending;
};

struct vrtp_media_s
{
   struct xrtp_media_s rtp_media;

   vrtp_handler_t * vorbis_profile;
};

/**
 * Parse rtp packet payload
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
int vorbis_rtp_is_continue(uint8 h)
{
	return h & 0x80; /* '10000000' */
}
int vorbis_rtp_is_complete(uint8 h)
{
	return h & 0x40;  /* '01000000' */
}
int vorbis_rtp_counter(uint8 h)
{
	return h & 0x1F;  /* '00011111' */
}

/**
 * Handle inward rtp packet
 */
int vrtp_rtp_in(profile_handler_t *h, xrtp_rtp_packet_t *rtp)
{
   vrtp_handler_t *vh = (vrtp_handler_t *)h;
   
   uint16 seqno, src;

   uint32 rtpts_payload;
   uint32 rtpts_arrival;
   uint32 rtpts_toplay;
   uint32 rtpts_playing;
   uint32 rtpts_sync;
   
   member_state_t * sender = NULL;
   session_connect_t *rtp_conn = NULL;
   session_connect_t *rtcp_conn = NULL;

   rtime_t ms_arrival,us_arrival,ns_arrival;
   rtime_t ms_playing, us_playing, ns_playing;
   rtime_t ms_sync, us_sync, ns_sync;
   rtime_t us_since_sync;

   xrtp_media_t *rtp_media;

   uint32 rate;

   rtp_unpack(rtp);

   seqno = rtp_packet_seqno(rtp);
   src = rtp_packet_ssrc(rtp);

   rtpts_payload = rtp_packet_timestamp(rtp);
   vrtp_log(("audio/vorbis.vrtp_rtp_in: packet ssrc=%u, seqno=%u, timestamp=%u, rtp->connect=%d\n", src, seqno, rtpts_payload, (int)rtp->connect));

   sender = session_member_state(rtp->session, src);

   if(sender && sender->valid && !connect_match(rtp->connect, sender->rtp_connect))
   {
      session_solve_collision(sender, src);

      rtp_packet_done(rtp);

      return XRTP_CONSUMED;
   }

   if(!sender)
   {
      /* The rtp packet is recieved before rtcp arrived, so the participant is not identified yet */
      if(!vh->session->$state.receive_from_anonymous)
	  {
         vrtp_log(("audio/vorbis.vrtp_rtp_in: participant waiting for identifed before receiving media\n"));
         rtp_packet_done(rtp);
         return XRTP_CONSUMED;
      }

      /* Get connect of the participant */
      rtp_conn = rtp->connect;
      rtcp_conn = NULL;

      rtp->connect = NULL; /* Dump the connect from rtcp packet */

      sender = session_new_member(vh->session, src, NULL);
      if(!sender)
	  {
         /* Something WRONG!!! packet disposed */
         rtp_packet_done(rtp);

         return XRTP_CONSUMED;
      }

      if(vh->session->$callbacks.member_connects)
	  {
         vh->session->$callbacks.member_connects(vh->session->$callbacks.member_connects_user, src, &rtp_conn, &rtcp_conn);
         session_member_set_connects(sender, rtp_conn, rtcp_conn);
      }
	  else
	  {
         rtcp_conn = connect_rtp_to_rtcp(rtp->connect); /* For RFC1889 static port allocation: rtcp port = rtp port + 1 */
         session_member_set_connects(sender, rtp->connect, rtcp_conn);
      }

      if(vh->session->join_to_rtp_port && connect_from_teleport(rtp->connect, vh->session->join_to_rtp_port))
	  {
         /* participant joined, clear the join desire */
         teleport_done(vh->session->join_to_rtp_port);
         vh->session->join_to_rtp_port = NULL;
         teleport_done(vh->session->join_to_rtcp_port);
         vh->session->join_to_rtcp_port = NULL;
      }
   }

   if(!sender->valid)
   {
      /* The rtp packet is recieved before rtcp arrived, so the participant is not identified yet */
      if(!vh->session->$state.receive_from_anonymous)
	  {
         vrtp_log(("audio/vorbis.vrtp_rtp_in: participant waiting for validated before receiving media\n"));
         rtp_packet_done(rtp);
         return XRTP_CONSUMED;
      }

      if(!session_update_seqno(sender, seqno))
	  {
         rtp_packet_done(rtp);
         return XRTP_CONSUMED;
      }
	  else
	  {
         if(!sender->rtp_connect)
		 {
            /* Get connect of the participant */
            rtp_conn = rtp->connect;
            rtcp_conn = NULL;

            rtp->connect = NULL; /* Dump the connect from rtcp packet */

            if(vh->session->$callbacks.member_connects)
			{
               vh->session->$callbacks.member_connects(vh->session->$callbacks.member_connects_user, src, &rtp_conn, &rtcp_conn);
               session_member_set_connects(sender, rtp_conn, rtcp_conn);
            }
			else
			{
               rtcp_conn = connect_rtp_to_rtcp(rtp->connect); /* For RFC1889 static port allocation: rtcp port = rtp port + 1 */
               session_member_set_connects(sender, rtp->connect, rtcp_conn);
            }
         }

         sender->valid = 1;
      }
   }

   if(session_member_update_by_rtp(sender, rtp) < XRTP_OK)
   {
	   vrtp_log(("text/rtp-test.tm_rtp_in: bad packet\n"));
	   rtp_packet_done(rtp);
	   return XRTP_CONSUMED;
   }

   if(session_member_seqno_stall(sender, seqno))
   {
	   vrtp_log(("audio/vorbis.vrtp_rtp_in: seqno[%u] is stall, discard\n", seqno));
	   rtp_packet_done(rtp);
	   return XRTP_CONSUMED;

   }

   /* calculate play timestamp */
   rtp_packet_arrival_time(rtp, &ms_arrival, &us_arrival, &ns_arrival);
   session_member_media_playing(sender, &rtpts_playing, &ms_playing, &us_playing, &ns_playing);

   rtp_media = (xrtp_media_t*)vh->vorbis_media;

   rate = rtp_media->rate(rtp_media);
   rtpts_arrival = rtpts_playing - (uint32)((us_playing - us_arrival)/1000000.0 * rate);

   rtpts_toplay = session_member_mapto_local_time(sender, rtpts_payload, rtpts_arrival, JITTER_ADJUSTMENT_LEVEL);

   
   if(!sender->media_playable)
   {
	   uint32 rtpts_mi;
	   int signum;

	   int32 rtpts_pass;

	   vorbis_info_t *vinfo = (vorbis_info_t*)session_member_mediainfo(sender, &rtpts_mi, &signum);
	   
	   if(!vinfo || vinfo->head_packets != 3)
	   {
			vrtp_log(("audio/vorbis.vrtp_rtp_in: invalid media info\n"));
			rtp_packet_done(rtp);
			return XRTP_CONSUMED;
	   }

	   rtpts_pass = rtpts_toplay - rtpts_mi;
	   if(rtpts_pass >= 0)
	   {
		   int ret;
		   ogg_packet pack;
		   media_player_t *player, *explayer = NULL;
		   media_control_t *ctrl;

		   pack.granulepos = -1;

		   pack.packet = vinfo->header_identification;
		   pack.bytes = vinfo->header_identification_len;
		   pack.packetno = 0;
		   vorbis_synthesis_headerin(&vinfo->vi, &vinfo->vc, &pack);

		   pack.packet = vinfo->header_comment;
		   pack.bytes = vinfo->header_comment_len;
		   pack.packetno = 1;
		   vorbis_synthesis_headerin(&vinfo->vi, &vinfo->vc, &pack);

		   pack.packet = vinfo->header_setup;
		   pack.bytes = vinfo->header_setup_len;
		   pack.packetno = 2;
		   vorbis_synthesis_headerin(&vinfo->vi, &vinfo->vc, &pack);

		   sender->media_playable = 1;

		   ctrl = (media_control_t*)session_media_control(vh->session);
		   player = ctrl->find_player(ctrl, "playback", VORBIS_MIME, "");

		   ret = player->open_stream(player, (media_info_t*)vinfo);
		   if( ret < MP_OK)
		   {
				sender->media_playable = -1;
				player->done(player);
			
				vrtp_log(("audio/vorbis.vrtp_rtp_in: media is not playable\n"));
				rtp_packet_done(rtp);
			
				return XRTP_CONSUMED;
		   }         

		   explayer = (media_player_t*)session_member_set_player(sender, player);
		   if(explayer)
			   explayer->done(explayer);

		   /* start media delivery */
		   session_member_deliver(sender, seqno, 3);
	   }
   }

   /* convert rtp timestamp to usec */
   session_member_sync_point(sender, &rtpts_sync, &ms_sync, &us_sync, &ns_sync);
   us_since_sync = (uint32)((rtpts_toplay - rtpts_sync)/1000000.0 * rate);
   
   /* convert rtp packet to ogg packet */
   {   
	   char *payload;
	   int payload_bytes;
	   int cont,comp,count;
	   ogg_packet *oggpack;

	   payload_bytes = rtp_packet_payload(rtp, &payload);
	   cont = vorbis_rtp_is_continue((uint8)payload[0]);
	   comp = vorbis_rtp_is_complete((uint8)payload[0]);
	   count = vorbis_rtp_counter((uint8)payload[0]);

	   if(cont || !comp)
	   {
		   /*  handling fragments is NOT implemented currently */
		   vrtp_log(("audio/vorbis.vrtp_rtp_in: simply discard an ogg fragment\n"));
	   }
	   else if(count==1)
	   {
		   int last = 1;
		   /* single complete ogg packet */
		   oggpack = malloc(sizeof(ogg_packet));

		   payload_bytes = rtp_packet_dump_payload(rtp, &payload);

		   oggpack->packet = &payload[1];
		   oggpack->bytes = payload_bytes - 1;

		   /* convert rtp timestamp to granulepos */
		   oggpack->granulepos = session_member_samples(sender, rtpts_payload); 

		   session_member_hold_media(sender, (void*)oggpack, oggpack->bytes, seqno, rtpts_toplay, us_sync, us_since_sync, last, payload);
	   }
	   else if(count>1)
	   {
		   /* multi ogg packet bundle */
		   char *ogg = &payload[1];
		   int last = 0;
		   int i;

		   oggpack = malloc(sizeof(ogg_packet));

		   payload_bytes = rtp_packet_dump_payload(rtp, &payload);

		   for(i=0; i<count; i++)
		   {
			   oggpack->bytes = (int)ogg[0];
			   oggpack->packet = &ogg[1];

			   if(i==count-1)
			   {
				   oggpack->granulepos = session_member_samples(sender, rtpts_payload); 
				   last = 1;
			   }
			   else
			   {
				   oggpack->granulepos = -1;
			   }

			   session_member_hold_media(sender, (void*)oggpack, oggpack->bytes, seqno, rtpts_toplay, us_sync, us_since_sync, last, payload);
			   
			   ogg = ogg + oggpack->bytes + 1;
		   }
	   }
   }

   rtp_packet_done(rtp);

   return XRTP_CONSUMED;
}

/* move to session member 
int vrtp_deliver_loop(vrtp_handler_t *h)
{
	   if(rtpmedia->$callbacks.media_arrived)
		   rtpmedia->$callbacks.media_arrived(rtpmedia->$callbacks.media_arrived_user, media_data, media_dlen, src, vh->timestamp_play);
}
*/

/**
 * put payload into the outward rtp packet
 */
int vrtp_rtp_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp)
{
   vrtp_handler_t * profile = (vrtp_handler_t *)handler;

   /* Mark always '0', Audio silent suppression not used */
   rtp_packet_set_mark(rtp, 0);

   /* timestamp */
   rtp_packet_set_timestamp(rtp, profile->timestamp_send);

   rtp_packet_set_payload(rtp, profile->payload_buf);
   /*
   vrtp_log(("audio/vorbis.vrtp_rtp_out: payload[%u] (%d bytes)\n", profile->timestamp_send, buffer_datalen(profile->payload_buf)));
   */
   if(!rtp_pack(rtp))
   {
      vrtp_log(("audio/vorbis.vrtp_rtp_out: Fail to pack rtp data\n"));

      return XRTP_FAIL;
   }

   /* unset after pack into the rtp buffer */
   rtp_packet_set_payload(rtp, NULL);
   
   return XRTP_OK;
}

/**
 * When a rtcp come in, need to do following:
 * - If a new src, add to member list, waiting to validate.
 * - If is a SR, do nothing before be validated, otherwise check
 * - If is a RR, check with session self
 * 
 * Following RTP APP packet is for stream header transferring
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * NOTE: THIS IS NOT COMPLAINT to 
 * "RTP Payload Format for Vorbis Encoded Audio <draft-kerr-avt-vorbis-rtp-03.txt>"
 * So, Version is set to 0xFFFF.

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| stype(0)|   PT=APP=204  |             Length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                             VORB                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                 Timestamp (in sample rate units)              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  Vorbis Version (FFFFFFFF)                    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |1|2|3|0|0|0|0|0| sequence num  |     Header 1 Bytes (HL)       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Header 2 Bytes (HL)       |     Header 3 Bytes (HL)       |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   ..                          Header 1                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   ..                          Header 2                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   ..                          Header 3                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *
 *
 */
int vrtp_rtcp_in(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp)
{
   uint32 src_sender = 0;
   uint32 rtcpts = 0, packet_sent = 0,  octet_sent = 0;

   uint8 frac_lost = 0;
   uint32 total_lost = 0, full_seqno, jitter = 0;
   uint32 lsr_stamp = 0, lsr_delay = 0;

   member_state_t * sender = NULL;

   rtime_t ms,us,ns;

   int rate;

   xrtp_session_t * ses = rtcp->session;
   
   char appname[4] = {'V','O','R','B'};

   /* for vorbis stream setup */
   char *appdata;
   int applen;

   vrtp_handler_t *profile = (vrtp_handler_t*)handler;

   /* Here is the implementation */
   profile->rtcp_size = rtcp->bytes_received;

   if(!rtcp->valid_to_get)
   {
      rtcp_unpack(rtcp);
      rtcp->valid_to_get = 1;

      /* rtcp_show(rtcp); */
   }

   session_count_rtcp(ses, rtcp);

   src_sender = rtcp_sender(rtcp);

   rtcp_arrival_time(rtcp, &ms, &us, &ns);
   vrtp_log(("audio/vorbis.vrtp_rtcp_in: arrived %dB at %dms/%dus/%dns\n", rtcp->bytes_received, ms, us, ns));

   /* rtp_conn and rtcp_conn will be uncertain after this call */

   sender = session_update_member_by_rtcp(rtcp->session, rtcp);
   if(!sender)
   {
      rtcp_compound_done(rtcp);

      vrtp_log(("audio/vorbis.vrtp_rtcp_in: Ssrc[%d] refused\n", src_sender));
      return XRTP_CONSUMED;
   }

   rate = profile->vorbis_media->rtp_media.rate(&profile->vorbis_media->rtp_media);

   if(!sender->valid)
   {
      vrtp_log(("audio/vorbis.vrtp_rtcp_in: Member[%d] is not valid yet\n", src_sender));
      rtcp_compound_done(rtcp);

      return XRTP_CONSUMED;
   }
   else
   {
	  uint32 hi_ntp, lo_ntp;

      if(rtcp_sender_info(rtcp, &src_sender, &hi_ntp, &lo_ntp,
                          &rtcpts, &packet_sent, &octet_sent) >= XRTP_OK)
	  {
         /* Check the SR report */
		  
         /* record the sync reference point */
		 rtcp_arrival_time(rtcp, &ms, &us, &ns);

		 /* convert rtcpts to local ts */
		 session_member_synchronise(sender, rtcpts, hi_ntp, lo_ntp, rate);

         /* Check the report */
         session_member_check_senderinfo(sender, hi_ntp, lo_ntp,
										 rtcpts, packet_sent, octet_sent);

         sender->lsr_msec = ms;
         sender->lsr_usec = us;

		 vrtp_log(("audio/vorbis.vrtp_rtcp_in:========================\n"));
		 vrtp_log(("audio/vorbis.vrtp_rtcp_in: SR report ssrc[%u][%s]\n", sender->ssrc, sender->cname));
		 vrtp_log(("audio/vorbis.vrtp_rtcp_in: ntp[%u,%u];ts[%u]\n", hi_ntp,lo_ntp,rtcpts));
		 vrtp_log(("audio/vorbis.vrtp_rtcp_in: sent[%dp;%dB]\n", packet_sent, octet_sent));
      }
	  else
	  {
         /* RR */

		 vrtp_log(("audio/vorbis.vrtp_rtcp_in:========================\n"));
		 vrtp_log(("audio/vorbis.vrtp_rtcp_in: RR report ssrc[%u][%s] \n", sender->ssrc, sender->cname));
      }
   }

   /* find out minority report */
   if(rtcp_report(rtcp, ses->self->ssrc, &frac_lost, &total_lost,
                      &full_seqno, &jitter, &lsr_stamp, &lsr_delay) >= XRTP_OK)
   {
      session_member_check_report(ses->self, frac_lost, total_lost, full_seqno,
                                jitter, lsr_stamp, lsr_delay);
   }
   else
   { /* Strange !!! */ }

   /* Check SDES */

#if 0
   /* Check APP if carried, vorbis header packet is carried by APP packet */
   applen = rtcp_app(rtcp, src_sender, 0, appname, &appdata);
   if(applen)
   {
		vorbis_info_t *vinfo;
		uint32 rtpts_vi;
		int signum_vi;

		uint8 *bytes = (uint8*)appdata;

		uint32 *rtpts_h = (uint32*)&bytes[0];
		uint32 *ver = (uint32*)&bytes[4];
		int signum_now = (int)bytes[9];

        rtime_t ms_sync, us_sync, ns_sync;
		uint32 rtpts_sync, rtpts_h_local;

		vrtp_log(("audio/vorbis.vrtp_rtcp_in:------------------------\n"));
		vrtp_log(("audio/vorbis.vrtp_rtcp_in: APP[VORB] ver[%x] %d bytes\n", *ver, applen));
		
		/* mapto local rtpts, rough method */
		rtpts_h_local = *rtpts_h + session_member_sync_point(sender, &rtpts_sync, &ms_sync, &us_sync, &ns_sync);
		
		vinfo = (vorbis_info_t*)session_member_mediainfo(sender, &rtpts_vi, &signum_vi);
		if(!vinfo)
		{
			vinfo = malloc(sizeof(vorbis_info_t));
			if(!vinfo)
			{
				vrtp_log(("audio/vorbis.vrtp_rtcp_in: no memory for vorbis info\n"));
			}
			else
			{
				memset(vinfo, 0, sizeof(vorbis_info_t));
				session_member_update_mediainfo(sender, (void*)vinfo, rtpts_h_local, signum_now);
			}
		}

		if(*ver == 0xFFFFFFFF && vinfo != NULL)
		{
			vrtp_log(("audio/vorbis.vrtp_rtcp_in: vorbis info #%d\n", bytes[9]));
			/* My own vorbis app version */
			if(signum_vi != signum_now)
			{
				vorbis_info_t *exvi = (vorbis_info_t*)session_member_renew_mediainfo(sender, vinfo, rtpts_h_local, signum_now);

				if(exvi && exvi->header_identification_len)
				{
					free(exvi->header_identification);
					exvi->header_identification_len = 0;
				}
				if(exvi && exvi->header_comment_len)
				{
					free(exvi->header_comment);
					exvi->header_comment_len = 0;
				}
				if(exvi && exvi->header_setup_len)
				{		   
					free(exvi->header_setup);
					exvi->header_setup_len = 0;
				}

				exvi->head_packets = 0;
			}
			
			if(vinfo->head_packets != 3)
			{
				int h1_bytes = bytes[10];
				int h2_bytes = bytes[12];
				int h3_bytes = bytes[14];

				uint8 *pack = &appdata[16];

				h1_bytes = (h1_bytes << 8) | bytes[11];
				h2_bytes = (h2_bytes << 8) | bytes[13];
				h3_bytes = (h3_bytes << 8) | bytes[15];

				if(bytes[8] & 0x80)
				{
					/*header 1*/
					vinfo->header_identification_len = h1_bytes;
					vinfo->header_identification = malloc(h1_bytes);
					memcpy(vinfo->header_identification, pack, h1_bytes);

					vinfo->head_packets++;

					pack += h1_bytes;

					vrtp_log(("audio/vorbis.vrtp_rtcp_in: cached identification header[%dB]\n", h1_bytes));
				}

				if(bytes[8] & 0x40)
				{
					/*header 2*/
					vinfo->header_comment_len = h2_bytes;
					vinfo->header_comment = malloc(h2_bytes);
					memcpy(vinfo->header_comment, pack, h2_bytes);

					vinfo->head_packets++;

					pack += h2_bytes;

					vrtp_log(("audio/vorbis.vrtp_rtcp_in: cached comment header[%dB]\n", h2_bytes));
				}

				if(bytes[8] & 0x20)
				{
					/* header 3 */
					vinfo->header_setup_len = h3_bytes;
					vinfo->header_setup = malloc(h3_bytes);
					memcpy(vinfo->header_setup, pack, h3_bytes);

					vinfo->head_packets++;

					vrtp_log(("audio/vorbis.vrtp_rtcp_in: cached setup header[%dB]\n", h3_bytes));
				}

				if(vinfo->head_packets == 3)
				{
					vrtp_log(("audio/vorbis.vrtp_rtcp_in: ssrc[%u]#ts[%u] vorbis info cached\n", src_sender, rtpts_h_local));
					
					/* give a chance to inspect the media info of the participant */
					if(sender->on_mediainfo)
						sender->on_mediainfo(sender->on_mediainfo_user, src_sender, vinfo);	
				}
			}
		}
	}
#endif
	rtcp_compound_done(rtcp);
	vrtp_log(("audio/vorbis.vrtp_rtcp_in:************ RTCP[%u]\n", src_sender));

	return XRTP_CONSUMED;
}

/*
 * Following need to be sent by rtcp:
 * - SR/RR depends on the condition
 * - SDES:CNAME
 * - BYE if bye
 */
int vrtp_rtcp_out(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp)
{
   vrtp_handler_t *h = (vrtp_handler_t *)handler;

   /* APP packet for vorbis codebook transfer */
   xrtp_session_t *ses = rtcp->session;
   uint32 rtpts_h;
   int signum;
   void *minfo;

   char appname[4] = {'V','O','R','B'};

   session_report(h->session, rtcp, h->timestamp_send);
    
   if(session_distribute_mediainfo(ses, &rtpts_h, &signum, &minfo))
   {
	   vorbis_info_t *vinfo = (vorbis_info_t*)minfo;
   
	   int applen = vinfo->header_identification_len + vinfo->header_comment_len + vinfo->header_setup_len + 16;
	   
	   if(applen > 0)
	   {
			uint8 *app_mi = malloc(applen);
			uint8 *app;
			int nheader=0;

			app_mi[0] = rtpts_h & 0xF000;
			app_mi[1] = rtpts_h & 0x0F00;
			app_mi[2] = rtpts_h & 0x00F0;
			app_mi[3] = rtpts_h & 0x000F;
			app_mi[4] = (char)0xFF;
			app_mi[5] = (char)0xFF;
			app_mi[6] = (char)0xFF;
			app_mi[7] = (char)0xFF;
			app_mi[8] = (char)0x0;
			app_mi[9] = signum & 0xFF;
			memset(&app_mi[10], 0, 6); /* len set to zero */

			app = &app_mi[16];

			vrtp_log(("audio/vorbis.vrtp_rtcp_out: vinfo@%d\n", vinfo));
			if(vinfo->header_identification_len)
			{
				app_mi[8] |= 0x80;
				app_mi[10] = (vinfo->header_identification_len & 0xFF00) >> 8;
				app_mi[11] = vinfo->header_identification_len & 0x00FF;
				memcpy(app, vinfo->header_identification, vinfo->header_identification_len);
				app += vinfo->header_identification_len;
				nheader++;
			}
			if(vinfo->header_comment_len)
			{
				app_mi[8] |= 0x40;
				app_mi[12] = (vinfo->header_comment_len & 0xFF00) >> 8;
				app_mi[13] = vinfo->header_comment_len & 0x00FF;
				memcpy(app, vinfo->header_comment, vinfo->header_comment_len);
				app += vinfo->header_comment_len;
				nheader++;
			}
			if(vinfo->header_setup_len)
			{
				app_mi[8] |= 0x20;
				app_mi[14] = (vinfo->header_setup_len & 0xFF00) >> 8;
				app_mi[15] = vinfo->header_setup_len & 0x00FF;
				memcpy(app, vinfo->header_setup, vinfo->header_setup_len);
				nheader++;
			}

			vrtp_log(("audio/vorbis.vrtp_rtcp_out: %d packets media info in app packet\n", nheader));
			
			rtcp_new_app(rtcp, session_ssrc(ses), 0, appname, applen, app_mi);
	   }
   }

   /* Profile specified */
   if(!rtcp_pack(rtcp))
   {
      vrtp_log(("audio/vorbis.vrtp_rtcp_out: Fail to pack rtcp data\n"));
      return XRTP_FAIL;
   }

   h->rtcp_size = rtcp_length(rtcp);

   return XRTP_OK;
}

int vrtp_rtp_size(profile_handler_t *handler, xrtp_rtp_packet_t *rtp)
{
   return ((vrtp_handler_t *)handler)->rtp_size;
}

int vrtp_rtcp_size(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp)
{
   return ((vrtp_handler_t *)handler)->rtcp_size;
}

/**
 * Methods for module class
 */
profile_class_t * rtp_vorbis_module(profile_handler_t * handler)
{
   return vrtp;
}

int rtp_vorbis_set_master(profile_handler_t *handler, void *master)
{
   vrtp_handler_t * _h = (vrtp_handler_t *)handler;
   _h->master = master;

   return XRTP_OK;
}

void * rtp_vorbis_master(profile_handler_t *handler)
{
   vrtp_handler_t * _h = (vrtp_handler_t *)handler;

   return _h->master;
}

int rtp_vorbis_match_mime(xrtp_media_t * media, char *mime)
{
   return !strncmp(vorbis_mime, mime, strlen(vorbis_mime));
}

int rtp_vorbis_done(xrtp_media_t * media)
{
   vrtp_handler_t * h = ((vrtp_media_t *)media)->vorbis_profile;
   h->vorbis_media = NULL;

   free(media);
   return XRTP_OK;
}

int rtp_vorbis_set_parameter(xrtp_media_t * media, char *key, void *val)
{
	return XRTP_OK;
}

void* rtp_vorbis_parameter(xrtp_media_t * media, char *key)
{
	return NULL;
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

uint32 rtp_vorbis_sign(xrtp_media_t * media)
{
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
uint8 vorbis_rtp_mark_continue(uint8 h, int cont)
{
	if (cont) return h | 0x80; /* '10000000' */

	return h & 0x7F; /* '01111111' */
}
uint8 vorbis_rtp_mark_complete(uint8 h, int comp)
{
	if (comp) return h | 0x40;  /* '01000000' */

	return h & 0xBF; /* '10111111' */
}
uint8 vorbis_rtp_set_number(uint8 h, uint8 n)
{
	return ((h&0xE0) | (n&0x1F));  /* '11100000' '00011111' */
}

int rtp_vorbis_send_loop(void *gen)
{
	rtp_frame_t *rtpf = NULL;

	int eos=0, eots=0;

	int first_loop= 1;
	int first_group = 1;
	int first_payload = 1;

	vrtp_handler_t *profile = (vrtp_handler_t*)gen;

	rtp_frame_t *remain_frame = NULL;
	char *packet_data = NULL;
	int packet_bytes = 0;
	uint8 n_packet = 0;

	int new_group = 0;
	int discard = 0;

	int in_group = 0;

	//int current_group_end = 0;

	xclock_t *ses_clock = session_clock(profile->session);

	int group_samples = 0;
	int payload_samples = 0;

	int group_packets;
	int first_group_payload = 0;

	rtime_t usec_group_start;
	rtime_t usec_group_end;

	rtime_t usec_stream_payload;

	rtime_t usec_group = 0;

	xlist_t *discard_frames = xlist_new();
	if(!discard_frames)
	{
		vrtp_log(("rtp_vorbis_send_loop: no memory for discard_frames\n"));
		return MP_EMEM;
	}

	/* randem value from current nano second */
	vrtp_log(("rtp_vorbis_send_loop: FIXME - timestamp should start at randem value\n"));
   
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
			/* vrtp_log(("rtp_vorbis_send_loop: no packet waiting, sleep!\n")); */

			xthr_cond_wait(profile->pending, profile->packets_lock);

			profile->idle = 0;
			/* vrtp_log(("rtp_vorbis_send_loop: wakeup! %d packets pending\n", xlist_size(profile->packets))); */
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

			usec_now = time_usec_now(ses_clock);
			if(in_group && usec_now > usec_group_end)
			{
				vrtp_log(("rtp_vorbis_send_loop: timeout for samplestamp[%lld]\n", profile->recent_samplestamp));
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
				
			vrtp_log(("rtp_vorbis_send_loop: new group[%lld], ", rtpf->samplestamp));
			vrtp_log(("usec_group=%dus\n", usec_group));

			profile->recent_samplestamp = rtpf->samplestamp;
			profile->usec_group_timestamp = profile->usec_payload_timestamp;

			in_group = 1;
			first_group_payload = 1;

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
			profile->timestamp_send += rtpf->samples;
			group_samples += rtpf->samples;
			group_packets++;
		}

		/* Test purpose */
		//vrtp_log(("audio/vorbis.rtp_vorbis_send_loop: (%d), %dB, %d/%d/%d(p/g/s) samples\n", rtpf->grpno, rtpf->unit_bytes, rtpf->samples, group_samples, profile->timestamp_send));
		/* Test end */
		
		/* send small packet bundled up to 32 */
		if(rtpf->unit_bytes < MAX_BUNDLE_PACKET_BYTES)
		{
			/*
			vrtp_log(("rtp_vorbis_send_loop: bundle packet[%lld]...", rtpf->samplestamp));
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
					vrtp_log(("rtp_vorbis_send_loop: Last packet of the stamp[%lld]\n", rtpf->samplestamp));
			
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
					vrtp_log(("audio/vorbis.rtp_vorbis_send_loop: continue packet\n"));
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
			rtime_t usec_payload_deadline;
			rtime_t usec_payload;

			usec_now = time_usec_now(ses_clock);

			if(first_payload)
				usec_stream_payload = usec_now;

			if(first_group_payload)
			{
				/* group usec range from the time send the first rtp payload of the samplestamp */
				if(first_group)
					usec_group_start = usec_now;
				else
					usec_group_start = usec_group_end + 1;

					
				usec_group_end = usec_group_start + usec_group;

				vrtp_log(("rtp_vorbis_send_loop: %dus...%dus\n", usec_group_start, usec_group_end));
			}

			if(continue_packet)
			{
				usec_payload = 0;
				usec_payload_deadline = usec_stream_payload;
			}
			else
			{
				usec_payload = (rtime_t)((int64)payload_samples * 1000000 / vinfo->vi.rate);
				usec_payload_deadline = usec_stream_payload + usec_payload;
			}

			/* Test purpose */
			//vrtp_log(("audio/vorbis.rtp_vorbis_send_loop: payload[%d]@%dus (%dP,%dS,%dus)\n", profile->usec_payload_timestamp, usec_stream_payload, n_packet, payload_samples, usec_payload));
			//vrtp_log(("audio/vorbis.rtp_vorbis_send_loop: %dus now, deadline[%dus]...in #%dus\n", usec_now, usec_payload_deadline, usec_payload_deadline - usec_now));
			/* Test end */

			/* call for session sending 
			 * usec_payload_launch: 
			 * usec left before send rtp packet to the net
			 * deadline<=0, means ASAP, the quickness decided by bandwidth
			 */
			session_rtp_to_send(profile->session, usec_payload_deadline, eots);  

			/* timestamp update */
			profile->usec_payload_timestamp += (int)usec_payload; 

			usec_stream_payload += (rtime_t)usec_payload;

			/* buffer ready to next payload */
			buffer_clear(profile->payload_buf, BUFFER_QUICKCLEAR);
			n_packet = 0;
			payload_samples = 0;

			first_payload = 0;
			first_group = 0;
			first_group_payload = 0;
		}

		/* group process on time */
		if(eots)
		{
			in_group = 0;
			vrtp_log(("audio/vorbis.rtp_vorbis_send_loop: end of group @%dus\n", time_usec_now(ses_clock)));
		}

		/* when the stream ends, quit the thread */
		if(eos && remain_frame==NULL)
		{
			vrtp_log(("audio/vorbis.rtp_vorbis_send_loop: End of Media, quit\n"));
			break;
		}
	}

	return MP_OK;
}

/*
 * the media enter point of the rtp session 
 * NOTE: '*data' is NOT what 'mlen' indicated.
 */
int rtp_vorbis_post(xrtp_media_t * media, media_data_t *frame, int data_bytes, uint32 timestamp)
{
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

   if(profile->idle) 
	   xthr_cond_signal(profile->pending);

   return MP_OK;
}

int rtp_vorbis_play(void *player, void *media, int64 packetno, int ts_last, int eos)
{
	media_player_t *mp = (media_player_t*)player;
	ogg_packet *pack = (ogg_packet*)media;

	pack->packetno = packetno;

	if(eos)
		mp->receive_media(mp, pack, pack?pack->granulepos:-1, MP_EOS);
	else
		mp->receive_media(mp, pack, pack->granulepos, ts_last);

	free(pack->packet);
	free(pack);

	return XRTP_OK;
}

int rtp_vorbis_sync(void *play, uint32 stamp, uint32 hi_ntp, uint32 lo_ntp)
{
    vrtp_log(("audio/vorbis.rtp_vorbis_sync: sync isn't implemented\n"));
	return XRTP_OK;
}

xrtp_media_t * rtp_vorbis(profile_handler_t *handler)
{
   xrtp_media_t * media = NULL;
   
   vrtp_handler_t * h = (vrtp_handler_t *)handler;

   vrtp_log(("audio/vorbis.rtp_vorbis: create media handler\n"));

   if(h->vorbis_media) return (xrtp_media_t*)h->vorbis_media;

   h->vorbis_media = (vrtp_media_t *)malloc(sizeof(struct vrtp_media_s));
   if(h->vorbis_media)
   {
      memset(h->vorbis_media, 0, sizeof(struct vrtp_media_s));

	  h->packets = xlist_new();
	  if(!h->packets)
	  {
		vrtp_debug(("audio/vorbis.rtp_vorbis: no input buffer created\n"));

		free(h->vorbis_media);
		h->vorbis_media = NULL;
		return NULL;
	  }
	   
	  h->thread = xthr_new(rtp_vorbis_send_loop, h, XTHREAD_NONEFLAGS); 
	  if(!h->thread)
	  {
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

      media->match_mime = rtp_vorbis_match_mime;
      media->done = rtp_vorbis_done;
      
      media->set_parameter = rtp_vorbis_set_parameter;
      media->parameter = rtp_vorbis_parameter;

	  media->set_callback = rtp_media_set_callback;

      media->set_rate = rtp_vorbis_set_rate;
      media->rate = rtp_vorbis_rate;
      
      media->sign = rtp_vorbis_sign;
      media->post = rtp_vorbis_post;
	  media->play = rtp_vorbis_play;
	  media->sync = rtp_vorbis_sync;

      vrtp_log(("audio/vorbis.rtp_vorbis: media handler created\n"));
   }

   return media;
}

int vrtp_match_id(profile_class_t * clazz, char *id)
{
   vrtp_log(("audio/vorbis.vrtp_match_id: i am '%s' handler\n", vorbis_mime));
   return strncmp(vorbis_mime, id, strlen(vorbis_mime)) == 0;
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

   vrtp->match_id = vrtp_match_id;
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
