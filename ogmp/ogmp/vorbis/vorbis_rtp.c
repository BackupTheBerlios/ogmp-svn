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
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TEXT_SAMPLING_INSTANCE  1  /* One sample per sampling */

#define VRTP_LOG
#define VRTP_DEBUG
 
#ifdef VRTP_LOG
   const int vrtp_log = 1;
#else
   const int vrtp_log = 0;
#endif
#define vrtp_log(fmtargs)  do{if(vrtp_log) printf fmtargs;}while(0)

#ifdef VRTP_DEBUG
   const int vrtp_debug = 1;
#else
   const int vrtp_debug = 0;
#endif
#define vrtp_debug(fmtargs)  do{if(vrtp_debug) printf fmtargs;}while(0)

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
};

struct vrtp_media_s{

   struct xrtp_media_s rtp_media;

   vrtp_handler_t * vorbis_profile;

   uint32 dolen;
   media_data_t * data_out;

};

/**
 * Main Methods MUST BE implemented
 */
int vrtp_rtp_in(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){

   vrtp_handler_t *vh = (vrtp_handler_t *)handler;
   
   vrtp_media_t *vmedia = vh->vorbis_media;
   xrtp_media_t *rtpmedia = (xrtp_media_t*)vmedia;

   uint32 seqno, src, rtpts;
   
   member_state_t * sender = NULL;
   session_connect_t *rtp_conn = NULL;
   session_connect_t *rtcp_conn = NULL;

   char *media_data;
   int media_dlen;

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
   xrtp_hrtime_t later = hrtime_now(vh->session->clock) - session_member_mapto_local_time(sender, rtp);

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

int vrtp_rtp_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp) {

   vrtp_handler_t * vh = (vrtp_handler_t *)handler;
   vrtp_media_t *vm = (vrtp_media_t *)vh->vorbis_media;

   rtp_packet_set_mark(rtp, 0);

   if(!rtp_packet_new_payload(rtp, vm->dolen, vm->data_out)){

      return XRTP_FAIL;
   }

   vrtp_log(("audio/vorbis.vrtp_rtp_out: new payload %d bytes\n", vh->vorbis_media->dolen));

   if(!rtp_pack(rtp)){

      vrtp_log(("audio/vorbis.vrtp_rtp_out: Fail to pack rtp data\n"));

      return XRTP_FAIL;
   }

   vh->rtp_size = rtp_packet_length(rtp);
   
   vrtp_log(("audio/vorbis.vrtp_rtp_out: rtp size = %u\n", vh->rtp_size));

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

char * rtp_vorbis_mime(xrtp_media_t * media){

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

/* For media, this means the time to display
 * xrtp will schedule the time to send media by this parameter.
 */
int rtp_vorbis_post(xrtp_media_t * media, media_data_t *data, int mlen, int in_usec){

   char *media_unit = NULL;
   
   long  unit_bytes = ((rtp_frame_t*)data)->unit_bytes;
   int samplestamp = ((rtp_frame_t*)data)->samplestamp;

   vorbis_info_t *vinfo = (vorbis_info_t*)((rtp_frame_t*)data)->media_info;

   media_unit = ((rtp_frame_t*)data)->media_unit;
   
   int ret = MP_OK;

   if (in_usec < 0)
      vrtp_log(("rtp_vorbis_post: ready to post the last %ld bytes vorbis packet[@%d]\n", unit_bytes, samplestamp));
   else
      vrtp_log(("rtp_vorbis_post: ready to post a %ld bytes vorbis packet[@%d]\n", unit_bytes, samplestamp));

   //ret = session_rtp_to_send(media->session, ,);  /* '0' means the quickness decided by xrtp */

   return ret;
}

xrtp_media_t * rtp_vorbis(profile_handler_t *handler){

   xrtp_media_t * media = NULL;
   
   vrtp_handler_t * h = (vrtp_handler_t *)handler;

   vrtp_log(("audio/vorbis.rtp_vorbis: create media handler\n"));

   if(h->vorbis_media) return (xrtp_media_t*)h->vorbis_media;

   h->vorbis_media = (vrtp_media_t *)malloc(sizeof(struct vrtp_media_s));
   if(h->vorbis_media){

      memset(h->vorbis_media, 0, sizeof(struct vrtp_media_s));

      h->vorbis_media->vorbis_profile = h;
      
      media = ((xrtp_media_t*)(h->vorbis_media));

      media->clockrate = 0;
      media->sampling_instance = TEXT_SAMPLING_INSTANCE;
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

char * vrtp_id(profile_class_t * clazz){

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

   vrtp_handler_t * _h;
   profile_handler_t * h;

   _h = (vrtp_handler_t *)malloc(sizeof(vrtp_handler_t));
   memset(_h, 0, sizeof(vrtp_handler_t));

   _h->session = ses;

   h = (profile_handler_t *)_h;

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
module_loadin_t mediaformat = {

   "rtp_profile",   /* Plugin ID */

   000001,         /* Plugin Version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   module_init     /* Module initializer */
};
