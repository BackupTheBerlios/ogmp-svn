/***************************************************************************
                          pipeline.h  -  Packet processer with varient handlers
                             -------------------
    begin                : Wed Apr 2 2003
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

 #define XRTP_PIPELINE_H
 
 #include "handler.h"

 #define PIPE_MAX_STEP 16

 #define PIPE_FLAG_NONE     0
 #define PIPE_FLAG_NOSCHED  1
 
 typedef struct packet_pipe_s packet_pipe_t;
 typedef struct pipe_step_s pipe_step_t;
 typedef struct pipe_load_s pipe_load_t;
 
 typedef struct pipe_load_s sched_tag_t;     /* for sched check */
 
 enum packet_type_e { XRTP_RTP, XRTP_RTCP };
 
 enum packet_direct_e { XRTP_SEND, XRTP_RECEIVE };

 struct pipe_step_s{

    int enable;
    
    int step_no;  /* the No. of step */
    
    packet_pipe_t * pipe;
    
    profile_handler_t * handler;

    pipe_load_t *load;

    int handler_return;    
 };

 struct pipe_load_s{
   
    int nextstep;
    
    void * packet;

    rtime_t max_usec;

    int packet_bytes;
 };

 #define PIPE_ENTER_STEP  -1
 #define PIPE_EXIT_STEP   -2
 #define PIPE_ERASE_STEP  -3
 
 struct packet_pipe_s{

    enum packet_type_e type;
    enum packet_direct_e direct;
    
    xrtp_session_t * session;
    
    int max_bytes_budget;
    int curr_bytes_budget;
    
    pipe_step_t * steps[PIPE_MAX_STEP];
    int num_step;

    /* rtp/rtcp packets to process */
    spin_queue_t * packets;
     
    xrtp_hrtime_t next_us;
    xrtp_hrtime_t timesize;

    int stop;

    int (*pipe_complete_cb)();
    void * pipe_complete_callee;
    
    int (*pipe_discard_cb)();
    void * pipe_discard_callee;
 };

 packet_pipe_t * pipe_new(xrtp_session_t * ses, enum packet_type_e type, enum packet_direct_e direct);

 int pipe_done(packet_pipe_t * pipe);

 pipe_step_t * pipe_add(packet_pipe_t *pipe, profile_handler_t *handler, int enable);

 pipe_step_t * pipe_add_before(packet_pipe_t *pipe, profile_handler_t *handler, int enable);

 pipe_step_t * pipe_replace(packet_pipe_t *pipe, char *oldid, profile_handler_t *handler);

 pipe_step_t * pipe_set_enter(packet_pipe_t *pipe, profile_handler_t *handler);

 pipe_step_t * pipe_set_exit(packet_pipe_t *pipe, profile_handler_t *handler);

 pipe_step_t * pipe_set_erase(packet_pipe_t *pipe, profile_handler_t *handler);

 int pipe_enable_step(pipe_step_t * step, int enable);

 int pipe_set_buffer(packet_pipe_t * pipe, int size, xrtp_hrtime_t timesize);

 pipe_step_t * pipe_find_step(packet_pipe_t * pipe, char * id);

 /**
  * Return the bytes completed.
  */
 int pipe_complete(packet_pipe_t * pipe, xrtp_hrtime_t deadline, int budget, xrtp_hrtime_t *r_nextts, int *r_nts);

 /**
  * Return the number of packet discarded.
  */
 int pipe_discard(packet_pipe_t * pipe, xrtp_hrtime_t deadline);

 /**
  * Each pump will process one packet
  */
 int pipe_pump(packet_pipe_t * pipe, rtime_t max_msec, void * packet, int *bytes_made);

 #define PIPE_CALLBACK_STEP_IDEL 1
 #define PIPE_CALLBACK_STEP_UNLOAD 2

 int pipe_get_step_callback(pipe_step_t * step, int type, void **ret_callback, void **ret_data);
