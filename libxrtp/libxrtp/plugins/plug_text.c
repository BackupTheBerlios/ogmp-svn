/***************************************************************************
                          plug_text.c  -  Text media module
                             -------------------
    begin                : Fri Mar 28 2003
    copyright            : (C) 2003 by Heming Ling
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

 #include "../session.h"
 #include "../const.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>

 #define TEXT_CLOCKRATE 1000    /* 1000Hz, RFC 2793 */
 #define TEXT_SAMPLING_INSTANCE  1  /* One sample per sampling */

#ifdef PLUG_TEXT_LOG
 #define tm_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define tm_log(fmtargs)
#endif

#define MAX_PAYLOAD_BYTES 1280

 const char tm_mime[] = "text/rtp-test";
 
 char plugin_desc[] = "The text media module for timing profile";

 int num_handler;

extern DECLSPEC
module_loadin_t xrtp_handler;
 
 profile_class_t * text;

 typedef struct tm_handler_s tm_handler_t;
 typedef struct tm_media_s tm_media_t;
 
 struct tm_handler_s{

    struct profile_handler_s iface;

    void * master;

    tm_media_t * media;
    
    xrtp_session_t * session;

    uint rtp_size;
    uint rtcp_size;

    uint32 seqno_next_unit;

	buffer_t *payload_buf;
 };

 struct media_time_s{

    uint64 time;
 };

 struct tm_media_s{

    struct xrtp_media_s iface;

    tm_handler_t * handler;

	//uint32 dolen;
    //media_data_t * data_out;
 };
 
/**
 * Main Methods MUST BE implemented
 */
int tm_rtp_in(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){
   
   tm_handler_t * h = (tm_handler_t *)handler;
   xrtp_media_t * media = (xrtp_media_t *)h->media;

   uint16 seqno;
   uint32 src;
   uint32 rtpts;

   member_state_t * sender = NULL;
   xrtp_hrtime_t later = 0;    
   char *media_data;
   int media_dlen;

   rtp_unpack(rtp);
    
   seqno = rtp_packet_seqno(rtp);
   src = rtp_packet_ssrc(rtp);
   rtpts = rtp_packet_timestamp(rtp);

   tm_log(("text/rtp-test.tm_rtp_in: packet ssrc=%u, seqno=%u, timestamp=%u, rtp->connect=%d\n", src, seqno, rtpts, (int)rtp->connect));
    
   sender = session_member_state(rtp->session, src);

   if(sender && sender->valid && !connect_match(rtp->connect, sender->rtp_connect)){

      session_solve_collision(sender, src);
        
      rtp_packet_done(rtp);

      return XRTP_CONSUMED;
   }

   if(!sender){

      session_connect_t *rtp_conn = NULL;
      session_connect_t *rtcp_conn = NULL;

      /* The rtp packet is recieved before rtcp arrived, so the participant is not identified yet */
      if(!h->session->$state.receive_from_anonymous){

         tm_log(("text/rtp-test.tm_rtp_in: participant waiting for identifed before receiving media\n"));
         rtp_packet_done(rtp);
         return XRTP_CONSUMED;
      }
        
      /* Get connect of the participant */
      rtp_conn = rtp->connect;
      rtp->connect = NULL; /* Dump the connect from rtcp packet */

      sender = session_new_member(h->session, src, NULL);
      if(!sender){

         /* Something WRONG!!! packet disposed */
         rtp_packet_done(rtp);

         return XRTP_CONSUMED;
      }
        
      if(h->session->$callbacks.member_connects){

         h->session->$callbacks.member_connects(h->session->$callbacks.member_connects_user, src, &rtp_conn, &rtcp_conn);
         session_member_set_connects(sender, rtp_conn, rtcp_conn);

      }else{

         rtcp_conn = connect_rtp_to_rtcp(rtp->connect); /* For RFC1889 static port allocation: rtcp port = rtp port + 1 */
         session_member_set_connects(sender, rtp->connect, rtcp_conn);            
      }

      if(h->session->join_to_rtp_port && connect_from_teleport(rtp->connect, h->session->join_to_rtp_port)){

         /* participant joined, clear the join desire */
         teleport_done(h->session->join_to_rtp_port);
         h->session->join_to_rtp_port = NULL;
         teleport_done(h->session->join_to_rtcp_port);
         h->session->join_to_rtcp_port = NULL;
      }
   }

   if(!sender->valid){

      /* The rtp packet is recieved before rtcp arrived, so the participant is not identified yet */
      if(!h->session->$state.receive_from_anonymous){

         tm_log(("text/rtp-test.tm_rtp_in: participant waiting for validated before receiving media\n"));
         rtp_packet_done(rtp);
         return XRTP_CONSUMED;
      }

      if(!session_update_seqno(sender, seqno)){
          
         rtp_packet_done(rtp);
            
         return XRTP_CONSUMED;
            
      }else{
              
         if(!sender->rtp_connect){

            /* Get connect of the participant */
            session_connect_t *rtp_conn = rtp->connect;
            session_connect_t *rtcp_conn = NULL;
                
            rtp->connect = NULL; /* Dump the connect from rtcp packet */

            if(h->session->$callbacks.member_connects){

               h->session->$callbacks.member_connects(h->session->$callbacks.member_connects_user, src, &rtp_conn, &rtcp_conn);
               session_member_set_connects(sender, rtp_conn, rtcp_conn);

            }else{

               rtcp_conn = connect_rtp_to_rtcp(rtp->connect); /* For RFC1889 static port allocation: rtcp port = rtp port + 1 */
               session_member_set_connects(sender, rtp->connect, rtcp_conn);
            }
         }
            
         sender->valid = 1;
      }
   }

   if(sender->n_rtp_received == 0)   h->seqno_next_unit = seqno;

   session_member_update_rtp(sender, rtp);

   /* Calculate local playtime */    
   later = time_nsec_now(h->session->clock) - session_member_mapto_local_time(sender, rtp);

   /* FIXME: Bug if packet lost */
   while(seqno == h->seqno_next_unit){

      /* Dummy rtp packet for time resynchronization */
      if(rtp->is_sync){

         xrtp_session_t * ses = h->session;
         xrtp_hrtime_t hrt_local = sender->last_hrt_local - sender->last_hrt_rtpts + rtp->hrt_rtpts;

         ses->$callbacks.resync_timestamp(ses->$callbacks.resync_timestamp_user, ses,
                                              rtp->hi_ntp, rtp->lo_ntp, hrt_local);

         rtp_packet_done(rtp);

      }else{

         tm_log(("text/rtp-test.tm_rtp_in: media data assembled, deliver\n"));
         media_dlen = rtp_packet_dump_payload(rtp, &media_data);
        
         if(media->$callbacks.media_arrived)
            media->$callbacks.media_arrived(media->$callbacks.media_arrived_user, media_data, media_dlen, src, later, 0);

         sender->last_hrt_local = rtp->hrt_local;
         sender->last_hrt_rtpts = rtp->hrt_rtpts;
        
         rtp_packet_done(rtp);
         rtp = NULL;

         h->seqno_next_unit++;
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

 int tm_rtp_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){

    tm_handler_t * h = (tm_handler_t *)handler;

    rtp_packet_set_mark(rtp, 0);
    
    rtp_packet_set_payload(rtp, ((tm_handler_t *)handler)->payload_buf);

    if(!rtp_pack(rtp)){

       tm_log("text/rtp-test.tm_rtp_out: Fail to pack rtp data\n");

       return XRTP_FAIL;
    }
    
    h->rtp_size = rtp_packet_length(rtp);
    tm_log(("text/rtp-test.tm_rtp_out: rtp size = %u\n", h->rtp_size));

    return XRTP_OK;
 }

/**
 * When a rtcp come in, need to do following:
 * - If a new src, add to member list, waiting to validate.
 * - If is a SR, do nothing before be validated, otherwise check
 * - If is a RR, check with session self
 */
int tm_rtcp_in(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){
   
   uint32 src_sender = 0, hintp_sender = 0, lontp_sender = 0;
   uint32 rtpts_sender = 0, packet_sent = 0,  octet_sent = 0;

   uint8 frac_lost = 0;
   uint32 total_lost = 0, full_seqno, jitter = 0;
   uint32 lsr_stamp = 0, lsr_delay = 0;
    
   member_state_t * sender = NULL;
   xrtp_session_t * ses = rtcp->session;

   tm_log(("text/rtp-test.tm_rtcp_in: rtcp size = %u\n", rtcp->bytes_received));
    
   /* Here is the implementation */
   ((tm_handler_t *)handler)->rtcp_size = rtcp->bytes_received;
    
   if(!rtcp->valid_to_get){
      
      rtcp_unpack(rtcp);
      rtcp->valid_to_get = 1;
        
      /* rtcp_show(rtcp); */
   }

   session_count_rtcp(ses, rtcp);
    
   src_sender = rtcp_sender(rtcp);
   tm_log(("text/rtp-test.tm_rtcp_in: packet sender ssrc is %d\n", src_sender));

   /* rtp_conn and rtcp_conn will be uncertain after this call */
   sender = session_update_member_by_rtcp(rtcp->session, rtcp);
    
   if(!sender){

      rtcp_compound_done(rtcp);

      tm_log(("text/rtp-test.tm_rtcp_in: Ssrc[%d] refused\n", src_sender));
      return XRTP_CONSUMED;
   }

   if(!sender->valid){

      tm_log(("text/rtp-test.tm_rtcp_in: Member[%d] is not valid yet\n", src_sender));
      rtcp_compound_done(rtcp);

      return XRTP_CONSUMED;
        
   }else{

      tm_log(("text/rtp-test.tm_rtcp_in: Member[%d] is valid now, check report\n", src_sender));
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
 int tm_rtcp_out(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){

    tm_handler_t * h = (tm_handler_t *)handler;
    
    /* Profile specified */
    if(!rtcp_pack(rtcp)){
       tm_log(("text/rtp-test.tm_rtcp_out: Fail to pack rtcp data\n"));
       return XRTP_FAIL;
    }
    
    h->rtcp_size = rtcp_length(rtcp);
    tm_log(("text/rtp-test.tm_rtcp_out: rtcp size = %u\n", h->rtcp_size));

    return XRTP_OK;
 }

 int tm_rtp_size(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){

    return ((tm_handler_t *)handler)->rtp_size;
 }

 int tm_rtcp_size(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){

    return ((tm_handler_t *)handler)->rtcp_size;
 }

 /**
  * Methods for module class
  */
 profile_class_t * tm_module(profile_handler_t * handler){
   
    return text;
 }

 int tm_set_master(profile_handler_t *handler, void *master){

    tm_handler_t * _h = (tm_handler_t *)handler;
    _h->master = master;

    return XRTP_OK;
 }

 void * tm_master(profile_handler_t *handler){

    tm_handler_t * _h = (tm_handler_t *)handler;

    return _h->master;
 }

 const char * tm_media_mime(xrtp_media_t * media){

    return tm_mime;
 }

 int tm_media_done(xrtp_media_t * media){

    tm_handler_t * h = ((tm_media_t *)media)->handler;
    h->media = NULL;
    
    free(media);
    return XRTP_OK;
 }

int tm_media_set_parameter(xrtp_media_t * media, int key, void * val){

   /* Do nothing */
   return XRTP_OK;
}

int tm_media_set_rate(xrtp_media_t * media, int rate){

   tm_log(("text/rtp-test.tm_media_set_rate: Always be %d Hz\n", TEXT_CLOCKRATE));

   return XRTP_OK;
}
 
int tm_media_rate(xrtp_media_t * media){

   return TEXT_CLOCKRATE;
}

 uint32 tm_media_sign(xrtp_media_t * media){

    srand(rand());
    return rand();
 }

 #define TEXT_MAX_DELAY  20 * HRTIME_MILLI  /* the max produce time */
 
 /* For media, this means the time to display
  * xrtp will schedule the time to send media by this parameter.
  */
 int tm_media_post(xrtp_media_t * media, media_data_t * data, int len, int allow_usec){

    /* One media unit per rtp packet */
    int is_last_pack = 1;
	int ret = OS_OK;

	tm_handler_t *th = ((tm_media_t *)media)->handler;
    
    /* Fine, no racing condition!!! */
	buffer_add_data(th->payload_buf, data, len);
    
    /* Determine the deadline of sending by
     *
     *  - duration if a Varible rate media  (Not implemented this version), or:
     *
     *  - media clockrate if duration is 0
     *
     * NOTE: Current only consider clockrate factor.
     */

    /* invoke the Session.rtp_to_send() */
    tm_log(("text/rtp-test.tm_media_go: %d bytes media data need to go\n", len));

    ret = session_rtp_to_send(media->session, TEXT_MAX_DELAY, is_last_pack);  /* '0' means the quickness decided by xrtp */
 
	buffer_clear(th->payload_buf, BUFFER_QUICKCLEAR);

	return ret;
 }

 xrtp_media_t * tm_media(profile_handler_t *handler){

    tm_handler_t * h = (tm_handler_t *)handler;

    tm_log(("text/rtp-test.tm_media: create media handler\n"));

    h->media = (tm_media_t *)malloc(sizeof(struct tm_media_s));
    if(h->media){

        xrtp_media_t * media = ((xrtp_media_t*)(h->media));

        memset(media, 0, sizeof(struct tm_media_s));
                
        h->media->handler = h;
        media->session = h->session;

        media->mime = tm_media_mime;
        media->done = tm_media_done;
        media->set_parameter = tm_media_set_parameter;
        
        media->set_rate = tm_media_set_rate;
        media->rate = tm_media_rate;
        
        media->sign = tm_media_sign;
        media->post = tm_media_post;
        
        media->clockrate = TEXT_CLOCKRATE;
        media->sampling_instance = TEXT_SAMPLING_INSTANCE;

        session_set_rtp_rate(h->session, TEXT_CLOCKRATE);

        tm_log(("text/rtp-test.tm_media: media handler created\n"));
    }

    return (xrtp_media_t *)h->media;
 }

 const char * tm_id(profile_class_t * clazz){
   
    return tm_mime;
 }

 int tm_type(profile_class_t * clazz){

    return XRTP_HANDLER_TYPE_MEDIA;
 }

 char * tm_description(profile_class_t * clazz){
    return plugin_desc;
 }

 int tm_capacity(profile_class_t * clazz){
    return XRTP_CAP_NONE;
 }

 int tm_handler_number(profile_class_t *clazz){
    return num_handler;
 }

 int tm_done(profile_class_t * clazz){
    free(clazz);
    return XRTP_OK;
 }

 profile_handler_t * tm_new_handler(profile_class_t * clazz, xrtp_session_t * ses){
   
    tm_handler_t * _h;
    profile_handler_t * h;
    
    _h = (tm_handler_t *)malloc(sizeof(tm_handler_t));
    memset(_h, 0, sizeof(tm_handler_t));
    
    _h->session = ses;
    _h->payload_buf = buffer_new(MAX_PAYLOAD_BYTES, NET_ORDER);
    tm_log(("text.vrtp_new_handler: FIXME - error probe\n"));

    h = (profile_handler_t *)_h;

    tm_log(("plug_text.tm_new_handler: New handler created\n"));

    h->module = tm_module;

    h->master = tm_master;
    h->set_master = tm_set_master;
    
    h->media = tm_media;
    
    h->rtp_in = tm_rtp_in;
    h->rtp_out = tm_rtp_out;
    h->rtp_size = tm_rtp_size;

    h->rtcp_in = tm_rtcp_in;    
    h->rtcp_out = tm_rtcp_out;
    h->rtcp_size = tm_rtcp_size;

    ++num_handler;
    
    return h;
 }

 int tm_done_handler(profile_handler_t * h){
   
    buffer_done(((tm_handler_t*)h)->payload_buf);

	free(h);
    
    --num_handler;
    
    return XRTP_OK;
 }

 /**
  * Methods for module initializing
  */
 void * module_init(){
   
    text = (profile_class_t *)malloc(sizeof(profile_class_t));

    text->id = tm_id;
    text->type = tm_type;
    text->description = tm_description;
    text->capacity = tm_capacity;
    text->handler_number = tm_handler_number;
    text->done = tm_done;
    
    text->new_handler = tm_new_handler;
    text->done_handler = tm_done_handler;

    num_handler = 0;

    tm_log(("plug_text.module_init: Module['text/rtp-test'] loaded.\n"));

    return text;
 }

/**
 * Plugin Infomation Block
 */
module_loadin_t xrtp_handler = {

   "rtp_profile",   /* Plugin ID */

   000001,         /* Plugin Version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */
   module_init     /* Module initializer */
};
