/***************************************************************************
                          plug_test1.c  -  description
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
 
 char test1_desc[] = "The test handler one for testing";

 int num_handler;
 
 handler_info_t xrtp_handler_info;
 
 profile_class_t * test1;

 typedef struct test1_handler_s test1_handler_t;
 struct test1_handler_s{
   
    struct profile_handler_s iface;
    
    void * master;

    int (*callout_idel)(void * data);
    void * callout_idel_data;
 };
 
 /**
  * Main Methods MUST BE implemented
  */
 int test1_handle_rtp_packet_in(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
   
    /* Here is the implementation */
    printf("\ntest1:{ Handler incoming rtp packet }\n\n");
    return XRTP_OK;
   
 }

 int test1_handle_rtp_packet_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\ntest1:{ Handle outgoing rtp packet }\n\n");
    return XRTP_OK;

 }

 int test1_handle_rtcp_compound_in(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
   
    /* Here is the implementation */
    printf("\ntest1:{ Handle incoming rtcp control packet }\n\n");
    return XRTP_OK;   
 }

 int test1_handle_rtcp_compound_out(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\ntest1:{ Handle outgoing rtcp control packet }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for making RTP Data Packet (RTP)
  */
 int test1_make_rtp_head(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To make rtp packet head }\n\n");
    return XRTP_OK;
 }

 int test1_make_rtp_headext(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t * session, int sixteen){
    /* Here is the implementation */
    printf("\ntest1:{ To make rtp packet extend head }\n\n");
    return XRTP_OK;
 }

 int test1_make_rtp_payload(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To make rtp packet payload }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for parsing RTP Data Packet (RTP)
  */
 int test1_parse_rtp_head(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To parse rtp packet head }\n\n");
    return XRTP_OK;
 }

 int test1_parse_rtp_headext(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To parse rtp packet extend head }\n\n");
    return XRTP_OK;
 }

 int test1_parse_rtp_payload(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To parse rtp packet payload }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for making RTP Control Packet (RTCP)
  */
 int test1_make_rtcp_head(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To make rtcp packet sender report }\n\n");
    return XRTP_OK;
 }

 int test1_make_rtcp_sender_info(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To make rtcp packet sender info block }\n\n");
    return XRTP_OK;
 }

 int test1_make_rtcp_report_block(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To make rtcp packet report block }\n\n");
    return XRTP_OK;
 }

 int test1_make_rtcp_ext(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To make rtcp packet extension }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for parsing RTP Control Packet (RTCP)
  */
 int test1_parse_rtcp_head(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To parse rtcp packet sender report }\n\n");
    return XRTP_OK;
 }

 int test1_parse_rtcp_sender_info(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To parse rtcp packet sender info block }\n\n");
    return XRTP_OK;
 }

 int test1_parse_rtcp_report_block(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To parse rtcp packet report block }\n\n");
    return XRTP_OK;
 }

 int test1_parse_rtcp_ext(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){
    /* Here is the implementation */
    printf("\ntest1:{ To parse rtcp packet extension }\n\n");
    return XRTP_OK;
 }

 /**
  * Methods for module class
  */
 profile_class_t * test1_get_module(profile_handler_t * handler){
    return test1;
 }

 int test1_set_master(profile_handler_t *handler, void *master){
   
    test1_handler_t * _h = (test1_handler_t *)handler;
    _h->master = master;
    
    return XRTP_OK;
 }

 void * test1_get_master(profile_handler_t *handler){

    test1_handler_t * _h = (test1_handler_t *)handler;

    return _h->master;
 }

 int test1_get_callback(profile_handler_t *handler, int type, void** callback, void** data){
    printf("\ntest1_handler:{ Not support callback(%d) of Test1 }\n\n", type);
    return XRTP_EUNSUP;
 }

 int test1_set_callout(profile_handler_t *handler, int type, void *callout, void *data){
    #ifdef XRTP_DEBUG
    fprintf("Report:test1_handler:{to set callout for Idel}\n");
    #endif

    test1_handler_t * _h = (test1_handler_t *)handler;
    
    switch(type){
       case(XRTP_HANDLER_IDEL):
           _h->callout_idel = callout;
           _h->callout_idel_data = data;
           #ifdef XRTP_DEBUG
           fprintf("Report:test1_handler:{Idel callout added}\n");
           #endif
           break;

       default:
          return XRTP_EPERMIT;
    }
    
    return XRTP_OK;
 }

 char * test1_id(profile_class_t * clazz){
    return xrtp_handler_info.id;
 }

 char * test1_description(profile_class_t * clazz){
    return test1_desc;
 }

 int test1_capacity(profile_class_t * clazz){
    return XRTP_CAP_NONE;
 }

 int test1_get_handler_number(profile_class_t *clazz){
    return num_handler;
 }

 int test1_clockrate(profile_class_t * clazz){
    return 0;
 }

 int test1_free(profile_class_t * clazz){
    free(clazz);
    return XRTP_OK;
 }

 profile_handler_t * test1_new_handler(profile_class_t * clazz, int pt){
   
    test1_handler_t * _h;
    profile_handler_t * h;
    
    _h = (test1_handler_t *)malloc(sizeof(test1_handler_t));
    h = (profile_handler_t *)_h;

    #ifdef XRTP_DEBUG
    fprintf("Report:test1_handler:{ New test1 created }\n");
    #endif

    h->get_module = test1_get_module;
    
    h->set_master = test1_set_master;
    h->get_master = test1_get_master;
    
    h->set_callout = test1_set_callout;

    h->handle_rtp_packet_in = test1_handle_rtp_packet_in;
    h->handle_rtp_packet_out = test1_handle_rtp_packet_out;

    h->handle_rtcp_compound_in = test1_handle_rtcp_compound_in;    
    h->handle_rtcp_compound_out = test1_handle_rtcp_compound_out;

    h->make_rtp_head = test1_make_rtp_head;
    h->make_rtp_headext = test1_make_rtp_headext;
    h->make_rtp_payload = test1_make_rtp_payload;
    
    h->parse_rtp_head = test1_parse_rtp_head;
    h->parse_rtp_headext = test1_parse_rtp_headext;
    h->parse_rtp_payload = test1_parse_rtp_payload;
    
    h->make_rtcp_head = test1_make_rtcp_head;
    h->make_rtcp_sender_info = test1_make_rtcp_sender_info;
    h->make_rtcp_report_block = test1_make_rtcp_report_block;
    h->make_rtcp_ext = test1_make_rtcp_ext;
    
    h->parse_rtcp_head = test1_parse_rtcp_head;
    h->parse_rtcp_sender_info = test1_parse_rtcp_sender_info;
    h->parse_rtcp_report_block = test1_parse_rtcp_report_block;
    h->parse_rtcp_ext = test1_parse_rtcp_ext;

    ++num_handler;
    
    return h;
 }

 int test1_free_handler(profile_handler_t * h){
    free(h);
    --num_handler;
    return XRTP_OK;
 }

 /**
  * Methods for module initializing
  */
 void * module_init(void * data){
   
    test1 = (profile_class_t *)malloc(sizeof(profile_class_t));

    test1->get_id = test1_id;
    test1->get_description = test1_description;
    test1->get_capacity = test1_capacity;
    test1->get_handler_number = test1_get_handler_number;
    test1->get_clockrate = test1_clockrate;
    test1->free = test1_free;
    
    test1->new_handler = test1_new_handler;
    test1->free_handler = test1_free_handler;

    num_handler = 0;

    #ifdef XRTP_DEBUG
    fprintf(stderr, "Module (%s) loaded.\n", test1_id());
    #endif

    return test1;
 }

 /**
  * Plugin Infomation Block
  */
 handler_info_t xrtp_handler_info = {
   
    "udef:test1",   /* Plugin ID */
    000001,         /* Plugin Version */
    000001,         /* Minimum version of lib API supportted by the module */
    000001,         /* Maximum version of lib API supportted by the module */
    module_init     /* Module initializer */
 };
