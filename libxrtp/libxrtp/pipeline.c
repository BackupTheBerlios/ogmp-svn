/***************************************************************************
                          pipeline.c  -  description
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

 #ifndef XRTP_SESSION_H
 #include "session.h"
 #endif
 
 #include "const.h"
 #include <stdlib.h>
 #include <string.h>
 
 #include "stdio.h"

 #define PIPELINE_LOG
 
 #ifdef PIPELINE_LOG
   const int pipe_log = 1;
 #else
   const int pipe_log = 0;
 #endif
 #define pipe_log(fmt, args...)  do{if(pipe_log) printf(fmt, ##args);}while(0)

 #define TIME_NEWER(x,y) (((x) - (y)) >> (HRTIME_BITS - 1))

 packet_pipe_t * pipe_new(xrtp_session_t * ses, enum packet_type_e type, enum packet_direct_e direct){
   
    packet_pipe_t * pipe;
    
    pipe = (packet_pipe_t *)malloc(sizeof(packet_pipe_t));    
    if(!pipe){
      
       pipe_log("< pipe_new: Can't create pipe >\n");
       return NULL;
    }

    pipe->type = type;
    pipe->direct = direct;
    pipe->num_step = 0;
    pipe->packets = NULL;
    
    pipe->timesize = 0;

    pipe->session = ses;
    
    pipe_log("pipe_new: ");
    if(type == XRTP_RTP) pipe_log("rtp ");
    if(type == XRTP_RTCP) pipe_log("rtcp ");
    if(direct == XRTP_SEND) pipe_log("sending ");
    if(direct == XRTP_RECEIVE) pipe_log("receiving ");
    pipe_log("pipe[@%u] created\n", (int)(pipe));
    
    return pipe;
 }

 int pipe_done(packet_pipe_t * pipe){
   
    if(pipe->packets){
       queue_done(pipe->packets);
    }
    
    free(pipe);

    return XRTP_OK;
 }

 pipe_step_t * pipe_add(packet_pipe_t *pipe, profile_handler_t *handler, int enable){

    pipe_step_t * step;
 
    if(pipe->num_step == PIPE_MAX_STEP){
      
       pipe_log("< pipe_add:pipe max handler reached >\n");
       return NULL;
    }

    step = (pipe_step_t *)malloc(sizeof(pipe_step_t));
    if(!step){
      
       pipe_log("< pipe_add: No enough memory to alloc pipe handler >\n");
       return NULL;
    }

    /*
     handler->set_callback(handler, XRTP_HANDLER_CALLBACK_IDEL, pipe_callback_step_idel, step);
     */
    step->handler = handler;
    handler->set_master(handler, step);
    
    step->pipe = pipe;
    step->enable = enable;

    pipe->steps[pipe->num_step] = step;
    step->step_no = pipe->num_step;
    pipe->num_step++;

    pipe_log("pipe_add: %d steps in pipe\n", pipe->num_step);
    pipe_log("pipe_add: pipe->step[%d]->pipe = %u\n", step->step_no, (int)pipe->steps[step->step_no]->pipe);

    return step;
 }

 pipe_step_t * pipe_add_before(packet_pipe_t *pipe, profile_handler_t *handler, int enable){

    pipe_step_t * step;

    if(pipe->num_step == PIPE_MAX_STEP){
       pipe_log("< pipe_add: pipe max handler reached >\n");
       return NULL;
    }

    step = (pipe_step_t *)malloc(sizeof(pipe_step_t));
    if(!step){
       pipe_log("< pipe_add_before: No enough memory to alloc pipe handler >\n");
       return NULL;
    }

    bzero(step, sizeof(struct pipe_step_s));
    
    //handler->set_callback(handler, XRTP_HANDLER_CALLBACK_IDEL, pipe_callback_step_idel, step);
    step->handler = handler;
    handler->set_master(handler, step);

    step->pipe = pipe;
    step->enable = enable;

    int i;
    for(i=pipe->num_step; i>0; i--){

        pipe->steps[i-1]->step_no++;
        pipe->steps[i] = pipe->steps[i-1];
    }
    
    step->step_no = 0;
    pipe->steps[0] = step;

    pipe->num_step++;
    pipe_log("pipe_add_before(): %d handler(s) in the pipe\n", pipe->num_step);

    return step;
 }

 pipe_step_t * pipe_replace(packet_pipe_t *pipe, char * oldid, profile_handler_t *handler){

    pipe_step_t * step, * oldstep = NULL;

    /* Can't replace when pipe is busy */
    if(pipe->packets && queue_length(pipe->packets) > 0)
       return NULL;

    step = (pipe_step_t *)malloc(sizeof(pipe_step_t));
    if(!step){
       pipe_log("< pipe_add: No enough memory to alloc pipe handler >\n");
       return NULL;
    }

    //handler->set_callback(handler, XRTP_HANDLER_CALLBACK_IDEL, pipe_callback_step_idel, step);
    step->handler = handler;
    handler->set_master(handler, step);

    step->pipe = pipe;

    profile_handler_t * hand = NULL;
    profile_class_t * modu = NULL;
    int i;
    for(i=0; i<pipe->num_step; i++){

       hand = pipe->steps[i]->handler;
       modu = hand->module(hand);
       if(!strcmp(oldid, modu->id(modu))){

          oldstep = pipe->steps[i];
          pipe->steps[i] = step;
          step->enable = oldstep->enable;
          step->step_no = oldstep->step_no;

          break;
       }
    }

    if(oldstep){

       modu->done_handler(hand);
       free(oldstep);
    }

    return step;
 }

 int pipe_enable_step(pipe_step_t * step, int enable){
    step->enable = enable;
    return XRTP_OK;
 }

 int pipe_set_buffer(packet_pipe_t * pipe, int size, xrtp_hrtime_t timesize){
   
    if(pipe->packets){
       pipe_log("< pipe_set_buffer: Change buffer size is not permit in this version >\n");
       return XRTP_EPERMIT;
    }
    
    pipe->packets = queue_new(size);
    if(!pipe->packets){
       pipe_log("< pipe_set_buffer: No enough memory to alloc packet buffer >\n");
       return XRTP_EMEM;
    }
    
    pipe->timesize = timesize;

    return XRTP_OK;
 }

 int pipe_first_step(packet_pipe_t * pipe){

    int i;
    for(i = 0; i < pipe->num_step; i++){
       if(pipe->steps[i]->enable){

          return i;
       }
    }
    
    pipe_log("pipe_first_step: no step found\n");
    
    return -1;
 }

 int pipe_next_step(packet_pipe_t * pipe, int step){
   
    int i;
    
    if(step+1 == pipe->num_step) return -1;
    
    for(i = step + 1; i < pipe->num_step; i++){
       if(pipe->steps[i]->enable){
          return i;
       }
    }
    return -1;  
 }

 int _pipe_step(pipe_step_t * step){

    xrtp_rtp_packet_t * rtp;
    xrtp_rtcp_compound_t * rtcp;
    
    pipe_log("_pipe_step: step->pipe = %u, step->load->packet = %u\n", (int)step->pipe, (int)step->load->packet);
    if(step->pipe->type == XRTP_RTP){

       rtp = (xrtp_rtp_packet_t *)(step->load->packet);

       if(step->pipe->direct == XRTP_RECEIVE){
       
          step->handler_return = step->handler->rtp_in(step->handler, rtp);
          step->load->packet_bytes = step->handler->rtp_size(step->handler, rtp);
          
       }else{
       
          step->handler_return = step->handler->rtp_out(step->handler, rtp);
          step->load->packet_bytes = step->handler->rtp_size(step->handler, rtp);
       }
       
    }else{ /* pipe-> type == XRTP_RTCP */

       rtcp = (xrtp_rtcp_compound_t *)(step->load->packet);

       if(step->pipe->direct == XRTP_RECEIVE){
       
          step->handler_return = step->handler->rtcp_in(step->handler, rtcp);
          step->load->packet_bytes = step->handler->rtcp_size(step->handler, rtcp);

       }else{
       
          step->handler_return = step->handler->rtcp_out(step->handler, rtcp);
          step->load->packet_bytes = step->handler->rtcp_size(step->handler, rtcp);
       }
    }

    return step->handler_return;
 }

 int pipe_rtp_outgoing(packet_pipe_t * pipe, xrtp_rtp_packet_t * rtp){

    pipe_log("pipe_rtp_outgoing: to be implemented\n");
    return XRTP_OK;
 }
 
 int pipe_rtcp_outgoing(packet_pipe_t * pipe, xrtp_rtcp_compound_t * rtcp){

    pipe_log("pipe_rtcp_outgoing: pump out the rtcp directly\n");
    xrtp_session_t * ses = rtcp->session;
    xrtp_buffer_t * buf= rtcp_buffer(rtcp);
    char * data = buffer_data(buf);
    uint datalen = buffer_datalen(buf);
    session_connect_t * conn = NULL;

    int i;
    for(i=0; i<ses->n_rtcp_out; i++){

       conn = ses->outgoing_rtcp[i];
       
       pipe_log("pipe_rtcp_outgoing: sent to outgoing connect %d of %d\n", i, ses->n_rtcp_out);
       connect_send(conn, data, datalen);
    }

    if(ses->join_to_rtcp_port){

       pipe_log("pipe_rtcp_outgoing: Session[%d] has a join desire[%u] pending\n", session_id(ses), (int)(ses->join_to_rtcp_port));
       conn = connect_new(ses->rtcp_port, ses->join_to_rtcp_port);
       if(conn){

          pipe_log("pipe_rtcp_outgoing: send to joining connect\n");
          connect_send(conn, data, datalen);
          connect_done(conn);
          
       }else{

          pipe_log("pipe_rtcp_outgoing: Fail to conect\n");
       }
    }
    
    pipe_log("pipe_rtcp_outgoing: sent to %d members\n", ses->n_rtcp_out);
    return XRTP_OK;
 }
 
 int pipe_complete(packet_pipe_t * pipe, xrtp_hrtime_t deadline, int budget, xrtp_hrtime_t * r_nextts, int * r_nts){

    pipe_load_t * load;
    int npacket = 0, nbytes = 0;
    pipe_step_t * step = NULL;

    xrtp_hrtime_t ts_prev;

    *r_nts = 0;

    load = (pipe_load_t*)queue_head(pipe->packets);

    ts_prev = load->ts;

    while( load && !TIME_NEWER(deadline, load->ts) && pipe->curr_bytes_budget >= load->packet_bytes){

       queue_serve(pipe->packets);

       if(pipe->type == XRTP_RTP){

          xrtp_rtp_packet_t * rtp = (xrtp_rtp_packet_t*)(load->packet);
          pipe_rtp_outgoing(pipe, rtp);
          rtp_packet_done(rtp);
       }
       
       if(pipe->type == XRTP_RTCP){

          xrtp_rtcp_compound_t * rtcp = (xrtp_rtcp_compound_t*)(load->packet);
          pipe_rtcp_outgoing(pipe, rtcp);
          rtcp_compound_done(rtcp);
       }
       
       free(load);
       step->load = NULL;
       
       npacket++;
       nbytes += load->packet_bytes;
       pipe->curr_bytes_budget -= load->packet_bytes;
       
       load = (pipe_load_t*)queue_head(pipe->packets);
       
       if(load->ts != ts_prev)
          *r_nts++;
    }

    if(load){

       *r_nextts = pipe->next_ts = load->ts;
       
    }else{

       *r_nextts = pipe->next_ts = HRTIME_INFINITY;
    }

    if(pipe->pipe_complete_cb)
        pipe->pipe_complete_cb(pipe->pipe_complete_callee, pipe->next_ts, npacket, nbytes);
    
    return XRTP_OK;
 }

 int pipe_discard(packet_pipe_t * pipe, xrtp_hrtime_t deadline){

    pipe_load_t * load;    
    int npacket = 0;    

    load = (pipe_load_t*)queue_head(pipe->packets);

    while( load && TIME_NEWER(deadline, load->ts) ){
      
       queue_serve(pipe->packets);
       
       if(pipe->type == XRTP_RTP){

          xrtp_rtp_packet_t * rtp = (xrtp_rtp_packet_t*)(load->packet);
          pipe_rtp_outgoing(pipe, rtp);
          rtp_packet_done(rtp);
       }

       if(pipe->type == XRTP_RTCP){

          xrtp_rtcp_compound_t * rtcp = (xrtp_rtcp_compound_t*)(load->packet);
          pipe_rtcp_outgoing(pipe, rtcp);
          rtcp_compound_done(rtcp);
       }

       free(load);

       npacket++;
       
       load = (pipe_load_t*)queue_head(pipe->packets);
    }
    
    if(load){

        pipe->next_ts = load->ts;
        
    }else{

        pipe->next_ts = HRTIME_INFINITY;
    }

    if(pipe->pipe_discard_cb)
        pipe->pipe_discard_cb(pipe->pipe_discard_callee, pipe->next_ts, npacket);

    return npacket;
 }

 int pipe_pump(packet_pipe_t * pipe, xrtp_hrtime_t ts, void * pac, int * packet_bytes){

    pipe_load_t * load;
    pipe_step_t * step = NULL;
    
    *packet_bytes = 0;
    
    pipe_log("pipe_pump: start proccess ...\n");
    load = (pipe_load_t *)malloc(sizeof(struct pipe_load_s));
    if(!load){

       return XRTP_EMEM;
    }
    
    load->packet = pac;
    load->ts = ts;

    pipe->stop = 0;

    load->nextstep = pipe_first_step(pipe);
    
    while( load->nextstep >= 0 ){
         
       pipe_log("pipe_pump: in step[%d]\n", load->nextstep);
       step = pipe->steps[load->nextstep];

       step->load = load;

       if(_pipe_step(step) == XRTP_CONSUMED){    /* pipe step */

          free(step->load);
          step->load = NULL;

          return XRTP_CONSUMED;
       }

       step->load = NULL;
          
       if(pipe->stop){

          break;
       }

       load->nextstep = pipe_next_step(pipe, load->nextstep);
    }
       
    if(!pipe->stop && pipe->type == XRTP_RTP){  /* Produce the rtp packet successfully */

       /* return with the RTP packet for scheduling */
       pipe_log("pipe_pump: rtp packet ready for scheduling\n");
    }

    if(!pipe->stop && pipe->type == XRTP_RTCP){   /* Produce the rtcp packet successfully */

       /* return with the RTCP data for sending */
       /* Little confusion, may clarify these later */
       pipe_log("pipe_pump: rtcp ready for sending\n");
    }

    if(pipe->stop){  /* pipe interupted, consume the packet */
       
       pipe_log("< pipe_pump: Fail to produce the packet >\n");
       
       if(pipe->type == XRTP_RTP)
          rtp_packet_done((xrtp_rtp_packet_t*)pac);
       
       if(pipe->type == XRTP_RTCP)
          rtcp_compound_done((xrtp_rtcp_compound_t*)pac);

       free(step->load);
       step->load = NULL;

       return 0;
    }
    
    if(pipe->direct == XRTP_SEND && packet_bytes != NULL)
       *packet_bytes = load->packet_bytes;


    free(step->load);
    step->load = NULL;

    pipe_log("pipe_pump: packet produced.\n");
    
    return XRTP_OK;
 }

 /**
  * Control system of pipeline
  */
 int _pipe_callback_step_idel(void * gen){
   
    pipe_step_t * step = (pipe_step_t *)gen;

    pipe_log("_pipe_callback_step_idel: Get a call from step@%d\n", (int)step);

    step->pipe->stop  = 1;

    return XRTP_OK;
 }

 int _pipe_callback_step_unload(void * gen){

    pipe_step_t * step = (pipe_step_t *)gen;

    pipe_log("_pipe_callback_step_unload: Get a call from step@%d\n", (int)step);

    step->load->packet  = NULL;

    return XRTP_OK;                        
 }

 int pipe_control(pipe_step_t* step, int type, void* *ret_callback, void* *ret_data){

    pipe_log("pipe_get_step_callback: Get callback(%d)\n", type);
    
    if(step == NULL){
       pipe_log("pipe_get_step_callback: The step is not given\n");
       return XRTP_EPARAM;
    }

    switch(type){

       case(PIPE_CALLBACK_STEP_IDEL):
          pipe_log("pipe_get_step_callback: Found callback.step_idel(step@%d)\n", (int)step);

          *ret_callback = _pipe_callback_step_idel;
          *ret_data = step;
          break;
          
       case(PIPE_CALLBACK_STEP_UNLOAD):
          pipe_log("pipe_get_step_callback: Found callback.step_unload(step@%d)\n", (int)step);

          *ret_callback = _pipe_callback_step_unload;
          *ret_data = step;
          break;
          
       default:
          return XRTP_EUNSUP;
    }

    return XRTP_OK;
 }
