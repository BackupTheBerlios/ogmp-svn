/***************************************************************************
                          plug_serializer.c  -  (de)serialize the packet
                             -------------------
    begin                : Wed Apr 23 2003
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
 
 char plugin_desc[] = "The handler converts the packet and the byte stream";

 int num_handler;

 handler_info_t xrtp_handler_info;

 profile_class_t * ser;

 typedef struct serializer_handler_s serializer_handler_t;
 struct serializer_handler_s{

    struct profile_handler_s iface;

    void * master;

    int (*callout_idel)(void * data);
    void * callout_idel_data;
 };

 /**
  * Methods for module class
  */
 profile_class_t * serializer_module(profile_handler_t * handler){
    return ser;
 }

 int serializer_set_master(profile_handler_t *handler, void *master){

    serializer_handler_t * _h = (serializer_handler_t *)handler;
    _h->master = master;

    return XRTP_OK;
 }

 void * serializer_master(profile_handler_t *handler){

    serializer_handler_t * _h = (serializer_handler_t *)handler;

    return _h->master;
 }

 int serializer_get_callback(profile_handler_t *handler, int type, void** callout, void** data){
    printf("\nserializer_handler:{ Not support callback(%d) of serializer }\n\n", type);
    return XRTP_EUNSUP;
 }

 int serializer_set_callout(profile_handler_t *handler, int type, void* callout, void* data){
    #ifdef XRTP_DEBUG
    fprintf("Report :)\tserializer_handler:\t{ to set callout }\n");
    #endif

    serializer_handler_t * _h = (serializer_handler_t *)handler;

    switch(type){
       case(XRTP_HANDLER_IDEL):
           _h->callout_idel = callout;
           _h->callout_idel_data = data;
           #ifdef XRTP_DEBUG
           fprintf("Report :)\tserializer_handler:\t{ Idel callout added }\n");
           #endif
           break;

       default:
           #ifdef XRTP_DEBUG
           fprintf("ERROR :(\tserializer_handler:\t<< callout[%d] unsupported >>\n", type);
           #endif
           return XRTP_EUNSUP;
    }

    return XRTP_OK;
 }

 char * serializer_id(profile_class_t * mod){
    return xrtp_handler_info.id;
 }

 char * serializer_description(profile_class_t * mod){
    return plugin_desc;
 }

 int serializer_capacity(profile_class_t * mod){
    return XRTP_CAP_NONE;
 }

 int serializer_get_handler_number(profile_class_t *mod){
    return num_handler;
 }

 int serializer_clockrate(profile_class_t * mod){
    return 0;
 }

 int serializer_free(profile_class_t * mod){
    free(mod);
    return XRTP_OK;
 }

 /*****************************************************************
  * Main Methods, Every handler MUST implemented one kind of them *
  *****************************************************************/
  
 int serializer_handle_rtp_packet_in(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\nserializer:{ Unpack incoming rtp packet }\n\n");

    return rtp_unpack(rtp);
 }

 int serializer_handle_rtp_packet_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\nserializer:{ Pack outgoing rtp packet }\n\n");
    
    return rtp_pack(rtp);
 }

 int serializer_handle_rtcp_compound_in(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\nserializer:{ Unpack incoming rtcp control packet }\n\n");
    
    return rtcp_unpack(rtcp);
 }

 int serializer_handle_rtcp_compound_out(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\nserializer:{ Pack outgoing rtcp control packet }\n\n");

    return rtcp_pack(rtcp);
 }

 int serializer_unpack_rtp(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\nserializer_unpack_rtp():{ Unpack incoming rtp packet }\n\n");

    return rtp_unpack(rtp);
 }

 int serializer_pack_rtp(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\nserializer_pack_rtp():{ Pack outgoing rtp packet }\n\n");

    return rtp_pack(rtp);
 }

 int serializer_unpack_rtcp(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\nserializer_unpack_rtcp():{ Unpack incoming rtcp control packet }\n\n");

    return rtcp_unpack(rtcp);
 }

 int serializer_pack_rtcp(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session){

    /* Here is the implementation */
    printf("\nserializer_pack_rtcp():{ Pack outgoing rtcp control packet }\n\n");

    return rtcp_pack(rtcp);
 }

 /*
  * End of Main Methods of Handler *
  **********************************/

 profile_handler_t * serializer_new_handler(profile_class_t * clazz, void * param){

    serializer_handler_t * _h;
    profile_handler_t * h;

    _h = (serializer_handler_t *)malloc(sizeof(serializer_handler_t));
    h = (profile_handler_t *)_h;

    #ifdef XRTP_DEBUG
    fprintf("Report:serializer_handler:{ New serializer created }\n");
    #endif

    h->module = serializer_module;

    h->master = serializer_master;
    h->set_master = serializer_set_master;

    h->get_callback = serializer_get_callback;
    h->set_callout = serializer_set_callout;

    h->handle_rtp_packet_in = serializer_handle_rtp_packet_in;
    h->handle_rtp_packet_out = serializer_handle_rtp_packet_out;

    h->handle_rtcp_compound_in = serializer_handle_rtcp_compound_in;
    h->handle_rtcp_compound_out = serializer_handle_rtcp_compound_out;

    h->pack_rtp = serializer_pack_rtp;
    h->unpack_rtp = serializer_unpack_rtp;

    h->make_rtp_head = NULL;  /* NULL means the method is nousefull */
    h->make_rtp_headext = NULL;
    h->make_rtp_payload = NULL;

    h->parse_rtp_head = NULL;
    h->parse_rtp_headext = NULL;
    h->parse_rtp_payload = NULL;

    h->pack_rtcp = serializer_pack_rtcp;
    h->unpack_rtcp = serializer_unpack_rtcp;

    h->make_rtcp_head = NULL;
    h->make_rtcp_sender_info = NULL;
    h->make_rtcp_report_block = NULL;
    h->make_rtcp_ext = NULL;

    h->parse_rtcp_head = NULL;
    h->parse_rtcp_sender_info = NULL;
    h->parse_rtcp_report_block = NULL;
    h->parse_rtcp_ext = NULL;

    ++num_handler;

    return h;
 }

 int serializer_free_handler(profile_handler_t * h){
    free(h);
    --num_handler;
    return XRTP_OK;
 }

 /**
  * Methods for module initializing
  */
 void * module_init(){

    ser = (profile_class_t *)malloc(sizeof(profile_class_t));

    ser->id = serializer_id;
    ser->description = serializer_description;
    ser->capacity = serializer_capacity;
    ser->handler_number = serializer_get_handler_number;
    ser->clockrate = serializer_clockrate;
    ser->free = serializer_free;

    ser->new_handler = serializer_new_handler;
    ser->free_handler = serializer_free_handler;

    num_handler = 0;

    #ifdef XRTP_DEBUG
    fprintf(stderr, "Module (%s) loaded.\n", serializer_id());
    #endif

    return ser;
 }
 
 /**
  * Plugin Infomation Block
  */
 handler_info_t xrtp_handler_info = {

    "udef:serializer",   /* Plugin ID */
    000001,              /* Plugin Version */
    000001,              /* Minimum version of lib API supportted by the module */
    000001,              /* Maximum version of lib API supportted by the module */
    module_init          /* Module initializer */
 };
