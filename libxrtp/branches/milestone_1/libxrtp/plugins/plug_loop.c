/***************************************************************************
                          plug_loop.c  -  Loop for test only
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
 #include <sys/socket.h>
 #include <netinet/in.h>

 #define LOOP_RTP_PORT  5000
 #define LOOP_RTCP_PORT  5001

 #define UDP_FLAGS 0  /* Refer to man(sendto) */

 char plugin_desc[] = "The loop handler link network sender and receiver";

 int num_handler;

 handler_info_t xrtp_handler_info;

 profile_class_t * loop;

 typedef struct loop_handler_s loop_handler_t;
 typedef struct loop_port_s loop_port_t;
 
 struct loop_handler_s{

    struct profile_handler_s iface;

    void * master;

    xrtp_session_t * session;

    char rtp_in_data[XRTP_MAX_MTU_BYTES];
    int rtp_in_datalen;
    
    char rtcp_in_data[XRTP_MAX_MTU_BYTES];
    int rtcp_in_datalen;

    xrtp_hrtime_t ts_rtp_arrival;
    xrtp_lrtime_t ts_rtcp_arrival;

 } loop_handler;

 struct session_port_s{

    int udp_rtp_socket;
    int udp_rtcp_socket;
    int max_socket_num;

    fd_set fdread;

    struct sockaddr_in rtp_local;
    int rtp_local_salen;  

    struct sockaddr_in rtcp_local;
    int rtcp_local_salen;

    int inited;

 } udp_port;

 typedef struct udp_param_s loop_param_t;
 struct udp32_param_s{

    uint32 addr;    
    uint32 port;
 };

 /**
  * Main Methods MUST BE implemented
  */
 int loop_rtp_in(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){
   
    /**
     * When this function is called, means the port has already
     * received the packet from network, it is waiting to intepret to
     * the struct for accessing, the receiving pipe is start form this point.
     */
    xrtp_buffer_t * buf = buffer_new(loop_handler.rtp_in_datalen, NET_BYTEORDER);
    
    if(!buf){

        return XRTP_EMEM;
    }
    
    buffer_fill_data(buf, loop_handler.rtp_in_data, loop_handler.rtp_in_datalen);
    rtp_packet_set_buffer(rtp, buf);

    rtp->ts_arrival = loop_handler.ts_rtp_arrival;
    
    return XRTP_OK;
 }

 int loop_rtp_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp){

    /**
     * This module will loop the out-going packet to receiving pipe
     * which emulate the sender action and receiver action
     * the packet is not really sent to network.
     * the data is only be sent to local receiving port.
     * the handler is for test purposal.
     * the asumption is:
     * the packet is already serielized in the data buffer,
     */

    /**
     * other than rtp_packet_ssrc(rtp);
     * the packet always send to self port
     * all other member is ignored.
     */
     
    /**
     * Don't need to map ssrc to port, as only one exist in test mode:
     
    uint32 myssrc = session_ssrc(loop_handler.session);
    session_port_t * port = session_member_port(loop_handler.session, myssrc);
     */
    
    xrtp_buffer_t * rtpstrm = rtp_packet_buffer(rtp);

    return sendto(udp_port.udp_rtp_socket, buffer_data(rtpstrm), buffer_datalen(rtpstrm), MSG_DONTWAIT, (struct sockaddr *)&(udp_port.rtp_local), sizeof(udp_port.rtp_local));
 }

 int loop_rtcp_in(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){
   
    /**
     * When this function is called, means the port has already
     * received the packet from network, it is waiting to intepret to
     * the struct for accessing, the receiving pipe is start form this point.
     */
    xrtp_buffer_t * buf = buffer_new(loop_handler.rtp_in_datalen, NET_BYTEORDER);

    if(!buf){

        return XRTP_EMEM;
    }

    buffer_fill_data(buf, loop_handler.rtcp_in_data, loop_handler.rtcp_in_datalen);
    rtcp_set_buffer(rtcp, buf);

    rtcp->ts_arrival = loop_handler.ts_rtcp_arrival;

    return XRTP_OK;
 }

 int loop_rtcp_out(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp){

    /**
     * Refer to loop_rtp_out() comments
     */
    xrtp_buffer_t * rtcpstrm = rtcp_buffer(rtcp);

    return sendto(udp_port.udp_rtcp_socket, buffer_data(rtcpstrm), buffer_datalen(rtcpstrm), MSG_DONTWAIT, (struct sockaddr *)&(udp_port.rtcp_local), sizeof(udp_port.rtcp_local));
 }

 /**
  * Methods for module class
  */
 profile_class_t * loop_module(profile_handler_t * handler){
   
    return loop;
 }

 int loop_set_master(profile_handler_t *handler, void *master){
   
    loop_handler_t * _h = (loop_handler_t *)handler;
    _h->master = master;
    
    return XRTP_OK;
 }

 void * loop_master(profile_handler_t *handler){

    loop_handler_t * _h = (loop_handler_t *)handler;

    return _h->master;
 }

 int loop_clockrate(profile_handler_t *handler){

    return HRTIME_INFINITY;
 }

 session_port_t* loop_port(profile_handler_t *hand, xrtp_session_t * ses, void * param){

    if(udp_port.inited)
        return &udp_port;

    udp_port.udp_rtp_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    udp_port.rtp_local.sin_family = AF_INET;
    udp_port.rtp_local.sin_port = LOOP_RTP_PORT;
    udp_port.rtp_local.sin_addr.s_addr = INADDR_LOOPBACK;

    FD_SET(udp_port.udp_rtp_socket, &udp_port.fdread);

    udp_port.udp_rtcp_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    udp_port.rtcp_local.sin_family = AF_INET;
    udp_port.rtcp_local.sin_port = LOOP_RTCP_PORT;
    udp_port.rtcp_local.sin_addr.s_addr = INADDR_LOOPBACK;

    FD_SET(udp_port.udp_rtcp_socket, &udp_port.fdread);

    udp_port.max_socket_num = (udp_port.udp_rtp_socket > udp_port.udp_rtp_socket) ? (udp_port.udp_rtp_socket) : (udp_port.udp_rtcp_socket);

    udp_port.inited = 1;
    
    return &udp_port;   
 }

 char * loop_id(profile_class_t * clazz){
   
    return xrtp_handler_info.id;
 }

 char * loop_description(profile_class_t * clazz){
    return plugin_desc;
 }

 int loop_capacity(profile_class_t * clazz){
   
    return XRTP_CAP_NONE;
 }

 int loop_handler_number(profile_class_t *clazz){
   
    return num_handler;
 }

 int loop_free(profile_class_t * clazz){
   
    free(clazz);
    return XRTP_OK;
 }

 profile_handler_t * loop_new_handler(profile_class_t * clazz, xrtp_session_t * ses){
   
    profile_handler_t * h;
    
    memset(&loop_handler, 0, sizeof(struct loop_handler_s));

    h = (profile_handler_t *)&loop_handler;

    h->module = loop_module;
    
    h->master = loop_master;
    
    h->set_master = loop_set_master;
    
    h->clockrate = loop_clockrate;
    
    h->rtp_in = loop_rtp_in;
    h->rtp_out = loop_rtp_out;

    h->rtcp_in = loop_rtcp_in;    
    h->rtcp_out = loop_rtcp_out;

    FD_ZERO(&(udp_port.fdread));

    #ifdef XRTP_DEBUG
    fprintf("Report:loop_handler:{ New handler created }\n");
    #endif

    num_handler = 1;

    udp_port.inited = 0;
    
    return h;
 }

 int loop_free_handler(profile_handler_t * h){
   
    free(h);
    
    --num_handler;
    
    return XRTP_OK;
 }

 /**
  * Methods for module initializing
  */
 void * module_init(){
   
    loop = (profile_class_t *)malloc(sizeof(profile_class_t));

    loop->id = loop_id;
    loop->description = loop_description;
    loop->capacity = loop_capacity;
    loop->handler_number = loop_handler_number;
    loop->free = loop_free;
    
    loop->new_handler = loop_new_handler;
    loop->free_handler = loop_free_handler;

    num_handler = 0;

    #ifdef XRTP_DEBUG
    fprintf(stderr, "Module (%s) loaded.\n", loop_id());
    #endif

    return loop;
 }

 /**
  * Plugin Infomation Block
  */
 handler_info_t xrtp_handler_info = {

    "udef:loop",   /* Plugin ID */
    XRTP_HANDLER_TYPE_TEST,   /* Plugin Type */
    000001,         /* Plugin Version */
    000001,         /* Minimum version of lib API supportted by the module */
    000001,         /* Maximum version of lib API supportted by the module */
    module_init     /* Module initializer */
 };

 /**
  * Following is TCP/UDP port implementation
  */

  char * port_type(session_port_t * port){

      return loop_id(loop);
  }

  int port_set_param(session_port_t *port, int key, void * value){

      return XRTP_EUNSUP;
  }

  int port_done(session_port_t * port){

      udp_port.inited = 0;

      return XRTP_OK;
  }

  int port_listen(session_port_t * port, xrtp_hrtime_t timeout){

      int nact = 0;

      struct timeval tv;

     /**
       * port waitting in session period time.
       */
      tv.tv_sec = 0;
      tv.tv_usec = HRTIME_USEC(timeout);
      
      nact = select(udp_port.max_socket_num+1, &(udp_port.fdread), NULL, NULL, &tv);
      
      if(nact){

          if(FD_ISSET(udp_port.udp_rtp_socket, &(udp_port.fdread))){

              FD_CLR(udp_port.udp_rtp_socket, &(udp_port.fdread));
              
              /* FIXME: IF con't get data in time, will be overwrirren */
              udp_port.rtp_local_salen = sizeof(udp_port.rtp_local);
              loop_handler.rtp_in_datalen = recvfrom(udp_port.udp_rtp_socket, &(loop_handler.rtp_in_data), XRTP_MAX_MTU_BYTES, UDP_FLAGS, (struct sockaddr *)&(udp_port.rtp_local), &(udp_port.rtp_local_salen));
              loop_handler.ts_rtp_arrival = time_now(loop_handler.session->clock);
              
              /* To signal the arrival event */
              session_rtp_arrived(loop_handler.session, loop_handler.ts_rtp_arrival);
          }
          
          if(FD_ISSET(udp_port.udp_rtcp_socket, &(udp_port.fdread))){

              FD_CLR(udp_port.udp_rtcp_socket, &(udp_port.fdread));
              
              /* FIXME: IF con't get data in time, will be overwrirren */
              udp_port.rtcp_local_salen = sizeof(udp_port.rtcp_local);
              loop_handler.rtcp_in_datalen = recvfrom(udp_port.udp_rtcp_socket, &(loop_handler.rtcp_in_data), XRTP_MAX_MTU_BYTES, UDP_FLAGS, (struct sockaddr *)&(udp_port.rtcp_local), &(udp_port.rtcp_local_salen));
              loop_handler.ts_rtcp_arrival = lrtime_now(loop_handler.session->clock);
              
              /* To signal the arrival event */
              session_rtcp_arrived(loop_handler.session, loop_handler.ts_rtcp_arrival);
          }
      }

      return nact;
  }
