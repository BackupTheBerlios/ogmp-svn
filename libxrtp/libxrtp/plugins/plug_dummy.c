/***************************************************************************
                          plug_dummy.c  -  Dummy plugin do nothing
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

 char plugin_desc[] = "The dummy handler does nothing";

 int num_handler;

 handler_info_t xrtp_handler_info;

 profile_class_t * dummy;

 typedef struct dummy_handler_s dummy_handler_t;
 struct dummy_handler_s{

    struct profile_handler_s iface;

    void * master;

    int (*callout_idel)(void * data);
    void * callout_idel_data;
 };

 /**
  * Main Methods MUST BE implemented
  */
 int dummy_rtp_in(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){
   
    /* Here is the implementation */
    printf("\ndummy:{ Handler incoming rtp packet }\n\n");
    return XRTP_OK;
   
 }

 int dummy_rtp_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){

    /* Here is the implementation */
    printf("\ndummy:{ Handle outgoing rtp packet }\n\n");
    return XRTP_OK;

 }

 int dummy_rtcp_in(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){
   
    /* Here is the implementation */
    printf("\ndummy:{ Handle incoming rtcp control packet }\n\n");
    return XRTP_OK;   
 }

 int dummy_rtcp_out(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){

    /* Here is the implementation */
    printf("\ndummy:{ Handle outgoing rtcp control packet }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for making RTP Data Packet (RTP)
  */
 int dummy_pack_rtp(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ Pack rtp packet to byte stream for send }\n\n");
    return XRTP_OK;
 }

 int dummy_make_rtp_head(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To make rtp packet head }\n\n");
    return XRTP_OK;
 }

 int dummy_make_rtp_headext(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t * session, int sixteen){
    /* Here is the implementation */
    printf("\ndummy:{ To make rtp packet extend head }\n\n");
    return XRTP_OK;
 }

 int dummy_make_rtp_payload(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To make rtp packet payload }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for parsing RTP Data Packet (RTP)
  */
 int dummy_unpack_rtp(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ Unpack byte stream to rtp packet }\n\n");
    return XRTP_OK;
 }

 int dummy_parse_rtp_head(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To parse rtp packet head }\n\n");
    return XRTP_OK;
 }

 int dummy_parse_rtp_headext(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To parse rtp packet extend head }\n\n");
    return XRTP_OK;
 }

 int dummy_parse_rtp_payload(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To parse rtp packet payload }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for making RTP Control Packet (RTCP)
  */
 int dummy_pack_rtcp(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ Pack rtcp packet compound to byte stream }\n\n");
    return XRTP_OK;
 }

 int dummy_make_rtcp_head(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To make rtcp packet sender report }\n\n");
    return XRTP_OK;
 }

 int dummy_make_rtcp_sender_info(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To make rtcp packet sender info block }\n\n");
    return XRTP_OK;
 }

 int dummy_make_rtcp_report_block(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To make rtcp packet report block }\n\n");
    return XRTP_OK;
 }

 int dummy_make_rtcp_ext(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To make rtcp packet extension }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for parsing RTP Control Packet (RTCP)
  */
 int dummy_unpack_rtcp(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ Unpack byte stream to rtcp packet compound }\n\n");
    return XRTP_OK;
 }

 int dummy_parse_rtcp_head(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To parse rtcp packet sender report }\n\n");
    return XRTP_OK;
 }

 int dummy_parse_rtcp_sender_info(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To parse rtcp packet sender info block }\n\n");
    return XRTP_OK;
 }

 int dummy_parse_rtcp_report_block(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To parse rtcp packet report block }\n\n");
    return XRTP_OK;
 }

 int dummy_parse_rtcp_ext(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ndummy:{ To parse rtcp packet extension }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for module class
  */
 profile_class_t * dummy_module(profile_handler_t * handler){
    return dummy;
 }

 int dummy_set_master(profile_handler_t *handler, void *master){
   
    dummy_handler_t * _h = (dummy_handler_t *)handler;
    _h->master = master;
    
    return XRTP_OK;
 }

 void * dummy_master(profile_handler_t *handler){

    dummy_handler_t * _h = (dummy_handler_t *)handler;

    return _h->master;
 }

 xrtp_media_t * dummy_get_media(profile_handler_t *handler, xrtp_session_t * ses){

    xrtp_media_t * media = NULL;

    /* prosedo code: media->session = ses */

    return media;
 }

 int dummy_get_callback(profile_handler_t *handler, int type, void** callout, void** data){
    printf("\ndummy_handler:{ Not support callback(%d) of Dummy }\n\n", type);
    return XRTP_EUNSUP;
 }

 int dummy_set_callout(profile_handler_t *handler, int type, void* callout, void* data){
    #ifdef XRTP_DEBUG
    fprintf("Report :)\tdummy_handler:\t{ to set callout }\n");
    #endif

    dummy_handler_t * _h = (dummy_handler_t *)handler;
    
    switch(type){
       case(XRTP_HANDLER_IDEL):
           _h->callout_idel = callout;
           _h->callout_idel_data = data;
           #ifdef XRTP_DEBUG
           fprintf("Report :)\tdummy_handler:\t{ Idel callout added }\n");
           #endif
           break;

       default:
           #ifdef XRTP_DEBUG
           fprintf("ERROR :(\tdummy_handler:\t<< callout[%d] unsupported >>\n", type);
           #endif
           return XRTP_EUNSUP;
    }
    
    return XRTP_OK;
 }

 char * dummy_id(profile_class_t * clazz){
    return xrtp_handler_info.id;
 }

 char * dummy_description(profile_class_t * clazz){
    return plugin_desc;
 }

 int dummy_capacity(profile_class_t * clazz){
    return XRTP_CAP_NONE;
 }

 int dummy_get_handler_number(profile_class_t *clazz){
    return num_handler;
 }

 int dummy_clockrate(profile_class_t * clazz){
    return 0;
 }

 int dummy_free(profile_class_t * clazz){
    free(clazz);
    return XRTP_OK;
 }

 profile_handler_t * dummy_new_handler(profile_class_t * clazz, void * param){
   
    dummy_handler_t * _h;
    profile_handler_t * h;
    
    _h = (dummy_handler_t *)malloc(sizeof(dummy_handler_t));
    h = (profile_handler_t *)_h;

    #ifdef XRTP_DEBUG
    fprintf("Report:dummy_handler:{ New dummy created }\n");
    #endif

    h->module = dummy_module;
    
    h->master = dummy_master;
    h->set_master = dummy_set_master;
    
    h->media = dummy_get_media;
    
    h->get_callback = dummy_get_callback;
    h->set_callout = dummy_set_callout;

    h->rtp_in = dummy_rtp_in;
    h->rtp_out = dummy_rtp_out;

    h->rtcp_in = dummy_rtcp_in;    
    h->rtcp_out = dummy_rtcp_out;

    h->pack_rtp = dummy_pack_rtp;
    h->unpack_rtp = dummy_unpack_rtp;
    
    h->make_rtp_head = dummy_make_rtp_head;
    h->make_rtp_headext = dummy_make_rtp_headext;
    h->make_rtp_payload = dummy_make_rtp_payload;
    
    h->parse_rtp_head = dummy_parse_rtp_head;
    h->parse_rtp_headext = dummy_parse_rtp_headext;
    h->parse_rtp_payload = dummy_parse_rtp_payload;
    
    h->pack_rtcp = dummy_pack_rtcp;
    h->unpack_rtcp = dummy_unpack_rtcp;
    
    h->make_rtcp_head = dummy_make_rtcp_head;
    h->make_rtcp_sender_info = dummy_make_rtcp_sender_info;
    h->make_rtcp_report_block = dummy_make_rtcp_report_block;
    h->make_rtcp_ext = dummy_make_rtcp_ext;
    
    h->parse_rtcp_head = dummy_parse_rtcp_head;
    h->parse_rtcp_sender_info = dummy_parse_rtcp_sender_info;
    h->parse_rtcp_report_block = dummy_parse_rtcp_report_block;
    h->parse_rtcp_ext = dummy_parse_rtcp_ext;

    ++num_handler;
    
    return h;
 }

 int dummy_free_handler(profile_handler_t * h){
    free(h);
    --num_handler;
    return XRTP_OK;
 }

 /**
  * Methods for module initializing
  */
 void * module_init(){
   
    dummy = (profile_class_t *)malloc(sizeof(profile_class_t));

    dummy->id = dummy_id;
    dummy->description = dummy_description;
    dummy->capacity = dummy_capacity;
    dummy->handler_number = dummy_get_handler_number;
    dummy->clockrate = dummy_clockrate;
    dummy->free = dummy_free;
    
    dummy->new_handler = dummy_new_handler;
    dummy->free_handler = dummy_free_handler;

    num_handler = 0;

    #ifdef XRTP_DEBUG
    fprintf(stderr, "Module (%s) loaded.\n", dummy_id());
    #endif

    return dummy;
 }

 /**
  * Plugin Infomation Block
  */
 handler_info_t xrtp_handler_info = {

    "udef:dummy",   /* Plugin ID */
    XRTP_HANDLER_TYPE_TEST,   /* Plugin Type */
    000001,         /* Plugin Version */
    000001,         /* Minimum version of lib API supportted by the module */
    000001,         /* Maximum version of lib API supportted by the module */
    module_init     /* Module initializer */
 };
