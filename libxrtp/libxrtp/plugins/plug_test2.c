/***************************************************************************
                          plug_test2.c  -  description
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
 
 char test2_desc[] = "The test handler two for testing";

 int num_handler;
 
 handler_info_t xrtp_handler_info;
 
 profile_class_t * test2;

 typedef struct test2_handler_s test2_handler_t;
 struct test2_handler_s{
   
    struct profile_handler_s iface;
    
    void * master;

    int (*callout_idel)(void * data);
    void * callout_idel_data;

    int (*callout_unload_packet)(void * data);
    void * callout_unload_packet_data;
 };
 
 /**
  * Main Methods MUST BE implemented
  */
 int test2_handle_rtp_packet_in(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
   
    /* Here is the implementation */
    printf("\ntest2:{ Handler incoming rtp packet }\n\n");
    return XRTP_OK;
   
 }

 int test2_handle_rtp_packet_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\ntest2:{ Handle outgoing rtp packet }\n\n");
    return XRTP_OK;

 }

 int test2_handle_rtcp_compound_in(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
   
    /* Here is the implementation */
    printf("\ntest2:{ Handle incoming rtcp control packet }\n\n");
    return XRTP_OK;   
 }

 int test2_handle_rtcp_compound_out(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){

    /* Here is the implementation */
    test2_handler_t * _h = (test2_handler_t *)handler;
    
    printf("\ntest2:{ Handle outgoing rtcp control packet }\n\n");
    if(_h->callout_idel && _h->callout_unload_packet){
       printf("\ntest2:{ Handler will halt the pipe }\n\n");
       _h->callout_unload_packet(_h->callout_unload_packet_data);
       _h->callout_idel(_h->callout_idel_data);       
    }
    return XRTP_OK;
 }

 /**
  * Methods for making RTP Data Packet (RTP)
  */
 int test2_make_rtp_head(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To make rtp packet head }\n\n");
    return XRTP_OK;
 }

 int test2_make_rtp_headext(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t * session, int sixteen){
    /* Here is the implementation */
    printf("\ntest2:{ To make rtp packet extend head }\n\n");
    return XRTP_OK;
 }

 int test2_make_rtp_payload(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To make rtp packet payload }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for parsing RTP Data Packet (RTP)
  */
 int test2_parse_rtp_head(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To parse rtp packet head }\n\n");
    return XRTP_OK;
 }

 int test2_parse_rtp_headext(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To parse rtp packet extend head }\n\n");
    return XRTP_OK;
 }

 int test2_parse_rtp_payload(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To parse rtp packet payload }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for making RTP Control Packet (RTCP)
  */
 int test2_make_rtcp_head(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To make rtcp packet sender report }\n\n");
    return XRTP_OK;
 }

 int test2_make_rtcp_sender_info(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To make rtcp packet sender info block }\n\n");
    return XRTP_OK;
 }

 int test2_make_rtcp_report_block(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To make rtcp packet report block }\n\n");
    return XRTP_OK;
 }

 int test2_make_rtcp_ext(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To make rtcp packet extension }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for parsing RTP Control Packet (RTCP)
  */
 int test2_parse_rtcp_head(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To parse rtcp packet sender report }\n\n");
    return XRTP_OK;
 }

 int test2_parse_rtcp_sender_info(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To parse rtcp packet sender info block }\n\n");
    return XRTP_OK;
 }

 int test2_parse_rtcp_report_block(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To parse rtcp packet report block }\n\n");
    return XRTP_OK;
 }

 int test2_parse_rtcp_ext(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest2:{ To parse rtcp packet extension }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for module class
  */
 profile_class_t * test2_get_module(profile_handler_t * handler){
    return test2;
 }

 int test2_set_master(profile_handler_t *handler, void *master){
   
    test2_handler_t * _h = (test2_handler_t *)handler;
    _h->master = master;
    
    return XRTP_OK;
 }

 void * test2_get_master(profile_handler_t *handler){

    test2_handler_t * _h = (test2_handler_t *)handler;

    return _h->master;
 }

 int test2_get_callback(profile_handler_t *handler, int type, void** ret_callback, void** ret_data){
    printf("\ntest2_handler:{ Not support callback(%d) of Test2 }\n\n", type);
    return XRTP_EUNSUP;
 }

 int test2_set_callout(profile_handler_t *handler, int type, void *callout, void *data){
    #ifdef XRTP_DEBUG
    fprintf("Report:test2_handler:{ to set callout }\n");
    #endif

    test2_handler_t * _h = (test2_handler_t *)handler;

    switch(type){
       case(XRTP_HANDLER_IDEL):
           _h->callout_idel = callout;
           _h->callout_idel_data = data;
           #ifdef XRTP_DEBUG
           fprintf("Report:test2_handler:{Idel callback added}\n");
           #endif
           break;

       case(XRTP_HANDLER_UNLOAD_PACKET):
          _h->callout_unload_packet = callout;
          _h->callout_unload_packet_data = data;
           #ifdef XRTP_DEBUG
           fprintf("Report:test2_handler:{unload_packet callback added}\n");
           #endif
           break;
           
       default:
          return XRTP_EPERMIT;
    }
    
    return XRTP_OK;
 }

 char * test2_id(profile_class_t * clazz){
    return xrtp_handler_info.id;
 }

 char * test2_description(profile_class_t * clazz){
    return test2_desc;
 }

 int test2_capacity(profile_class_t * clazz){
    return XRTP_CAP_NONE;
 }

 int test2_get_handler_number(profile_class_t *clazz){
    return num_handler;
 }

 int test2_clockrate(profile_class_t * clazz){
    return 0;
 }

 int test2_free(profile_class_t * clazz){
    free(clazz);
    return XRTP_OK;
 }

 profile_handler_t * test2_new_handler(profile_class_t * clazz, int pt){
   
    test2_handler_t * _h;
    profile_handler_t * h;
    
    _h = (test2_handler_t *)malloc(sizeof(test2_handler_t));
    h = (profile_handler_t *)_h;

    #ifdef XRTP_DEBUG
    fprintf("Report:test2_handler:{ New test2 created }\n");
    #endif

    h->get_module = test2_get_module;
    
    h->set_master = test2_set_master;
    h->get_master = test2_get_master;
    
    h->set_callout = test2_set_callout;

    h->handle_rtp_packet_in = test2_handle_rtp_packet_in;
    h->handle_rtp_packet_out = test2_handle_rtp_packet_out;

    h->handle_rtcp_compound_in = test2_handle_rtcp_compound_in;    
    h->handle_rtcp_compound_out = test2_handle_rtcp_compound_out;

    h->make_rtp_head = test2_make_rtp_head;
    h->make_rtp_headext = test2_make_rtp_headext;
    h->make_rtp_payload = test2_make_rtp_payload;
    
    h->parse_rtp_head = test2_parse_rtp_head;
    h->parse_rtp_headext = test2_parse_rtp_headext;
    h->parse_rtp_payload = test2_parse_rtp_payload;
    
    h->make_rtcp_head = test2_make_rtcp_head;
    h->make_rtcp_sender_info = test2_make_rtcp_sender_info;
    h->make_rtcp_report_block = test2_make_rtcp_report_block;
    h->make_rtcp_ext = test2_make_rtcp_ext;
    
    h->parse_rtcp_head = test2_parse_rtcp_head;
    h->parse_rtcp_sender_info = test2_parse_rtcp_sender_info;
    h->parse_rtcp_report_block = test2_parse_rtcp_report_block;
    h->parse_rtcp_ext = test2_parse_rtcp_ext;

    _h->master = NULL;

    _h->callout_idel = NULL;
    _h->callout_idel_data = NULL;

    _h->callout_unload_packet = NULL;
    _h->callout_unload_packet_data = NULL;

    ++num_handler;
    
    return h;
 }

 int test2_free_handler(profile_handler_t * h){
    free(h);
    --num_handler;
    return XRTP_OK;
 }

 /**
  * Methods for module initializing
  */
 void * module_init(void * data){
   
    test2 = (profile_class_t *)malloc(sizeof(profile_class_t));

    test2->get_id = test2_id;
    test2->get_description = test2_description;
    test2->get_capacity = test2_capacity;
    test2->get_handler_number = test2_get_handler_number;
    test2->get_clockrate = test2_clockrate;
    test2->free = test2_free;
    
    test2->new_handler = test2_new_handler;
    test2->free_handler = test2_free_handler;

    num_handler = 0;

    #ifdef XRTP_DEBUG
    fprintf(stderr, "Module (%s) loaded.\n", test2_id());
    #endif

    return test2;
 }

 /**
  * Plugin Infomation Block
  */
 handler_info_t xrtp_handler_info = {
   
    "udef:test2",   /* Plugin ID */
    000001,         /* Plugin Version */
    000001,         /* Minimum version of lib API supportted by the module */
    000001,         /* Maximum version of lib API supportted by the module */
    module_init     /* Module initializer */
 };
