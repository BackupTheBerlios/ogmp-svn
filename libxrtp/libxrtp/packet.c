/***************************************************************************
                          packet.c  -  Packet Manipulater
                             -------------------
    begin                : Fri Apr 18 2003
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

#include "session.h"
#include <stdlib.h>
 
#include "const.h"
#include <timedia/xmalloc.h>
#include <timedia/xstring.h>
 
#include "stdio.h"
/*
#define PACKET_LOG
#define PACKET_DEBUG
*/
#ifdef PACKET_LOG
 #define packet_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define packet_log(fmtargs)
#endif

#ifdef PACKET_DEBUG
 #define packet_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define packet_debug(fmtargs)
#endif

 /* ------------------- General Packet interface ------------------------ */

 /**
  * See packet.h
  */
 int rtp_packet_set_handler(xrtp_rtp_packet_t * pac, void * hand){

    pac->handler = hand;
    if(!hand){
       packet_log(("rtp_packet_set_handler: No handler assiated\n"));
    }

    return XRTP_OK;
 }

 /**
  * See packet.h
  */
 int rtcp_compound_set_handler(xrtp_rtcp_compound_t * com, void * hand){

    com->handler = hand;
    if(!hand){
       packet_log(("rtp_packet_set_handler: No handler assiated\n"));
    }

    return XRTP_OK;
 }

 /* --------------------- RTP Packet interface ------------------------ */

 /**
  * See packet.h
  */
 xrtp_rtp_packet_t * rtp_new_packet(xrtp_session_t * ses, int pt, enum xrtp_direct_e dir, session_connect_t * conn
									, rtime_t ms_arrival, rtime_t us_arrival, rtime_t ns_arrival)
 {
    xrtp_rtp_packet_t *rtp;

    rtp = (xrtp_rtp_packet_t *)xmalloc(sizeof(struct xrtp_rtp_packet_s));
    if(!rtp)
	{
       packet_log(("rtp_new_packet: No memory\n"));
       return NULL;
    }

    memset(rtp, 0, sizeof(struct xrtp_rtp_packet_s));

    /* for sync rtp only  
    rtp->is_sync = 0; 
    rtp->hi_ntp = rtp->lo_ntp = 0;
    */   
    rtp->session = ses;
    rtp->handler = NULL;
    rtp->valid_to_get = 0;

    if(dir == RTP_RECEIVE)
	{
        rtp->msec_arrival = ms_arrival;
        rtp->usec_arrival = us_arrival;
		rtp->nsec_arrival = ns_arrival;
    }
    
    rtp->connect = conn;
    
    rtp->direct = dir;

    rtp->headext = NULL;
    rtp->$head.version = RTP_VERSION;

    rtp->$head.n_CSRC = 0;
    rtp->$head.pt = pt;
    rtp->$payload.len = 0;
    
    rtp->packet_bytes = RTP_HEAD_FIX_BYTE;

    rtp->buffer = NULL;
    
    packet_log(("rtp_packet_new: New packet head %d bytes\n", rtp->packet_bytes));

    return rtp;
 }

 /**
  * Associate a buffer to packet
  */
 int rtp_packet_set_buffer(xrtp_rtp_packet_t * pac, xrtp_buffer_t * buf)
 {
    if(pac->buffer)
       buffer_done(pac->buffer);

    pac->buffer = buf;

    return XRTP_OK;
 }

 /**
  * Get rtp buffer
  */
 xrtp_buffer_t * rtp_packet_buffer(xrtp_rtp_packet_t * pac){

    return pac->buffer;
 }

 /**
  * Release head extension
  */
 int rtp_packet_done_headext(xrtp_rtp_packet_t * pac, xrtp_rtp_headext_t * hext)
 {
    if(pac->direct == RTP_SEND)
    {
       packet_log(("_rtp_packet_done_headext: free data as its seperated from packet in sending\n"));
       xfree(hext->data);
    }
       
    xfree(hext);
    pac->headext = NULL;
    
    packet_log(("rtp_packet_done_headext: headext freed\n"));

    return XRTP_OK;
 }

/**
 * Release payload data
 */
int rtp_packet_done_payload(xrtp_rtp_packet_t *rtp, xrtp_rtp_payload_t *pay)
{
    /*
     * payload can not be free, as it's not owned by rtp packet
     */   
    rtp->$payload.data = NULL;
    packet_log(("rtp_packet_done_payload: unlink payload data\n"));

    return XRTP_OK;
}

 /**
  * See packet.h
  *
  * FIXME: if the data is const value, cause segment faulty!
  */
 int rtp_packet_done(xrtp_rtp_packet_t * pac)
 {
    if(pac->headext)
       rtp_packet_done_headext(pac, pac->headext);
    
    if(pac->$payload.data)
       rtp_packet_done_payload(pac, &(pac->$payload));
    
    if(pac->connect)
        connect_done(pac->connect);
        
    if(pac->buffer)
        buffer_done(pac->buffer);
    
    xfree(pac);
    
    return XRTP_OK;
 }

 /**
  * See packet.h
  */
 int rtp_packet_validate(xrtp_rtp_packet_t * pac){

    pac->valid_to_get = 1;



    return XRTP_OK;
 }

 int rtp_packet_valid(xrtp_rtp_packet_t * pac){

    return pac->valid_to_get;
 }

 /**
  * Specify the maximum payload length of a rtp packet
  *
  * param in: value zero means default length.
  */
 int rtp_packet_set_maxlen(xrtp_rtp_packet_t * pac, uint maxlen){

    int ret = OS_OK;

	if(pac->buffer && maxlen != buffer_maxlen(pac->buffer))
	{
       
		ret = buffer_newsize(pac->buffer, maxlen);

		if(ret < OS_OK) 
			return ret;
    }
    
    return OS_OK;
 }

 uint rtp_packet_maxlen(xrtp_rtp_packet_t * pac){

    return buffer_maxlen(pac->buffer);
 }
 
 uint rtp_packet_payload_maxlen(xrtp_rtp_packet_t * pac){

    uint len = rtp_packet_maxlen(pac) - RTP_HEAD_FIX_BYTE;
    
    len -= pac->$head.n_CSRC * RTP_CSRC_BYTE;

    if(pac->headext)
       len = len - pac->headext->len - RTP_HEADEXT_FIX_BYTE;

    return len;
 }

 uint rtp_packet_bytes(xrtp_rtp_packet_t * pac){

    uint len = RTP_HEAD_FIX_BYTE;

    len += pac->$head.n_CSRC * RTP_CSRC_BYTE;
    
    if(pac->headext)    
       len = len + pac->headext->len + RTP_HEADEXT_FIX_BYTE;

    len = len + pac->$payload.len;

    return len;    
 }

 int rtp_packet_set_mark(xrtp_rtp_packet_t * pac, int boolen){

    pac->$head.mark = boolen;
    return XRTP_OK;
 }

 int rtp_packet_mark(xrtp_rtp_packet_t * pac){
    
    return pac->$head.mark;
 }
 
 int rtp_packet_set_seqno(xrtp_rtp_packet_t * pac, uint16 seqno){

    pac->$head.seqno = seqno;
    return XRTP_OK;
 }

 uint16 rtp_packet_seqno(xrtp_rtp_packet_t * pac){

    return pac->$head.seqno;
 }

 int rtp_packet_set_timestamp(xrtp_rtp_packet_t * pac, uint32 ts){

    pac->$head.timestamp = ts;
    return XRTP_OK;
 }

 uint32 rtp_packet_timestamp(xrtp_rtp_packet_t * pac){

    return pac->$head.timestamp;
 }

 /**
  * Add a CSRC and return num of CSRC in the packet.
  */
 int rtp_packet_add_csrc(xrtp_rtp_packet_t * pac, uint32 CSRC){

    int ncsrc = pac->$head.n_CSRC;

    if(ncsrc == XRTP_MAX_CSRC){

       packet_log(("rtp_packet_add_csrc: Maximum reached\n"));
       return 0;                                     
    }   
    pac->$head.csrc_list[pac->$head.n_CSRC++] = CSRC;

    pac->packet_bytes += RTP_CSRC_BYTE;
    
    return pac->$head.n_CSRC;
 }

 /**
  * See packet.h
  */
 xrtp_rtp_head_t * rtp_packet_set_head(xrtp_rtp_packet_t * pac, uint32 ssrc, uint16 seqno)
 {
    pac->$head.SSRC = ssrc;
    pac->$head.seqno = seqno;

    packet_log(("rtp_packet_set_head: ssrc=%u, seqno=%u\n", ssrc, seqno));

    return &(pac->$head);
 }

 uint32 rtp_packet_ssrc(xrtp_rtp_packet_t * rtp)
 {
    return rtp->$head.SSRC;
 }
 
 /**
  * See packet.h
  */
 xrtp_rtp_headext_t * _rtp_packet_new_headext(xrtp_rtp_packet_t * pac, uint16 info, uint16 len, char * xdat){

    xrtp_rtp_headext_t * hx = (xrtp_rtp_headext_t *)xmalloc(sizeof(struct xrtp_rtp_headext_s));
    if(!hx){

       packet_log(("rtp_packet_new_headext: Can't allocate resource\n"));

       return NULL;
    }

    pac->headext = hx;
    hx->info = info;
    hx->len = len;
    hx->data = xdat;   /* Q: If need copy data */

    pac->packet_bytes = pac->packet_bytes + RTP_HEADEXT_FIX_BYTE + len;

    return hx;
 }

 /**
  * Set head extension in the packet.
  */
 int rtp_packet_set_headext(xrtp_rtp_packet_t * pac, uint16 info, uint16 len, char * xdata){

    xrtp_rtp_headext_t * hx = (xrtp_rtp_headext_t *)xmalloc(sizeof(struct xrtp_rtp_headext_s));
    if(!hx){

       packet_log(("rtp_packet_new_headext: Can't allocate resource\n"));

       return XRTP_EMEM;
    }

    if(pac->headext)
       xfree(pac->headext);

    hx->info = info;
    hx->len = len;
    hx->data = xdata;   /* Q: If need copy data */
    
    hx->len_dw = (hx->len % 4) ? (hx->len / 4 + 1) : (hx->len / 4);
    hx->len_pad = hx->len_dw * 4 - hx->len;
    
    pac->headext = hx;
    pac->$head.ext = 1;

    pac->packet_bytes += RTP_HEADEXT_FIX_BYTE + hx->len + hx->len_pad;
    
    packet_log(("rtp_packet_set_headext: RTP headext data %d bytes\n", len));
    packet_log(("rtp_packet_set_headext: RTP headext pad %d bytes\n", hx->len_pad));
    packet_log(("rtp_packet_set_headext: RTP headext %d bytes\n", RTP_HEADEXT_FIX_BYTE + hx->len + hx->len_pad));
    packet_log(("rtp_packet_set_headext: packet+headext %d bytes\n", pac->packet_bytes));

    return XRTP_OK;
 }

 /**
  * See packet.h
  */
 uint16 rtp_packet_headext_info(xrtp_rtp_packet_t * pac){

    if(!pac->headext){

       return 0;
    }

    return pac->headext->info;
 }

 /**
  * See packet.h
  */
 uint16 rtp_packet_headext(xrtp_rtp_packet_t * pac, char* *ret_data){

    if(!pac->headext){

       *ret_data = NULL;
       return 0;
    }

    *ret_data = pac->headext->data;
    return pac->headext->len;
 }

 /**
  * Set Payload in the packet.
 int rtp_packet_set_payload(xrtp_rtp_packet_t * pac, int len, char * pay){
  */
 int rtp_packet_set_payload(xrtp_rtp_packet_t *rtp, buffer_t *payload_buf)
 {

    rtp->$payload.out_buffer = payload_buf;

    if(payload_buf == NULL)
    {
		rtp->$payload.data = NULL;
		/*rtp->$payload.len = 0; use the value later*/
		packet_log(("_rtp_packet_set_payload: unset payload\n"));

		return XRTP_OK;
	}
	
	/* Don't need to copy data for performance consider*/
    rtp->$payload.data = buffer_data(payload_buf);

	 rtp->$payload.len = buffer_datalen(payload_buf);

    rtp->packet_bytes += rtp->$payload.len;

    packet_log(("_rtp_packet_set_payload: payload %d bytes\n", rtp->$payload.len));


    packet_log(("_rtp_packet_set_payload: packet+headext+payload %d bytes\n", rtp->packet_bytes));

    return XRTP_OK;
 }


 /**
  * See packet.h
  */
 xrtp_rtp_payload_t * rtp_packet_new_payload(xrtp_rtp_packet_t * pac, int len, char * pay){

    /* Don't need to copy data for performance consider*/
    pac->$payload.data = xmalloc(len);
    
    if(!pac->$payload.data){

        packet_log(("< rtp_packet_new_payload: Fail to allocate payload memery >\n"));

        return NULL;
    }

    memcpy(pac->$payload.data, pay, len);
    pac->$payload.len = len;
    pac->packet_bytes += len;
        
    packet_log(("rtp_packet_new_payload: payload copied, packet size is %d bytes\n", pac->packet_bytes));

    return &(pac->$payload);
 }

 /**
  * See packet.h
  */
 uint rtp_packet_payload(xrtp_rtp_packet_t * pac, char **ret_payload)
 {
    if(pac->$payload.len == 0)
    {
       *ret_payload = NULL;
       return 0;
    }

    *ret_payload = pac->$payload.data;
    
    return pac->$payload.len;
 }

 uint rtp_packet_dump_payload(xrtp_rtp_packet_t * rtp, char **r_payload){


    uint len = rtp->$payload.len;

    if(len == 0){

       *r_payload = NULL;
       return 0;
    }

    if(rtp->direct == RTP_SEND){

        *r_payload = rtp->$payload.data;
        
    }else{
      
        *r_payload = xmalloc(rtp->$payload.len);
        if(!*r_payload){

            *r_payload = NULL;
            return 0;
        }

        memcpy(*r_payload, rtp->$payload.data, len);
    }
    
    rtp->$payload.data = NULL;
    
    return len;
 }
 
 uint rtp_packet_payload_bytes(xrtp_rtp_packet_t * rtp){

    return rtp->$payload.len;
 }

 /* Tag for Scheduling proposal */
 int rtp_packet_set_info(xrtp_rtp_packet_t * rtp, int bytes){

    rtp->info_bytes = bytes;
    
    return XRTP_OK;
 }

 int rtp_packet_info(xrtp_rtp_packet_t * rtp, int *bytes){

    *bytes = rtp->info_bytes;
    
    return XRTP_OK;
 }

int rtp_packet_arrival_time(xrtp_rtp_packet_t * rtp, rtime_t *ms, rtime_t *us, rtime_t *ns)
{
	*ms = rtp->msec_arrival;
	*us = rtp->usec_arrival;
	*ns = rtp->nsec_arrival;

	return XRTP_OK;
}

 /* --------------------- RTCP Compound interface ------------------------ */

 /**
  * Create a empty RTCP compound packet for recreate the rtcp structure.
  *
  * param in: max number of packets allowed
  */
 xrtp_rtcp_compound_t* rtcp_new_compound(xrtp_session_t * ses, enum xrtp_direct_e dir, session_connect_t * conn, 
                            rtime_t msec, rtime_t usec, rtime_t nsec)
 {

    xrtp_rtcp_compound_t * comp = (xrtp_rtcp_compound_t *)xmalloc(sizeof(struct xrtp_rtcp_compound_s));
    
    if(!comp)
       return NULL;
    
    memset(comp, 0, sizeof(struct xrtp_rtcp_compound_s));

    comp->session = ses;
    
    if(dir == RTP_RECEIVE)
    {
       comp->msec = msec;
       comp->usec = usec;
       comp->nsec = nsec;
    }

    comp->connect = conn;


    comp->direct = dir;

    comp->valid_to_get = 0;
    comp->first_head = NULL;

    comp->bytes = 0;

    comp->max_packets = XRTP_MAX_RTCP_PACKETS;
    comp->maxlen = XRTP_RTCP_DEFAULTMAXBYTES;
    comp->buffer = NULL;

    return comp;
 }
 
 /**
  * The incoming data buffer for rtcp compound
  */
 int rtcp_set_buffer(xrtp_rtcp_compound_t * rtcp, xrtp_buffer_t * buf)
 {
    if(rtcp->buffer)
       buffer_done(rtcp->buffer);

    rtcp->buffer = buf;

    return XRTP_OK;
 }

 xrtp_buffer_t * rtcp_buffer(xrtp_rtcp_compound_t * rtcp)
 {
    return rtcp->buffer;
 }

 xrtp_rtcp_sr_t * rtcp_make_sr_packet(uint8 ver, uint8 pad, uint8 count, uint16 len)
 {
    xrtp_rtcp_sr_t *sr;

    if(len*4 < RTCP_SENDERINFO_BYTE + RTCP_REPORT_BYTE * count)
    {
       packet_debug(("rtcp_make_sr_packet: Bad format\n"));
       return NULL;
    }
    
    sr = (xrtp_rtcp_sr_t *)xmalloc(sizeof(struct xrtp_rtcp_sr_s));
    if(!sr)
    {
       packet_debug(("rtcp_make_sr_packet: No memory\n"));
       return NULL;
    }

    memset(sr, 0, sizeof(*sr));
    
    sr->$head.version = ver;
    sr->$head.padding = pad;
    sr->$head.count = count;
    sr->$head.type = RTCP_TYPE_SR;
    sr->$head.length = len;
    sr->$head.bytes = len * 4; /* without head bytes */

    return sr;
 }
 
 xrtp_rtcp_rr_t * rtcp_make_rr_packet(uint8 ver, uint8 pad, uint8 count, uint16 len)
 {
    xrtp_rtcp_rr_t *rr;

    if(4*len < RTCP_SSRC_BYTE + count * RTCP_REPORT_BYTE)
    {
       packet_debug(("rtcp_make_rr_packet: Bad format\n"));
       return NULL;
    }
       
    rr = (xrtp_rtcp_rr_t *)xmalloc(sizeof(struct xrtp_rtcp_rr_s));
    if(!rr)
    {
       packet_debug(("rtcp_make_rr_packet: No memory\n"));
       return NULL;
    }
    
    rr->$head.version = ver;
    rr->$head.padding = pad;
    rr->$head.count = count;
    rr->$head.type = RTCP_TYPE_RR;
    rr->$head.length = len;
    rr->$head.bytes = len * 4; /* without head bytes */

    return rr;
 }

 /**
  * See packet.h
  */
 xrtp_rtcp_compound_t * rtcp_sender_report_new(xrtp_session_t * ses, int padding)
 {
    xrtp_rtcp_sr_t * sr;

    xrtp_rtcp_compound_t * rtcp = (xrtp_rtcp_compound_t *)xmalloc(sizeof(struct xrtp_rtcp_compound_s));
    if(!rtcp)
	 {
       packet_log(("rtcp_sender_report_new: Can't allocate resource\n"));
       
       return NULL;
    }
    
    memset(rtcp, 0, sizeof(struct xrtp_rtcp_compound_s));
    
    rtcp->max_packets = XRTP_MAX_RTCP_PACKETS;

    rtcp->session = ses;
    rtcp->handler = NULL;
    
    rtcp->valid_to_get = 0;
    rtcp->padding = padding;
    rtcp->direct = RTP_SEND;

    sr = (xrtp_rtcp_sr_t*)xmalloc(sizeof(struct xrtp_rtcp_sr_s));
    if(!sr)
	 {
       packet_log(("rtcp_sender_report_new: Can't allocate resource for sender report packet\n"));

       xfree(rtcp->heads);
       xfree(rtcp);
       return NULL;
    }
    memset(sr, 0, sizeof(*sr));

    sr->$head.version = 2;
    sr->$head.padding = padding;
    sr->$head.count = 0;
    sr->$head.type = RTCP_TYPE_SR;
    
    sr->$head.bytes = RTCP_HEAD_FIX_BYTE + RTCP_SENDERINFO_BYTE;
    sr->$head.length = sr->$head.bytes/4 - 1;   /*RFC 1889*/
    sr->ext = NULL;

    rtcp->heads[0] = rtcp->first_head = (xrtp_rtcp_head_t *)sr;
    
    rtcp->n_packet = 1;

    rtcp->maxlen = XRTP_RTCP_DEFAULTMAXBYTES;

    rtcp->bytes = sr->$head.bytes;

    packet_log(("rtcp_sender_report_new: SR = %d Bytes\n", rtcp_length(rtcp)));

    return rtcp;
 }

 /**
  * See packet.h

  */
 xrtp_rtcp_compound_t * rtcp_receiver_report_new(xrtp_session_t * ses, uint32 ssrc, int padding)
 {
    xrtp_rtcp_rr_t * rr;
    
    xrtp_rtcp_compound_t * rtcp = (xrtp_rtcp_compound_t *)xmalloc(sizeof(struct xrtp_rtcp_compound_s));
    if(!rtcp){

       packet_log(("rtcp_reveiver_report_new: Can't allocate resource\n"));

       return NULL;
    }

    memset(rtcp, 0, sizeof(struct xrtp_rtcp_compound_s));

    rtcp->max_packets = XRTP_MAX_RTCP_PACKETS;

    rtcp->session = ses;
    rtcp->handler = NULL;
    
    rtcp->valid_to_get = 0;
    rtcp->padding = padding;
    rtcp->direct = RTP_SEND;

    rr = (xrtp_rtcp_rr_t *)xmalloc(sizeof(struct xrtp_rtcp_rr_s));
    if(!rr){

       packet_log(("rtcp_receiver_report_new: Can't allocate resource for sender report packet\n"));

       xfree(rtcp->heads);
       xfree(rtcp);
       return NULL;
    }
    memset(rr, 0, sizeof(*rr));
    
    rr->$head.count = 0;
    rr->$head.version = 2;
    rr->$head.padding = 0;
    rr->$head.type = RTCP_TYPE_RR;

    rr->$head.bytes = RTCP_HEAD_FIX_BYTE + RTCP_SSRC_BYTE;
    rr->$head.length = rr->$head.bytes / 4 - 1 ;

    rr->SSRC = ssrc;

    rr->ext = NULL;

    rtcp->heads[0] = rtcp->first_head = (xrtp_rtcp_head_t *)rr;

    rtcp->n_packet = 1;

    rtcp->maxlen = XRTP_RTCP_DEFAULTMAXBYTES;
    rtcp->bytes = rr->$head.bytes;

    packet_log(("rtcp_receiver_report_new: RR is %d bytes\n", rtcp_length(rtcp)));

    return rtcp;
 }

 uint8 rtcp_type(xrtp_rtcp_compound_t * com){

    return com->first_head->type;


 }

 /**
  * See packet.h
  */
 int rtcp_validate(xrtp_rtcp_compound_t * com){

    com->valid_to_get = 1;

    return XRTP_OK;
 }

 int rtcp_valid(xrtp_rtcp_compound_t * com){

    return com->valid_to_get;
 }

 uint32 rtcp_sender(xrtp_rtcp_compound_t * com){

    xrtp_rtcp_sr_t * sr;
    xrtp_rtcp_rr_t * rr;

    switch(com->first_head->type){

       case(RTCP_TYPE_SR):

          sr = (xrtp_rtcp_sr_t *)com->first_head;

          return sr->$sender.SSRC;

       case(RTCP_TYPE_RR):

          rr = (xrtp_rtcp_rr_t *)com->first_head;

          return rr->SSRC;
    }

    return XRTP_SRC_NULL;
 }

 /*********************************************************
  * Methods for SDES Packet
  */
 xrtp_rtcp_sdes_t * rtcp_new_sdes(xrtp_rtcp_compound_t * com)
 {
    xrtp_rtcp_sdes_t * sdes;
    
    if(com->n_packet == com->max_packets)
    {
       packet_log(("rtcp_new_sdes: Maximum packet reached\n"));

       return NULL;
    }
    
    sdes = (xrtp_rtcp_sdes_t *)xmalloc(sizeof(struct xrtp_rtcp_sdes_s));
    if(!sdes)
    {
       packet_log(("rtcp_new_sdes: No memory\n"));
       return NULL;
    }

    memset(sdes, 0, sizeof(struct xrtp_rtcp_sdes_s));
    
    sdes->$head.count = 0;
    sdes->$head.version = 2;
    sdes->$head.padding = 0;
    sdes->$head.type = RTCP_TYPE_SDES;
    sdes->$head.bytes = RTCP_HEAD_FIX_BYTE;
    sdes->$head.length = sdes->$head.bytes / 4 - 1;   /* RFC 1889 */


    com->heads[com->n_packet++] = (xrtp_rtcp_head_t *)sdes;
    
    packet_log(("rtcp_new_sdes: %d rtcp packets\n", com->n_packet));

    return sdes;
 }
 
 /***** Releasing SDES Packet.Chunk.Item *****/
 
 int rtcp_done_sdes_item(xrtp_rtcp_sdes_item_t * item)
 {
    packet_log(("rtcp_done_sdes_item: xfree item[@%d]\n", (int)item));

    xfree(item);

    return XRTP_OK;
 }

 /***** Releasing SDES Packet.Chunk *****/
 
 int rtcp_done_sdes_chunk(xrtp_rtcp_sdes_chunk_t * chunk)
 {
    int i;

    for(i=0; i<chunk->n_item; i++)
		rtcp_done_sdes_item(chunk->items[i]);

    packet_log(("rtcp_done_sdes_chunk: xfree chunk[@%d]\n", (int)chunk));

	xfree(chunk);
    
    return XRTP_OK;
 }
 
 /***** Releasing SDES Packet *****/
 
 int rtcp_done_sdes(xrtp_rtcp_sdes_t * sdes){

    int i;
	
    for(i=0; i<sdes->$head.count; i++)
       rtcp_done_sdes_chunk(sdes->chunks[i]);

    packet_log(("rtcp_done_sdes: xfree sdes[@%d]\n", (int)sdes));

	xfree(sdes);

    return XRTP_OK;
 }
 
 /***** Add SDES Item to Packet *****/
 
 int rtcp_add_sdes(xrtp_rtcp_compound_t * com, uint32 ssrc, uint8 type, uint8 len, char *v)
 {
    int i;
    xrtp_rtcp_sdes_t * sdes = NULL;
    xrtp_rtcp_sdes_chunk_t * chunk = NULL;
    xrtp_rtcp_sdes_item_t *item = NULL;

    int new_sdes = 0, new_chunk = 0, new_item = 0;

    int addlen = 0;
    int room = com->maxlen - com->bytes;

    packet_log(("rtcp_add_sdes: %dB room\n", room));

    for(i=0; i<com->n_packet; i++)
    {
       if(com->heads[i]->type == RTCP_TYPE_SDES)
       {
          sdes = (xrtp_rtcp_sdes_t *)com->heads[i];
          packet_log(("rtcp_add_sdes: Found SDES packet\n"));
          break;
       }
    }

    if(!sdes)
    {
	   if(com->n_packet == com->max_packets)
	   {
          packet_log(("rtcp_add_sdes: maximum packet, no more addition\n"));
          return XRTP_EREFUSE;
	   }

       room -= (RTCP_HEAD_FIX_BYTE + RTCP_SRC_BYTE + RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
       if(room < len)
       {
          packet_log(("rtcp_add_sdes: No room for sdes, need bytes[%d]\n", len));
          return XRTP_EREFUSE;
       }

       new_sdes = new_chunk = new_item = 1;
    }
    else
    {
       chunk = NULL;

       for(i=0; i<sdes->$head.count; i++)
       {
          if(sdes->chunks[i]->SRC == ssrc)
          {
             chunk = sdes->chunks[i];
             packet_log(("rtcp_add_sdes: Found SRC[%d] chunk\n", ssrc));
             break;
          }
       }
       
       if(!chunk)
       {
          if(sdes->$head.count == XRTP_MAX_SDES_CHUNK)
          {
             packet_log(("rtcp_add_sdes(): reach maximum chunks[%d]\n", XRTP_MAX_SDES_CHUNK));
             return XRTP_EREFUSE;
          }

          room -= (RTCP_SRC_BYTE + RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
          if(room < len)
          {
             packet_log(("rtcp_add_sdes: No room for chunk, need bytes[%d]\n", room));
             return XRTP_EREFUSE;
          }

          new_chunk = new_item = 1;
       }
       else
       {
          item = NULL;
          
          if(chunk->ended) return XRTP_EREFUSE;

          for(i=0; i<chunk->n_item; i++)
          {
             if(chunk->items[i]->id == type)
             {
                item = chunk->items[i];
                packet_log(("rtcp_add_sdes: Found item type[%d]\n", type));
                break;
             }
          }

          if(!item)
          {
             room -= (RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
             if(room < len)
             {
                packet_log(("rtcp_add_sdes: No room for item, need bytes[%d]\n", room));
                return XRTP_EREFUSE;
             }

             new_item = 1;
          }
          else
          {
             packet_log(("rtcp_add_sdes: Replace old item\n"));

             addlen = len - item->len;

             item->len = len;
             memcpy(item->value, v, len);

             com->bytes += addlen;

             return XRTP_OK;
          }
       }
    }

    if(new_chunk)
    {
       chunk = (xrtp_rtcp_sdes_chunk_t *)xmalloc(sizeof(struct xrtp_rtcp_sdes_chunk_s));
       if(!chunk)
       {
          packet_log(("rtcp_add_sdes: No memory for Chunk\n"));
          return XRTP_EMEM;
       }
       
       packet_log(("rtcp_add_sdes: new chunk[@%d]\n", (int)chunk));
       memset(chunk, 0, sizeof(struct xrtp_rtcp_sdes_chunk_s));

       chunk->SRC = ssrc;

       addlen += RTCP_SRC_BYTE;
       
       packet_log(("rtcp_add_sdes: New Chunk{ssrc[%d],%dB}\n", ssrc, RTCP_SRC_BYTE));
    }

    if(new_item)
    {
       item = (xrtp_rtcp_sdes_item_t *)xmalloc(sizeof(struct xrtp_rtcp_sdes_item_s));
       if(!item)
       {
          if(new_chunk) xfree(chunk);
          packet_log(("rtcp_add_sdes: No memory for Item\n"));
          return XRTP_EMEM;
       }

       packet_log(("rtcp_add_sdes: new item[@%d]\n", (int)item));
       memset(item, 0, sizeof(struct xrtp_rtcp_sdes_item_s));

       item->id = type;
       item->len = len;
       memcpy(item->value, v, len);

       chunk->items[chunk->n_item++] = item;

       addlen += RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE + len;
       
       packet_log(("rtcp_add_sdes: New item{id[%d,%dB]}\n", type, len));
    }

    if(!new_sdes && new_chunk)
       sdes->chunks[sdes->$head.count++] = chunk;

    if(new_sdes)
    {
       sdes = rtcp_new_sdes(com); /* Create a SDES and add to compound */

       if(!sdes)
       {
          if(new_chunk)
             xfree(chunk);

          if(new_item)
             xfree(item);

          return XRTP_EREFUSE;
       }

       sdes->chunks[sdes->$head.count++] = chunk;
    }
    
    sdes->$head.bytes += addlen;
    
    packet_log(("rtcp_add_sdes: %d chunks, %dB\n", sdes->$head.count, sdes->$head.bytes));

    return addlen;
 }

 /**
  * Add Private SDES Item to Packet
  */
 int rtcp_add_priv_sdes(xrtp_rtcp_compound_t * com, uint32 SRC, uint8 pre_len, char * pre_v, uint8 len, char * v){

    int i;
    xrtp_rtcp_sdes_t * sdes = NULL;
    xrtp_rtcp_sdes_chunk_t * chunk = NULL;
    xrtp_rtcp_sdes_item_t *item = NULL;

    int new_sdes = 0, new_chunk = 0, new_item = 0;

    int _addlen = 0;
    int _room = com->maxlen - com->bytes;

    packet_log(("rtcp_add_priv_sdes: %d bytes to carry data\n", _room));

    for(i=0; i<com->n_packet; i++)
    {
       if(com->heads[i]->type == RTCP_TYPE_SDES)
       {
          sdes = (xrtp_rtcp_sdes_t *)com->heads[i];

          packet_log(("rtcp_add_priv_sdes: Found SDES packet\n"));

          break;
       }
    }

    if(!sdes)
    {
       _room -= (RTCP_HEAD_FIX_BYTE + RTCP_SRC_BYTE + RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
       if(_room < len)
       {
          return XRTP_EREFUSE;
       }

       packet_log(("rtcp_add_priv_sdes: Need a new SDES Packet\n"));

       new_sdes = new_chunk = new_item = 1;
    }
    else
    {
       chunk = NULL;

       for(i=0; i<sdes->$head.count; i++)
       {
          if(sdes->chunks[i]->SRC == SRC)
          {
             chunk = sdes->chunks[i];

             packet_log(("rtcp_add_priv_sdes: Found Chunk for SRC(%d)\n", SRC));

             break;
          }
       }

       if(!chunk)
       {
          if(sdes->$head.count == XRTP_MAX_SDES_CHUNK)
          {
             packet_log(("rtcp_add_priv_sdes: No more chunk then %d\n", XRTP_MAX_SDES_CHUNK));

             return XRTP_EREFUSE;
          }

          _room -= (RTCP_SRC_BYTE + RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
          if(_room < len)
          {
             packet_log(("rtcp_add_priv_sdes: SDES Chunk needs more then %d bytes!\n", _room));

             return XRTP_EREFUSE;
          }

          packet_log(("rtcp_add_priv_sdes: Need a new SRC(%d) Chunk\n", SRC));

          new_chunk = new_item = 1;
       }
       else
       {
          item = NULL;

          if(chunk->ended) return XRTP_EREFUSE;

          for(i=0; i<chunk->n_item; i++)
          {
             if(chunk->items[i]->id == RTCP_SDES_PRIV)
             {
                item = chunk->items[i];

                packet_log(("rtcp_add_priv_sdes: Found private sdes item\n"));

                break;
             }
          }

          if(!item)
          {
             _room -= (RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
             if(_room < len)
             {
                packet_log(("rtcp_add_priv_sdes: SDES Item needs more then %d bytes!\n", _room));

                return XRTP_EREFUSE;
             }

             packet_log(("rtcp_add_priv_sdes: Need a new private SDES item\n"));

             new_item = 1;
          }
          else
          {
             packet_log(("rtcp_add_priv_sdes: Replace old item\n"));

             _addlen = len + pre_len + RTCP_SDES_LEN_BYTE - item->len;

             item->len = len + pre_len + RTCP_SDES_LEN_BYTE;

             item->$priv.len_prefix = pre_len;
             memcpy(item->$priv.prefix, pre_v, pre_len);
             item->$priv.len_value = len;
             memcpy(item->$priv.value, v, len);

             com->bytes += _addlen;

             return XRTP_OK;
          }
       }
    }

    if(new_chunk)
    {
       chunk = (xrtp_rtcp_sdes_chunk_t *)xmalloc(sizeof(struct xrtp_rtcp_sdes_chunk_s));
       if(!chunk)
       {
          if(new_sdes)
             rtcp_done_sdes(sdes);
       }

       chunk->SRC = SRC;
       chunk->n_item = 0;
       chunk->ended = 0;

       _addlen += RTCP_SRC_BYTE;
       
       packet_log(("rtcp_add_priv_sdes: New Chunk(%d) = %dB\n", SRC, RTCP_SRC_BYTE));
    }

    if(new_item)
    {
       item = (xrtp_rtcp_sdes_item_t *)xmalloc(sizeof(struct xrtp_rtcp_sdes_item_s));
       if(!item)
       {
          packet_log(("rtcp_add_priv_sdes: Can't allocate Item resource\n"));

          if(new_chunk) xfree(chunk);

          return XRTP_EMEM;
       }

       item->id = RTCP_SDES_PRIV;
       
       item->len = len + pre_len + RTCP_SDES_LEN_BYTE;

       item->$priv.len_prefix = pre_len;
       memcpy(item->$priv.prefix, pre_v, pre_len);

       item->$priv.len_value = len;
       memcpy(item->$priv.value, v, len);

       chunk->items[chunk->n_item++] = item;

       _addlen += RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE + item->len;
       
       packet_log(("rtcp_add_priv_sdes: New Item=%dB:chunk(%d)\n", RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE + item->len, chunk->SRC));
    }

    if(!new_sdes && new_chunk)

       sdes->chunks[sdes->$head.count++] = chunk;

    if(new_sdes)
    {
       sdes = rtcp_new_sdes(com); /* Create a SDES and add to compound */
       if(!sdes)
       {
          if(new_chunk) xfree(chunk);

          if(new_item)
          {
             xfree(item->value);
             xfree(item);
          }

          return XRTP_EREFUSE;
       }

       packet_log(("rtcp_add_priv_sdes: New SDES Packet\n"));

       sdes->chunks[sdes->$head.count++] = chunk;

    }

    sdes->$head.bytes += _addlen;

    packet_log(("rtcp_add_priv_sdes: SDES = %dB\n", sdes->$head.bytes));

    return _addlen;
 }
 
 /* Indicate an end of a SDES chunk, and calc the sdes packet bytes */
 int rtcp_end_sdes(xrtp_rtcp_compound_t * rtcp, uint32 SRC){

    xrtp_rtcp_sdes_t * sdes = NULL;
    xrtp_rtcp_sdes_chunk_t * chunk = NULL;

    int i;
    for(i=0; i<rtcp->n_packet; i++){
       
       if(rtcp->heads[i]->type == RTCP_TYPE_SDES){

          sdes = (xrtp_rtcp_sdes_t *)rtcp->heads[i];
          break;
       }
    }
    
    if(!sdes){
       /* No SDES packet found */
       return XRTP_OK;
    }

    for(i=0; i<sdes->$head.count; i++){

       if(sdes->chunks[i]->SRC == SRC){

          chunk = sdes->chunks[i];
          break;
       }
    }

    if(!chunk || chunk->ended){

       return XRTP_OK;  /* Already ended */
    }
    
    /* 32-bit alignment */
    chunk->padlen = 4 - sdes->$head.bytes % 4;
    sdes->$head.bytes = sdes->$head.bytes + chunk->padlen;
    chunk->ended = 1;
    
    packet_log(("rtcp_end_sdes: RTCP is %d bytes\n", rtcp_length(rtcp)));

    return XRTP_OK;        
 }
 /***** Get SDES Item from Packet *****/

 uint8 rtcp_sdes(xrtp_rtcp_compound_t * com, uint32 SRC, uint8 type, char **r_sdes)
 {
    xrtp_rtcp_sdes_t * sdes = NULL;
    xrtp_rtcp_sdes_chunk_t * chunk = NULL;
    xrtp_rtcp_sdes_item_t *item = NULL;

    int i;

    for(i=0; i<com->n_packet; i++){

       if(com->heads[i]->type == RTCP_TYPE_SDES){


          sdes = (xrtp_rtcp_sdes_t *)com->heads[i];
          break;
       }
    }

    if(!sdes){

        packet_log(("< rtcp_sdes: No SDES packet found in rtcp >\n"));
        *r_sdes = NULL;
        return 0;  /* zero length means no found */

    }else{

       for(i=0; i<sdes->$head.count; i++){

          if(sdes->chunks[i]->SRC == SRC){

             chunk = sdes->chunks[i];
             break;
          }
       }

       if(!chunk){

          packet_log(("< rtcp_sdes: No SDES packet for SSRC[%d] found in the packet >\n", SRC));
          *r_sdes = NULL;
          return 0;  /* zero length means no found */

       }else{

          for(i=0; i<chunk->n_item; i++){

             if(chunk->items[i]->id == type){

                item = chunk->items[i];
                break;
             }
          }

          if(!item){

              packet_log(("< rtcp_sdes: No SDES[%d] packet for SSRC[%d] found in the packet >\n", type, SRC));
              *r_sdes = NULL;
              return 0;  /* zero length means no found */

          }else{

             *r_sdes = item->value;
             return item->len;
          }
       }
    }
 }

 uint8 rtcp_priv_sdes(xrtp_rtcp_compound_t * com, uint32 SRC, uint8 pre_len, char * pre_v, char* *ret_v){

    xrtp_rtcp_sdes_t * sdes = NULL;
    xrtp_rtcp_sdes_chunk_t * chunk = NULL;
    xrtp_rtcp_sdes_item_t *item = NULL;

    int i;

    for(i=0; i<com->n_packet; i++){

       if(com->heads[i]->type == RTCP_TYPE_SDES){

          sdes = (xrtp_rtcp_sdes_t *)com->heads[i];
          break;
       }
    }

    if(!sdes){

          return 0;  /* zero length means no found */

    }else{

       for(i=0; i<sdes->$head.count; i++){

          if(sdes->chunks[i]->SRC == SRC){

             chunk = sdes->chunks[i];
             break;
          }
       }

       if(!chunk){

            return 0;  /* zero length means no found */

       }else{

          xrtp_rtcp_sdes_item_t * _item;

          for(i=0; i<chunk->n_item; i++){

             _item = chunk->items[i];

             if(_item->id == RTCP_SDES_PRIV && _item->$priv.len_prefix == pre_len && !xstr_ncomp(_item->$priv.prefix, pre_v, pre_len)){

                item = chunk->items[i];
                break;
             }
          }

          if(!item){

                return 0;  /* zero length means no found */

          }else{

             *ret_v = item->$priv.value;

             return item->len - item->$priv.len_prefix - RTCP_SDES_LEN_BYTE;
          }
       }
    }
 }

 /**********************************************************/

 /*********************************************************
  * Methods for BYE Packet
  */
 xrtp_rtcp_bye_t * rtcp_new_bye(xrtp_rtcp_compound_t * com, uint32 SRCs[], uint8 n_SRC, uint8 len, char * why){

    int i = 0;
    uint8 pad = 0;
	xrtp_rtcp_bye_t * bye;

    uint32 _addlen = RTCP_HEAD_FIX_BYTE + n_SRC * RTCP_SRC_BYTE + RTCP_WHY_BYTE + len;

    packet_log(("rtcp_new_bye: Bye=%dB(Before padding)\n", _addlen));

    pad = (_addlen % 4) ? 4 - _addlen % 4 : 0;
    
    _addlen = pad + _addlen;

    packet_log(("rtcp_new_bye: Bye=%dB(After padding)\n", _addlen));

    if(com->n_packet == com->max_packets){

       packet_log(("rtcp_new_bye: Maximum packet reached\n"));

       return NULL;
    }

    if(_addlen + com->bytes > com->maxlen){

       packet_log(("rtcp_new_bye: Maximum compound size is %d bytes\n", com->maxlen));

       return NULL;
    }

    bye = (xrtp_rtcp_bye_t *)xmalloc(sizeof(struct xrtp_rtcp_bye_s));
    if(!bye){

       packet_log(("rtcp_new_bye: Can't allocate for BYE packet\n"));

       return NULL;
    }

    bye->$head.count = 0;
    bye->$head.version = 2;
    bye->$head.padding = 0;
    bye->$head.type = RTCP_TYPE_BYE;
    bye->$head.bytes = _addlen;
    bye->$head.length = _addlen / 4 - 1;    /* RFC 1889 */

    bye->pad_why = pad;
    bye->len_why = len;
    bye->why = why;

    for(i=0; i<n_SRC; i++){

       bye->srcs[i] = SRCs[i];
    }
    
    bye->$head.count = n_SRC;

    com->heads[com->n_packet++] = (xrtp_rtcp_head_t *)bye;

    return bye;
 }
 
 int rtcp_done_bye(xrtp_rtcp_bye_t * bye)
 {
    xfree(bye->why);

	xfree(bye);

    return XRTP_OK;
 }

 int _rtcp_bye_src(xrtp_rtcp_bye_t * bye, uint32 src){

    int i, c = bye->$head.count;

    if(c == XRTP_MAX_BYE_SRCS)
       return XRTP_EREFUSE;
       
    for(i=0; i<bye->$head.count; i++){

       if(bye->srcs[i] == src)
          return XRTP_OK;
    }

    bye->srcs[bye->$head.count++] = src;

    return XRTP_OK;
 }
 
 uint8 rtcp_bye_why(xrtp_rtcp_compound_t * com, uint32 SSRC, uint32 SRC, char* *ret_why){

    xrtp_rtcp_bye_t * bye;

    int i, j;
    for(i=0; i<com->n_packet; i++){

       if(com->heads[i]->type == RTCP_TYPE_BYE){

          bye = (xrtp_rtcp_bye_t *)com->heads[i];
          for(j=0; j<bye->$head.count; j++){

             if(bye->srcs[j] == SRC){

                 *ret_why = bye->why;
                   
                 return bye->len_why;
             }
          }
       }
    }

    return 0; /* No found */
 }
 
 uint rtcp_bye_from(xrtp_rtcp_compound_t * com, uint32 r_SSRCs[], uint nbuf){

    uint n = 0, c = 0;
    int i;
    xrtp_rtcp_bye_t * bye = NULL;
    
    for(i=0; i<com->n_packet; i++){

       if(com->heads[i]->type == RTCP_TYPE_BYE){

          bye = (xrtp_rtcp_bye_t *)(com->heads[i]);
          n = com->heads[i]->count;
       }
    }

    if(!bye) return 0;

    for(c=0; c< n; c++)
       r_SSRCs[c] = bye->srcs[c];
    
    return n;
 }
 
/**********************************************************/
 
/*********************************************************
 * Methods for APP Packet
 */
xrtp_rtcp_app_t* rtcp_new_app(xrtp_rtcp_compound_t * com, uint32 SSRC, uint8 subtype, char name[4], uint len, char *appdata)
{
    xrtp_rtcp_app_t *app;
	uint32 *nm;

    if(com->n_packet == com->max_packets)
	{

       packet_log(("rtcp_new_app: Maximum packet reached\n"));
       return NULL;
    }

    app = (xrtp_rtcp_app_t*)xmalloc(sizeof(struct xrtp_rtcp_app_s));
    if(!app)
	{
       packet_log(("rtcp_new_app: No memory for APP packet\n"));
       return NULL;
    }
	memset(app, 0, sizeof(struct xrtp_rtcp_app_s));

    app->len_data = len;
    app->data = appdata;

    app->$head.count = subtype;

	app->$head.padding = 0;
    if(len % 4)
	{
		int npad = 4 - len % 4;
		app->$head.padding = 1;

		len += npad;
		packet_log(("rtcp_new_app: need %d bytes padding\n", npad));
    }

    app->$head.version = 2;
    app->$head.type = RTCP_TYPE_APP;
    app->$head.bytes = RTCP_HEAD_FIX_BYTE + RTCP_SRC_BYTE + RTCP_APP_NAME_BYTE + len;
    app->$head.length = app->$head.bytes / 4 - 1;   /* RFC 1889 */
    
    app->SSRC = SSRC;

	nm = (uint32*)app->name;
    *nm = *((uint32*)name);

    com->heads[com->n_packet++] = (xrtp_rtcp_head_t *)app;

    return app;
}
 
int rtcp_done_app(xrtp_rtcp_app_t * app)
{
	packet_log(("rtcp_done_app: app data provider to xfree data, not me!\n"));
    
	xfree(app);

    return XRTP_OK;
}

int rtcp_app(xrtp_rtcp_compound_t *rtcp, uint32 ssrc, uint8 subtype, char name[4], char **appdata)
{
    xrtp_rtcp_app_t * app;
    int i;

    for(i=0; i<rtcp->n_packet; i++)

	{
       if(rtcp->heads[i]->type == RTCP_TYPE_APP && rtcp->heads[i]->count == subtype)
	   {
          uint32 *n;

		  app = (xrtp_rtcp_app_t*)rtcp->heads[i];
          n = (uint32*)app->name;
		  packet_log(("rtcp_app: [%d]\n", *n));
		  if(*n == *((uint32*)name))
		  {
			  *appdata = app->data;
			  return app->len_data;
		  }
       }
    }

	return 0;
}

 /**********************************************************/

 /*********************************************************
  * Release Packet
  */
 int rtcp_done_packet(xrtp_rtcp_compound_t * com, xrtp_rtcp_head_t * head)
 {
    int i = 0;
    xrtp_rtcp_sr_t * sr = NULL;
    xrtp_rtcp_rr_t * rr = NULL;

    switch(head->type)
	{
       case(RTCP_TYPE_SR):

          sr = (xrtp_rtcp_sr_t *)head;

          for(i=0; i<sr->$head.count; i++)
             xfree(sr->reports[i]);

		  xfree(sr);

          break;

       case(RTCP_TYPE_RR):

          rr = (xrtp_rtcp_rr_t *)head;

          for(i=0; i<rr->$head.count; i++)
             xfree(rr->reports[i]);

		  xfree(rr);

          break;

       case(RTCP_TYPE_SDES):
          
			return rtcp_done_sdes((xrtp_rtcp_sdes_t *)head);
          
       case(RTCP_TYPE_BYE):
          
			return rtcp_done_bye((xrtp_rtcp_bye_t *)head);
          
       case(RTCP_TYPE_APP):

			return rtcp_done_app((xrtp_rtcp_app_t *)head);
          
       default:
          
			return XRTP_EUNSUP;
    }

    return XRTP_OK;
 }

 int rtcp_padding(xrtp_rtcp_compound_t * com){

    return com->first_head->padding;
 }

 uint32 rtcp_length(xrtp_rtcp_compound_t * com){

    int i;
    
    com->bytes = 0;
    
    for(i=0; i<com->n_packet; i++){

       com->bytes += com->heads[i]->bytes;
    }
    
    com->padding = com->bytes % 4 ? (4 - com->bytes % 4) : 0;
    com->bytes = rtcp_padding(com) ? com->bytes + com->padding : com->bytes;
    
    return com->bytes;
 }

 int rtcp_set_sender_info(xrtp_rtcp_compound_t * com, uint32 SSRC,
                          uint32 hi_ntp, uint32 lo_ntp, uint32 rtp_ts,
                          uint32 n_pac, uint32 n_oct){

    xrtp_rtcp_senderinfo_t * info;

    if(com->first_head->type != RTCP_TYPE_SR){

       return XRTP_EFORMAT;
    }
    
    info = &((xrtp_rtcp_sr_t *)(com->first_head))->$sender;

    info->SSRC = SSRC;
    info->hi_ntp = hi_ntp;
    info->lo_ntp = lo_ntp;
    info->rtp_stamp = rtp_ts;
    info->n_packet_sent = n_pac;
    info->n_octet_sent = n_oct;

    return XRTP_OK;
 }

 int rtcp_add_sr_report(xrtp_rtcp_sr_t *sr, uint32 SRC,
                         uint8 frac_lost, uint32 total_lost, uint32 full_seqn,
                         uint32 jitter, uint32 stamp_lsr, uint32 delay_lsr)
 {
    xrtp_rtcp_report_t * repo;
    
    if(sr->$head.count == XRTP_MAX_REPORTS)
	{
		packet_log(("rtcp_add_sr_report: Max reports, no more addition\n"));
		return XRTP_EFULL;
    }

    repo = (xrtp_rtcp_report_t *)xmalloc(sizeof(struct xrtp_rtcp_report_s));
    if(!repo)
	{
		packet_log(("rtcp_add_sr_report: No memory for SR report\n"));
		return XRTP_EMEM;
    }

    repo->SRC = SRC;
    repo->frac_lost = frac_lost;
    repo->total_lost = total_lost;
    repo->full_seqn = full_seqn;
    repo->jitter = jitter;
    repo->stamp_lsr = stamp_lsr;
    repo->delay_lsr = delay_lsr;

    sr->reports[sr->$head.count++] = repo;
    sr->$head.bytes += RTCP_REPORT_BYTE;

    packet_log(("rtcp_add_sr_report: n_report=%d, bytes=%dB\n", sr->$head.count, RTCP_REPORT_BYTE));

    return XRTP_OK;
 }

 int rtcp_add_rr_report(xrtp_rtcp_rr_t * rr, uint32 SRC,
                         uint8 frac_lost, uint32 total_lost, uint32 full_seqn,
                         uint32 jitter, uint32 stamp_lsr, uint32 delay_lsr)
 {
    xrtp_rtcp_report_t * repo;
    
    if(rr->$head.count == XRTP_MAX_REPORTS)
	{
		packet_log(("rtcp_add_rr_report: Max reports, no more addition\n"));
		return XRTP_EFULL;
    }

    repo = (xrtp_rtcp_report_t *)xmalloc(sizeof(struct xrtp_rtcp_report_s));
    if(!repo)
	{
		packet_log(("rtcp_add_rr_report: No memory for RR report\n"));
		return XRTP_EMEM;
    }

    repo->SRC = SRC;
    repo->frac_lost = frac_lost;
    repo->total_lost = total_lost;
    repo->full_seqn = full_seqn;
    repo->jitter = jitter;
    repo->stamp_lsr = stamp_lsr;
    repo->delay_lsr = delay_lsr;

    rr->reports[rr->$head.count++] = repo;
    rr->$head.bytes += RTCP_REPORT_BYTE;

    packet_log(("rtcp_add_rr_report: n_report=%d, bytes=%dB\n", rr->$head.count, RTCP_REPORT_BYTE));

    return XRTP_OK;
 }

 int rtcp_add_report(xrtp_rtcp_compound_t * com, uint32 SRC,
                     uint8 frac_lost, uint32 total_lost, uint32 full_seqn,
                     uint32 jitter, uint32 stamp_lsr, uint32 delay_lsr)
 {

    int ret;
    xrtp_rtcp_sr_t * sr;
    xrtp_rtcp_rr_t * rr;
    
    if((com->bytes + RTCP_REPORT_BYTE) > com->maxlen)
	{
		packet_log(("rtcp_add_report: Maximum %d bytes, no more addition\n", com->maxlen));
		return XRTP_EFULL;
    }

    switch(com->first_head->type)
	{
       case(RTCP_TYPE_SR):

          sr = (xrtp_rtcp_sr_t *)com->first_head;
          
          ret = rtcp_add_sr_report(sr, SRC, frac_lost, total_lost, full_seqn,
                                   jitter, stamp_lsr, delay_lsr);
          if(ret < XRTP_OK)
             return ret;
          
          break;

       case(RTCP_TYPE_RR):
       
          rr = (xrtp_rtcp_rr_t *)com->first_head;

          ret = rtcp_add_rr_report(rr, SRC, frac_lost, total_lost, full_seqn,
                                   jitter, stamp_lsr, delay_lsr);
          if(ret < XRTP_OK)
             return ret;

          break;

       default:

          return XRTP_EFORMAT;
    }

    com->first_head->length = com->first_head->bytes / 4 - 1; /* RFC 1889 */
    
    packet_log(("rtcp_add_report: RTCP raw packet is %d bytes\n", rtcp_length(com)));

    return XRTP_OK;
 }

 int rtcp_done_reports(xrtp_rtcp_compound_t * com)
 {
    int i = 0;
    xrtp_rtcp_sr_t * sr = NULL;
    xrtp_rtcp_rr_t * rr = NULL;

    switch(com->first_head->type){

       case(RTCP_TYPE_SR):

          sr = (xrtp_rtcp_sr_t *)com->first_head;

          for(i=0; i<sr->$head.count; i++){
             xfree(sr->reports[i]);
          }
          sr->$head.count = 0;

          break;

       case(RTCP_TYPE_RR):

          rr = (xrtp_rtcp_rr_t *)com->first_head;

          for(i=0; i<rr->$head.count; i++){
             xfree(rr->reports[i]);
          }
          rr->$head.count = 0;

          break;

       default:

          return XRTP_EFORMAT;
    }

    return XRTP_OK;
 }

int
rtcp_arrival_time(xrtp_rtcp_compound_t * rtcp, rtime_t *ms, rtime_t *us, rtime_t *ns)
{
	*ms = rtcp->msec;
	*us = rtcp->usec;
	*ns = rtcp->nsec;

	return XRTP_OK;
}
 
int rtcp_sender_info(xrtp_rtcp_compound_t * com, uint32 * r_SRC,
                     uint32 * r_hntp, uint32 * r_lntp, uint32 * r_ts,
                     uint32 * r_pnum, uint32 * r_onum)
{
    xrtp_rtcp_senderinfo_t * info;

    if(com->first_head->type != RTCP_TYPE_SR)
    {
        packet_debug(("rtcp_sender_info: Receiver Report\n"));

        return XRTP_EFORMAT;
    }

    info = &((xrtp_rtcp_sr_t *)(com->first_head))->$sender;

    *r_SRC = info->SSRC;
    *r_hntp = info->hi_ntp;
    *r_lntp = info->lo_ntp;
    *r_ts = info->rtp_stamp;
    *r_pnum = info->n_packet_sent;
    *r_onum = info->n_octet_sent;

    return XRTP_OK;
 }

 int rtcp_report(xrtp_rtcp_compound_t * com, uint32 ssrc,
                     uint8 * ret_flost, uint32 * ret_tlost,
                     uint32 * ret_seqn, uint32 * ret_jit,
                     uint32 * ret_ts, uint32 * ret_delay){
                     
    int i = 0;
    xrtp_rtcp_sr_t * sr;
    xrtp_rtcp_rr_t * rr;

    xrtp_rtcp_report_t *repo = NULL;
    
    switch(com->first_head->type)
    {
       case(RTCP_TYPE_SR):

          sr = (xrtp_rtcp_sr_t *)com->first_head;

          for(i=0; i<sr->$head.count; i++)
          {
             repo = sr->reports[i];
             
             if(repo->SRC == ssrc)
                break;
          }

          break;

       case(RTCP_TYPE_RR):

          rr = (xrtp_rtcp_rr_t *)com->first_head;

          for(i=0; i<rr->$head.count; i++)
          {
             repo = rr->reports[i];
             
             if(repo->SRC == ssrc)
                break;
          }

          break;

       default:

          return XRTP_EFORMAT;
    }

    if(!repo)
    {
       packet_debug(("rtcp_report: No report for ssrc[%u]\n", ssrc));
       return XRTP_INVALID;
    }

    *ret_flost = repo->frac_lost;
    *ret_tlost = repo->total_lost;
    *ret_seqn = repo->full_seqn;
    *ret_jit = repo->jitter;
    *ret_ts = repo->stamp_lsr;
    *ret_delay = repo->delay_lsr;

    return XRTP_OK;
 }

 /**
  * Release the rtcp compound packet
  */
 int rtcp_compound_done(xrtp_rtcp_compound_t * com)
 {
    int i;

    for(i=0; i<com->n_packet; i++)
    {
       rtcp_done_packet(com, com->heads[i]);
    }

    if(com->connect)
    {
        packet_debug(("rtcp_compound_done: xfree connect[@%d] in rtcp packet\n", (int)(com->connect)));
        connect_done(com->connect);
    }
        
    if(com->buffer)
        buffer_done(com->buffer);
    
	xfree(com);

    packet_log(("rtcp_compound_done: done\n"));
    
    return XRTP_OK;
 }

 //NEED TO THINK ABOUT PADDING ISSUE;

 void rtp_packet_show(xrtp_rtp_packet_t * rtp){

    int i = 0;
    printf("\n<RTP Packet No.=%d>\n", rtp->$head.seqno);
    printf("<Version = %d/>\n", rtp->$head.version);
    printf("<Padding = %d/>\n", rtp->$head.padding);
    printf("<Extension = %d/>\n", rtp->$head.ext);
    printf("<nCSRC = %d/>\n", rtp->$head.n_CSRC);
    printf("<Mark = %d/>\n", rtp->$head.mark);
    printf("<PT = %d/>\n", rtp->$head.pt);
    printf("<Timestamp = %d/>\n", rtp->$head.timestamp);
    printf("<SSRC src=%d/>\n", rtp->$head.SSRC);
    
    for(i=0; i<rtp->$head.n_CSRC; i++)
       printf("<CSRC n=%d src=%d>\n", i, rtp->$head.csrc_list[i]);

    if(rtp->headext){

       printf("<RTP Head Extension>\n");
       printf("<Info = %d/>\n", rtp->headext->info);
       printf("<length = %d/>\n", rtp->headext->len);
       printf("<data>");
       for(i=0; i<rtp->headext->len; i++)
          printf("[%d] ", ((uint8 *)rtp->headext->data)[i]);       
       printf("\n</data>\n");
       printf("</RTP Head Extension>\n");
    }
    
    printf("<RTP Payload>\n");
    for(i=0; i<rtp->$payload.len; i++)
       printf("[%d] ", ((uint8 *)rtp->$payload.data)[i]);
    printf("\n</RTP Payload>\n");
    printf("</RTP Packet>\n\n");

    return;
 }

 /**
  * Pack RTP Packet to byte stream for send, invoken by Profile Handler
  */
 xrtp_buffer_t* rtp_pack(xrtp_rtp_packet_t * rtp)
 {
    profile_handler_t * hand;
    xrtp_buffer_t * buf;

    uint16 w = 0;
    int i = 0;

    if(rtp->packet_bytes % 4)
    {
        rtp->$head.padding = 1;
        buf = buffer_new(rtp->packet_bytes + (4 - rtp->packet_bytes % 4), NET_ORDER);
    }
    else
    {
        rtp->$head.padding = 0;
        buf = buffer_new(rtp->packet_bytes, NET_ORDER);
    }
    
    if(!buf)
    {
        packet_log(("rtp_pack: Fail to allocate buffer memery\n"));
        return NULL;
    }

    packet_log(("rtp_pack: rtp packet length is %d\n", rtp->packet_bytes));
    packet_log(("rtp_pack: buffer size is %d\n", buffer_maxlen(buf)));


    /* Pack RTP Fixed Head */    
    packet_log(("rtp_pack: Packing RTP head\n"));

    w = (rtp->$head.version << 14) | (rtp->$head.padding << 13) | (rtp->$head.ext << 12) | (rtp->$head.n_CSRC << 8) | (rtp->$head.mark << 7) | rtp->$head.pt;
    buffer_add_uint16(buf, w);
    buffer_add_uint16(buf, rtp->$head.seqno);
    buffer_add_uint32(buf, rtp->$head.timestamp);
    buffer_add_uint32(buf, rtp->$head.SSRC);

    for(i=0; i<rtp->$head.n_CSRC; i++)
    {
       buffer_add_uint32(buf, rtp->$head.csrc_list[i]);
    }

    /* Pack RTP Head Extension */
    if(rtp->headext)
    {
       /* NOTE! The length is counting number of 4-BYTE */
       uint len;
       
       packet_log(("rtp_pack: Packing RTP head extension\n"));
    
       len = rtp->headext->len;
       len = (len % 4) ? (len / 4 + 1) : (len / 4);
       
       
       buffer_add_uint16(buf, rtp->headext->info);
       buffer_add_uint16(buf, rtp->headext->len_dw);
       
       hand = rtp->handler;
       if(hand && hand->pack_rtp_headext)
       {
          /* Profile specific method */
          packet_log(("rtp_pack: Call handler.pack_rtp_headext()\n"));
          hand->pack_rtp_headext(hand, rtp, rtp->session, rtp->headext->info, len);
       }
       else
       {
          /* Dummy method */
          packet_log(("rtp_pack: copy headext\n"));

          buffer_add_data(buf, rtp->headext->data, rtp->headext->len);
          
          for(i=0; i<rtp->headext->len_pad; i++){

             packet_log(("rtp_pack: padding headext\n"));

             buffer_add_uint8(buf, RTP_PADDING_VALUE);
          }
       }       
    }

    /* Pack RTP Payload */
    packet_log(("rtp_pack: Packing RTP Payload\n"));

    buffer_add_data(buf, rtp->$payload.data, rtp->$payload.len);
    rtp->$payload.out_buffer = NULL; /* no more use */

    /* Pad RTP packet */
    if(rtp->$head.padding)
    {
       int pad = 4 - rtp->packet_bytes % 4;
       
       for(i=0; i<pad-1; i++)
          buffer_add_uint8(buf, (char)RTP_PADDING_VALUE);

       buffer_add_uint8(buf, (char)pad);
       
       packet_log(("rtp_pack: %d padding byte RTP Payload\n", pad));
    }

    /* Cleaning house */
    if(rtp->buffer)
       buffer_done(rtp->buffer);
    
    rtp->buffer = buf;

    return buf;
 }

 /**
  * Unpack RTP Packet to memory for parsing, invoken by Profile Handler
  */
 int rtp_unpack(xrtp_rtp_packet_t * rtp)
 {

    uint16 w;
    uint32 dw;
    
    int ret = XRTP_OK;

    profile_handler_t * hand = NULL;
    xrtp_rtp_head_t * rtph = NULL;
    int prefix_bytes = 0;

    int i = 0;

    uint16 extinfo, extlen;

    if(!rtp->buffer)
       return XRTP_INVALID;
       
    hand = rtp->handler;
    
    rtp->direct = RTP_RECEIVE;
    rtp->packet_bytes = buffer_datalen(rtp->buffer);

    /* Unpack RTP Fixed head */
    if(buffer_next_uint16(rtp->buffer, &w) != sizeof(uint16)){ return XRTP_EFORMAT; }
    
    rtph = &rtp->$head;
    
    rtph->version = (w >> 14) & 0x3;   /* '11000000-00000000' */
    if(rtph->version != RTP_VERSION)
    {
       return XRTP_EFORMAT;
    }

    rtph->padding = (w >> 13) & 0x1;   /* '00100000-00000000' */
    
    rtph->ext = (w >> 12) & 0x1;       /* '00010000-00000000' */
    
    rtph->n_CSRC = (w >> 8) & 0x0f;    /* '00001111-00000000' */
    rtph->mark = (w >> 7) & 0x1;       /* '00000000-10000000' */

    
    /*
    packet_log(("rtp_unpack: version = %d\n", rtph->version));
    packet_log(("rtp_unpack: padding = %d\n", rtph->padding));
    packet_log(("rtp_unpack: ext = %d\n", rtph->ext));
    packet_log(("rtp_unpack: n_csrc = %d\n", rtph->n_CSRC));
    packet_log(("rtp_unpack: mark = %d\n", rtph->mark));
     */
     
    rtph->pt = (w & 0x7f);      /* '00000000-01111111' */
    if(rtph->pt >= RTCP_TYPE_SR)
    {        
       /* Check if a valid PT of the session 
       packet_log("rtp_unpack: invalid rtp pt = %d!\n", rtph->pt);
        */
       return XRTP_EFORMAT;
    }

    /*packet_log("rtp_unpack: pt = %d\n", rtph->pt);*/

    if(buffer_next_uint16(rtp->buffer, &w) != sizeof(uint16)){ return XRTP_EFORMAT; }
    rtph->seqno = w;

    if(buffer_next_uint32(rtp->buffer, &dw) != sizeof(uint32)){ return XRTP_EFORMAT; }
    rtph->timestamp = dw;
    
    if(buffer_next_uint32(rtp->buffer, &dw) != sizeof(uint32)){ return XRTP_EFORMAT; }
    rtph->SSRC = dw;
    
    prefix_bytes = RTP_HEAD_FIX_BYTE;

    /* parsing CSRC list */
    for(i=0; i<rtph->n_CSRC; i++)
    {
       if(buffer_next_uint32(rtp->buffer, &dw) != sizeof(uint32)){ return XRTP_EFORMAT; }
       rtph->csrc_list[i] = dw;
       prefix_bytes += RTP_CSRC_BYTE;
    }
    
    /* Parse RTP Head by handler */
    if(hand && hand->parse_rtp_head)
    {
       ret = hand->parse_rtp_head(hand, rtp, rtp->session);
       
       if(ret < XRTP_OK) return ret;
    }

    /* Unpack RTP Head Extension */
    if(rtph->ext)
    {
       if(buffer_next_uint16(rtp->buffer, &extinfo) != sizeof(uint16)){ return XRTP_EFORMAT; }
       if(buffer_next_uint16(rtp->buffer, &extlen) != sizeof(uint16)){ return XRTP_EFORMAT; }
       
       extlen *= 4; /* Change 4-byte back to byte count */
       rtp->headext = (xrtp_rtp_headext_t *)xmalloc(sizeof(struct xrtp_rtp_headext_s));
       if(!rtp->headext)
       {
          return XRTP_EMEM;
       }
       rtp->headext->info = extinfo;

       rtp->headext->len = extlen;
       prefix_bytes += extlen;
       
       rtp->headext->buf_pos = buffer_position(rtp->buffer);
       rtp->headext->data = buffer_address(rtp->buffer);
       
       if(buffer_skip(rtp->buffer, rtp->headext->len) != rtp->headext->len)
       {
           xfree(rtp->headext);
           rtp->headext = NULL;
           return XRTP_EFORMAT;
       }

       if(hand && hand->parse_rtp_headext)
       {
          ret = hand->parse_rtp_headext(hand, rtp, rtp->session);
          
          if(ret < XRTP_OK) return ret;
       }
    }

    /* Unpack RTP Payload, which handled by profile handler */
    rtp->$payload.buf_pos = buffer_position(rtp->buffer);
    rtp->$payload.data = buffer_address(rtp->buffer);
    rtp->$payload.len = rtp->packet_bytes - prefix_bytes;
    packet_log(("rtp_unpack: payload+padding, %d bytes\n", rtp->$payload.len));

    if(rtph->padding)
    {
        char b;
        
        buffer_skip(rtp->buffer, rtp->$payload.len-1);
        buffer_next_int8(rtp->buffer, &b);
        
        if(b <= RTP_MAX_PAD && b >= RTP_MIN_PAD)
            rtp->$payload.len -= b;
        else
            packet_log(("rtp_unpack: %d is illegal padding value\n", b));

        packet_log(("rtp_unpack: payload, %d bytes\n", rtp->$payload.len));
    }
    
    if(hand && hand->parse_rtp_payload)
    {
       ret = hand->parse_rtp_payload(hand, rtp, rtp->session);
    }

    return ret;
 }

 /**
  * Display rtcp SR packet
  */
 void rtcp_show_sr(xrtp_rtcp_sr_t * sr){

    int i;
    xrtp_rtcp_report_t * rep;

    printf("<packet type=sr bytes=%d>\n", sr->$head.bytes);
    printf("<version = %d/>\n", sr->$head.version);
    printf("<padding = %d/>\n", sr->$head.padding);
    printf("<num_report = %d/>\n", sr->$head.count);
    printf("<type = %d/>\n", sr->$head.type);    
    printf("<length = %d/>\n\n", sr->$head.length);
    
    printf("<senderinfo SSRC = %d/>\n", sr->$sender.SSRC);
    printf("<hi_ntp = %d/>\n", sr->$sender.hi_ntp);
    printf("<lo_ntp = %d/>\n", sr->$sender.lo_ntp);
    printf("<rtp_stamp = %d/>\n", sr->$sender.rtp_stamp);
    printf("<num_packet_sent = %d/>\n", sr->$sender.n_packet_sent);
    printf("<num_octet_sent = %d/>\n", sr->$sender.n_octet_sent);
    printf("</senderinfo>\n\n");

    for(i=0; i<sr->$head.count; i++){

       rep = sr->reports[i];
       printf("<report SRC = %d>\n", rep->SRC);
       printf("<frac_lost = %d/>\n", rep->frac_lost);
       printf("<total_lost = %d/>\n", rep->total_lost);
       printf("<full_seqno = %d/>\n", rep->full_seqn);
       printf("<jitter = %d/>\n", rep->jitter);
       printf("<stamp_lsr = %d/>\n", rep->stamp_lsr);
       printf("<delay_lsr = %d/>\n", rep->delay_lsr);
       printf("</report>\n\n");       
    }

    if(sr->ext){

       printf("<report_ext length = %d>\n", sr->ext->len);
    
       for(i=0; i<sr->ext->len; i++)
          printf("%d ", sr->ext->data[i]);
          
       printf("\n");
       printf("</report_ext>\n");
    }
        
    printf("</packet>\n\n");    
 }
 
 /**
  * Display rtcp RR packet
  */
 void rtcp_show_rr(xrtp_rtcp_rr_t * rr){

    int i;
    xrtp_rtcp_report_t * rep;

    printf("<packet type=rr bytes=%d>\n", rr->$head.bytes);
    printf("<version = %d/>\n", rr->$head.version);
    printf("<padding = %d/>\n", rr->$head.padding);
    printf("<num_report = %d/>\n", rr->$head.count);
    printf("<type = %d/>\n", rr->$head.type);
    printf("<length = %d/>\n\n", rr->$head.length);

    printf("<SSRC src=%d/>\n\n", rr->SSRC);

    for(i=0; i<rr->$head.count; i++){

       rep = rr->reports[i];
       printf("<report SRC = %d>\n", rep->SRC);
       printf("<frac_lost = %d/>\n", rep->frac_lost);
       printf("<total_lost = %d/>\n", rep->total_lost);
       printf("<full_seqno = %d/>\n", rep->full_seqn);
       printf("<jitter = %d/>\n", rep->jitter);
       printf("<stamp_lsr = %d/>\n", rep->stamp_lsr);
       printf("<delay_lsr = %d/>\n", rep->delay_lsr);
       printf("</report>\n\n");
    }

    if(rr->ext){

       printf("<report_ext length = %d>\n", rr->ext->len);
       for(i=0; i<rr->ext->len; i++)
          printf("%d ", rr->ext->data[i]);
          
       printf("\n");
       printf("</report_ext>\n");
    }

    printf("</packet>\n\n");
 }
 
 /**
  * Display rtcp SDES Packet
  */
 void rtcp_show_sdes(xrtp_rtcp_sdes_t * sdes){

    int i, j, k;
    xrtp_rtcp_sdes_chunk_t *chk = NULL;
    xrtp_rtcp_sdes_item_t *item = NULL;

    printf("<packet type=sdes bytes=%d>\n", sdes->$head.bytes);
    printf("<version = %d/>\n", sdes->$head.version);
    printf("<padding = %d/>\n", sdes->$head.padding);
    printf("<num_src = %d/>\n", sdes->$head.count);
    printf("<type = %d/>\n", sdes->$head.type);
    printf("<length = %d/>\n\n", sdes->$head.length);

    for(i=0; i<sdes->$head.count; i++){

       chk = sdes->chunks[i];
       printf("<chunk SRC=%d>\n", chk->SRC);
       for(j=0; j<chk->n_item; j++){

          item = chk->items[j];
          if(item->id == RTCP_SDES_PRIV){

             printf("<item id=%d>\n", item->id);
             printf("<prefix length=%d>\n", item->$priv.len_prefix);
             for(k=0; k<item->$priv.len_prefix; k++){
                printf("%d ", item->$priv.prefix[k]);
             }
             printf("\n</prefix>\n");
             printf("<value length=%d>\n", item->$priv.len_value);
             for(k=0; k<item->$priv.len_value; k++){
                printf("%d ", item->$priv.value[k]);
             }
             printf("\n</value>\n");
             printf("</item>\n");
             
          }else{
             
             printf("<item id=%d>\n", item->id);
             printf("<value length=%d>", item->len);
             for(k=0; k<item->len; k++){
                printf("%d ", item->value[k]);
             }
             printf("\n</value>\n");
             printf("</item>\n");
          }

       }
       printf("</chunk>\n\n");
    }
    printf("</packet>\n\n");
 }
 
 /**

  * Display rtcp BYE Packet
  */
 void rtcp_show_bye(xrtp_rtcp_bye_t * bye){

    int i = 0;

    printf("<packet type=bye bytes=%d>\n", bye->$head.bytes);
    printf("<version = %d/>\n", bye->$head.version);
    printf("<padding = %d/>\n", bye->$head.padding);
    printf("<num_src = %d/>\n", bye->$head.count);
    printf("<type = %d/>\n", bye->$head.type);
    printf("<length = %d/>\n\n", bye->$head.length);

    for(i=0; i<bye->$head.count; i++){

       printf("<to src=%d/>\n", bye->srcs[i]);
    }
    printf("<why>");
    for(i=0; i<bye->len_why; i++)
       printf("%d ", bye->why[i]);
       
    printf("</why>\n");
    printf("</packet>\n\n");
 }
 
 /**
  * Display rtcp APP Packet
  */
 void rtcp_show_app(xrtp_rtcp_app_t * app){

    int i;

    printf("<packet type=app SSRC=%d bytes=%d>\n", app->SSRC, app->$head.bytes);
    printf("<version = %d/>\n", app->$head.version);
    printf("<padding = %d/>\n", app->$head.padding);
    printf("<num_src = %d/>\n", app->$head.count);
    printf("<type = %d/>\n", app->$head.type);
    printf("<length = %d/>\n\n", app->$head.length);

    printf("<name = %d/>\n", (uint)app->name);
    printf("<data>");

    for(i=0; i<app->len_data; i++)
       printf("%d ", app->data[i]);
    printf("</data>\n");
    printf("</packet>\n\n");
 }

 /**
  * Display rtcp compound
  */
 void rtcp_show(xrtp_rtcp_compound_t * rtcp){

    int i = 0;

    printf("<rtcp packets=%d bytes=%d>\n", rtcp->n_packet, rtcp_length(rtcp));
    
	if(rtcp->first_head->type == RTCP_TYPE_SR)       
       rtcp_show_sr((xrtp_rtcp_sr_t *)rtcp->first_head);
       
    if(rtcp->first_head->type == RTCP_TYPE_RR)       
       rtcp_show_rr((xrtp_rtcp_rr_t *)rtcp->first_head);

    for(i=1; i<rtcp->n_packet; i++){

       switch(rtcp->heads[i]->type){

          case(RTCP_TYPE_SDES):
             rtcp_show_sdes((xrtp_rtcp_sdes_t *)rtcp->heads[i]);
             break;
             
          case(RTCP_TYPE_BYE):
             rtcp_show_bye((xrtp_rtcp_bye_t *)rtcp->heads[i]);
             break;
             
          case(RTCP_TYPE_APP):
             rtcp_show_app((xrtp_rtcp_app_t *)rtcp->heads[i]);
             break;
             
          default:
             printf("<unknown/>\n");
       }
    }
    
    printf("</rtcp>\n");
 }
 
 /**
  * RTCP packing methods:
  *
  * Pack RTCP Compound to byte stream for send, invoken by Profile Handler
  */
 int _rtcp_pack_report(xrtp_rtcp_report_t * rep, xrtp_buffer_t * buf){

    uint32 w = 0;
    
    buffer_add_uint32(buf, rep->SRC);
    
    w = rep->frac_lost << 24 | rep->total_lost;
    buffer_add_uint32(buf, w);
    
    buffer_add_uint32(buf, rep->full_seqn);
    buffer_add_uint32(buf, rep->jitter);
    buffer_add_uint32(buf, rep->stamp_lsr);
    buffer_add_uint32(buf, rep->delay_lsr);

    return XRTP_OK;
 }
 
 int rtcp_pack_head(xrtp_rtcp_head_t * head, xrtp_buffer_t * buf)
 {
    uint16 w = 0;

    /* Pack RTCP Fixed Head */

    w = (head->version << 14) | (head->padding << 13) | (head->count << 8) | head->type;
    buffer_add_uint16(buf, w);
    head->length = head->bytes / 4 - 1;
    buffer_add_uint16(buf, head->length);

    return XRTP_OK;
 }

 int rtcp_pack_sdes(xrtp_rtcp_sdes_t * sdes, xrtp_buffer_t * buf)
 {

    int ret = rtcp_pack_head(&sdes->$head, buf);
        
    xrtp_rtcp_sdes_chunk_t * chk = NULL;
    xrtp_rtcp_sdes_item_t *item = NULL;
    
    int i, j;
    for(i=0; i<sdes->$head.count; i++)
	{
       /* Pack SDES Chunk */
       chk = sdes->chunks[i];
       buffer_add_uint32(buf, chk->SRC);
       
       for(j=0; j<chk->n_item; j++)
	   {
          /* Pack SDES Item */
          item = chk->items[j];
          
          buffer_add_uint8(buf, item->id);
          buffer_add_uint8(buf, item->len);
          switch(item->id)
		  {
             case(RTCP_SDES_PRIV):
                buffer_add_uint8(buf, item->$priv.len_prefix);
                buffer_add_data(buf, item->$priv.prefix, item->$priv.len_prefix);
                buffer_add_data(buf, item->$priv.value, item->$priv.len_value);
                break;
                
             default:
                buffer_add_data(buf, item->value, item->len);
          }
       }
       
       /* check padding */
       buffer_fill_value(buf, 0, chk->padlen);
       
       packet_log(("_rtcp_pack_sdes: padding %d byte in chunk\n", chk->padlen));
    }
    
    return ret;
 }
    
int rtcp_pack_bye(xrtp_rtcp_bye_t * bye, xrtp_buffer_t * buf)
{

    int ret = rtcp_pack_head(&bye->$head, buf);
    
    int i;
    for(i=0; i<bye->$head.count; i++)
	{
       buffer_add_uint32(buf, bye->srcs[i]);
    }

    buffer_add_uint8(buf, bye->len_why);
    buffer_add_data(buf, bye->why, bye->len_why);

    return ret;
}
 
int rtcp_pack_app(xrtp_rtcp_app_t *app, xrtp_buffer_t * buf)
{
    int ret = rtcp_pack_head(&app->$head, buf);

    buffer_add_uint32(buf, app->SSRC);

    buffer_add_uint8(buf, app->name[0]);
    buffer_add_uint8(buf, app->name[1]);
    buffer_add_uint8(buf, app->name[2]);
    buffer_add_uint8(buf, app->name[3]);

    buffer_add_data(buf, app->data, app->len_data);
	if(app->$head.padding)
	{
		char npad = 4 - app->len_data % 4;
		buffer_fill_value(buf, npad, npad);
	}
    
    return ret;
}
 
xrtp_buffer_t * rtcp_pack(xrtp_rtcp_compound_t * com)
{
    int ret;
    int i = 0;

    xrtp_rtcp_head_t * rtcphead = com->first_head;
    
    uint32 rtcp_bytes = rtcp_length(com);

    xrtp_buffer_t * buf = buffer_new(rtcp_bytes, NET_ORDER);
    if(!buf)
       return NULL;

    //uint16 w = 0;
    
    /* Pack RTCP Fixed Head */
    ret = rtcp_pack_head(rtcphead, buf);
        
    /* Pack Report Packet */
    if(rtcphead->type == RTCP_TYPE_SR)
	{
       /* Pack Senderinfo Block */
       xrtp_rtcp_sr_t * sr = (xrtp_rtcp_sr_t *)rtcphead;
       
       buffer_add_uint32(buf, sr->$sender.SSRC);

       buffer_add_uint32(buf, sr->$sender.hi_ntp);
       buffer_add_uint32(buf, sr->$sender.lo_ntp);
       buffer_add_uint32(buf, sr->$sender.rtp_stamp);
       buffer_add_uint32(buf, sr->$sender.n_packet_sent);
       buffer_add_uint32(buf, sr->$sender.n_octet_sent);

       /* Pack SR Report Blocks */
       for(i=0; i<sr->$head.count; i++)
          _rtcp_pack_report(sr->reports[i], buf);

       /* Pack SR Extension Block */
       if(sr->ext)
	   {
          buffer_add_data(buf, sr->ext->data, sr->ext->len);
       }
    }
	else
	{  /* rtcphead->type == RTCP_TYPE_SR */
       xrtp_rtcp_rr_t * rr = (xrtp_rtcp_rr_t *)rtcphead;
       
       buffer_add_uint32(buf, rr->SSRC);
       
       /* Pack RR Report Blocks */
       for(i=0; i<rr->$head.count; i++)
          _rtcp_pack_report(rr->reports[i], buf);
          
       /* Pack RR Extension Block */
       if(rr->ext)
	   {
          buffer_add_data(buf, rr->ext->data, rr->ext->len);
       }
    }

    /* Pack Rest Packets */
    for(i=1; i<com->n_packet; i++)
	{
       switch(com->heads[i]->type)
	   {
          case(RTCP_TYPE_SDES):
             ret = rtcp_pack_sdes((xrtp_rtcp_sdes_t *)com->heads[i], buf);
             break;
          case(RTCP_TYPE_BYE):
             ret = rtcp_pack_bye((xrtp_rtcp_bye_t *)com->heads[i], buf);
             break;
          case(RTCP_TYPE_APP):
             ret = rtcp_pack_app((xrtp_rtcp_app_t *)com->heads[i], buf);
             break;
       }
    }

    com->buffer = buf;
    
    return buf;
}

 /*
  * End of RTCP packing methods
  ***********************************************************************/
  
 /**
  * Unpack RTCP Compound to memory for parsing, invoken by Profile Handler
  */
 int rtcp_parse_head(xrtp_rtcp_compound_t * rtcp, xrtp_rtcp_head_t * head, uint8 * r_ver, uint8 * r_padding, uint8 * r_count, uint8 * r_type, uint16 * r_len)
 {
    uint16 w;

    xrtp_buffer_t * buf = rtcp->buffer;

    /* Unpack RTP Fixed head */
    if(buffer_next_uint16(buf, &w) != sizeof(uint16)){ return XRTP_EFORMAT; }

    *r_ver = (w >> 14) & 0x3;       /* '00000011' */
    *r_padding = (w >> 13) & 0x1;   /* '00000001' */
    *r_count = (w >> 8) & 0x1F;     /* '00011111' */
    *r_type = w & 0xFF;             /* '11111111' */

    if(buffer_next_uint16(buf, r_len) != sizeof(uint16)){ return XRTP_EFORMAT; }

    packet_log(("rtcp_parse_head: %d 32bit RTCP #%d Packet found\n", *r_len, *r_type));

    return XRTP_OK;
 }
    
 xrtp_rtcp_sdes_item_t * rtcp_sdes_make_item(uint8 id, uint8 vlen, char *value)
 {
    xrtp_rtcp_sdes_item_t *item = (xrtp_rtcp_sdes_item_t *)xmalloc(sizeof(struct xrtp_rtcp_sdes_item_s));
    if(item)
    {
       memset(item, 0, sizeof(*item));

       item->id = id;
       item->len = vlen;
       memcpy(item->value, value, vlen);
    }

    return item;
 }

 xrtp_rtcp_sdes_item_t * rtcp_sdes_make_priv_item(uint8 vlen, uint8 prelen, char *prefix, uint8 privlen, char *prival)
 {
    xrtp_rtcp_sdes_item_t *item = (xrtp_rtcp_sdes_item_t *)xmalloc(sizeof(struct xrtp_rtcp_sdes_item_s));
    if(item)
    {
       memset(item, 0, sizeof(*item));

       item->id = RTCP_SDES_PRIV;
       item->len = vlen;
       
       item->$priv.len_prefix = prelen;
       memcpy(item->$priv.prefix, prefix, prelen);
       
       item->$priv.len_value = privlen;
       memcpy(item->$priv.value, prival, privlen);
    }

    return item;
 }

 xrtp_rtcp_sdes_t * rtcp_make_sdes(uint8 ver, uint8 pad, uint8 count, uint16 len){

    xrtp_rtcp_sdes_t *sdes = (xrtp_rtcp_sdes_t *)xmalloc(sizeof(struct xrtp_rtcp_sdes_s));
    if(sdes)
    {
       memset(sdes, 0, sizeof(*sdes));
       
       sdes->$head.version = ver;
       sdes->$head.padding = pad;
       sdes->$head.count = 0;  /* update late */
       sdes->$head.type = RTCP_TYPE_SDES;
       sdes->$head.length = len;
       sdes->$head.bytes = RTCP_HEAD_FIX_BYTE;
    }

    return sdes;
 }

 int rtcp_sdes_load_item(xrtp_rtcp_sdes_t *sdes, uint32 ssrc, xrtp_rtcp_sdes_item_t *item){

    int i;
    int chk_exist = 0;
    
    for(i=0; i<sdes->$head.count; i++){

       if(sdes->chunks[i]->SRC == ssrc){

          chk_exist = 1;
          
          /* As the function usually for rebuilding from byte stream
           * Assume no same id items will came at same time */
          sdes->chunks[i]->items[sdes->chunks[i]->n_item] = item;
          packet_log(("_rtcp_sdes_add_item: chunk(%d).items[%d]\n", sdes->chunks[i]->SRC, sdes->chunks[i]->n_item));
          sdes->chunks[i]->n_item++;
       }
    }

    if(!chk_exist)
    {
       xrtp_rtcp_sdes_chunk_t *chunk = (xrtp_rtcp_sdes_chunk_t *)xmalloc(sizeof(struct xrtp_rtcp_sdes_chunk_s));
       if(!chunk)
       {
          return XRTP_EMEM;
       }

       memset(chunk, 0, sizeof(*chunk));

       chunk->SRC = ssrc;
       chunk->items[0] = item;
       chunk->n_item = 1;
       
       sdes->chunks[sdes->$head.count++] = chunk;
       
       packet_log(("_rtcp_sdes_add_item: new %dth chunk(%d).items[0]\n", sdes->$head.count, ssrc));
    }

    return XRTP_OK;
 }

 /**
  * Unpack RTCP SDES packet
  */
 int rtcp_unpack_sdes(xrtp_rtcp_compound_t * rtcp, uint8 ver, uint8 pad, uint8 count, uint16 len)
 {
    int ret = XRTP_OK;
    uint32 ssrc;
    uint8 id;
    uint chk_start, chk_pad;
    
    xrtp_rtcp_sdes_item_t *item = NULL;
        
    int i = 0;
    int end = 0;
    xrtp_buffer_t * buf = rtcp->buffer;

    xrtp_rtcp_sdes_t *sdes = rtcp_make_sdes(ver, pad, count, len);
    if(!sdes)
       return XRTP_EMEM;

    for(i=0; i<count; i++)
    {
       chk_start = buffer_position(buf);
       
       /* Unpack a chunk */
       if(buffer_next_uint32(buf, &ssrc) != sizeof(uint32))
       {
          ret=XRTP_EFORMAT;
          goto error;
       }
       
       while(!end)
       {
          /* Unpack an item */
          if(buffer_next_uint8(buf, &id) != sizeof(uint8))
          {
             ret=XRTP_EFORMAT;
             goto error;
          }
          
          if(id == RTCP_SDES_END)
          {
             uint pos = buffer_position(buf);
             
             end = 1;
             chk_pad = 4 - (pos - chk_start) % 4;
             sdes->$head.bytes += chk_pad + pos - chk_start;
             
             if(pos % 4)
                buffer_seek(buf, buffer_position(buf) + chk_pad);
                
             break;
          }
          else
          {
             /* id != RTCP_SDES_PRIV */
             char val[XRTP_MAX_SDES_BYTES];
             uint8 vlen;

             if(buffer_next_uint8(buf, &vlen) != sizeof(uint8))
             {
                ret=XRTP_EFORMAT;
                goto error;
             }

             if(id != RTCP_SDES_PRIV)
             {
                if(buffer_next_data(buf, val, vlen) != vlen)
                {
                   ret=XRTP_EFORMAT;
                   goto error;
                }
                   
                item = rtcp_sdes_make_item(id, vlen, val);
             }
             else
             {
                uint8 prelen;

                char *prefix;
                uint8 privlen;
                char *prival;

                if(buffer_next_uint8(buf, &prelen) != sizeof(uint8))
                {
                   ret=XRTP_EFORMAT;
                   goto error;
                }
             
                prefix = val;
             
                if(buffer_next_data(buf, prefix, prelen) != prelen)
                {
                   ret=XRTP_EFORMAT;
                   goto error;
                }


                privlen = vlen - prelen - sizeof(uint8);
                prival = val + prelen;
             
                if(buffer_next_data(buf, prival, privlen) != privlen)
                {
                   ret=XRTP_EFORMAT;
                   goto error;
                }

                item = rtcp_sdes_make_priv_item(vlen, prelen, prefix, privlen, prival);
             }
          }
          
          ret = rtcp_sdes_load_item(sdes, ssrc, item);
          if(ret < XRTP_OK)
             goto error;
       }
    }

    rtcp->heads[rtcp->n_packet++] = (xrtp_rtcp_head_t *)sdes;
    packet_log(("_rtcp_unpack_sdes: %dth packet\n", rtcp->n_packet));

    return ret;

    error:
    if(sdes->$head.count == 0)
       rtcp_done_sdes(sdes);

    return ret;
 }

 /**
  * Unpack RTCP BYE packet
  */
 int _rtcp_unpack_bye(xrtp_rtcp_compound_t * rtcp, uint8 ver, uint8 padding, uint8 count, uint16 length){
    
    int ret = XRTP_OK;
    uint pos_start;
    
    int i;
    xrtp_buffer_t * buf = rtcp->buffer;
    uint32 srcs[XRTP_MAX_RTCP_COUNT];
    
    uint8 wlen = 0;
    char * why = NULL;
	int len_body = 0;
    xrtp_rtcp_bye_t * bye = NULL;
    
    pos_start = buffer_position(buf);  /* exlude the bye head */
    
    /* No such thing, Wrong!
     * uint32 SSRC;
     * if(buffer_next_uint32(buf, &SSRC) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
     */
      
    for(i=0; i<count; i++){

       if(buffer_next_uint32(buf, &srcs[i]) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
    }

    len_body = buffer_position(buf) - pos_start;
    if(len_body < length*4){

       if(buffer_next_uint8(buf, &wlen) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       
       why = (char *)xmalloc(sizeof(char)*wlen);
       if(!why) return XRTP_EMEM;

       ret = buffer_next_data(buf, why, wlen);
       if(ret < XRTP_OK){

          xfree(why);
          return ret;
       }
    }

    bye = rtcp_new_bye(rtcp, srcs, count, wlen, why);
    if(!bye){

       if(why) xfree(why);
       return XRTP_FAIL;
    }
    packet_log(("_rtcp_unpack_bye: Add %dth packet(BYE)\n", rtcp->n_packet));

    return ret;
 }

 /**
  * Unpack RTCP APP packet
  */
 int rtcp_unpack_app(xrtp_rtcp_compound_t * rtcp, uint8 ver, uint8 padding, uint8 count, uint16 length)
 {
    int ret = XRTP_OK;
    
    uint32 SSRC;
	char name[4];
	int dlen = 0;
	//int dpos;
	uint8 npad = 0;
    xrtp_buffer_t *buf = rtcp->buffer;
	char *data = NULL;
	xrtp_rtcp_app_t *app = NULL;


    
    if(buffer_next_uint32(buf, &SSRC) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
    if(buffer_next_uint8(buf, &name[0]) != sizeof(uint8)){ ret=XRTP_EFORMAT; }
    if(buffer_next_uint8(buf, &name[1]) != sizeof(uint8)){ ret=XRTP_EFORMAT; }
    if(buffer_next_uint8(buf, &name[2]) != sizeof(uint8)){ ret=XRTP_EFORMAT; }
    if(buffer_next_uint8(buf, &name[3]) != sizeof(uint8)){ ret=XRTP_EFORMAT; }
    
	//dpos = buffer_position(buf);
	data = buffer_address(buf);

    dlen = (length * 4) - 8;
    if(!padding)
	{
		buffer_skip(buf, dlen);
	}
	else
	{
		buffer_skip(buf, dlen-1);
		if(buffer_next_uint8(buf, &npad) != sizeof(uint8)){ ret=XRTP_EFORMAT; }
		//buffer_seek(buf, dpos);

		if(npad > 0 && npad < 4)
		{
			dlen -= npad;
			packet_log(("rtcp_unpack_app: trim %d bytes padding\n", npad));
		}
		else
		{ 
			ret=XRTP_EFORMAT; 
		}
	}

	if(ret < XRTP_OK)
	{
		buffer_skip(buf, dlen);
		return ret;
	}

	/*
    data = (char*)xmalloc(sizeof(char) * dlen);
    if(!data)
	{
		packet_log(("rtcp_unpack_app: No memory for APP data\n"));
		return XRTP_EMEM;
	}

	if(buffer_next_data(buf, data, dlen) != dlen){ ret=XRTP_EFORMAT; }
	*/
    
    app = rtcp_new_app(rtcp, SSRC, count, name, dlen, data);
    if(!app)
	{
       //xfree(data);
       return XRTP_FAIL;
    }

    packet_log(("rtcp_unpack_app: Add %dth packet(APP)\n", rtcp->n_packet));

    return ret;
 }
 
 /**
  * Unpack RTCP Compound to memory for parsing, invoken by Profile Handler
  */
 int rtcp_unpack(xrtp_rtcp_compound_t * rtcp){

    int ret = XRTP_OK;
    uint pos_start;

    profile_handler_t *hand = rtcp->handler;

    uint8 ver, padding, count, type;
    uint16 length;
    
    int i;

    uint32 dw, SRC, tlost, fseqn, jitter, lsrstamp, lsrdelay;
    uint8 flost;

    int len_pac = 0;

    uint len_unscaned = 0;

    if(!rtcp->buffer)
       return XRTP_INVALID;

    pos_start = buffer_position(rtcp->buffer); /* Record the start point */
    
    /* Unpack RTCP Fixed head */
    ret = rtcp_parse_head(rtcp, rtcp->first_head, &ver, &padding, &count, &type, &length);
    if(ret < XRTP_OK)
		return ret;

    /* Unpack RTCP first Packet, MUST be only SR or RR */
    if(type == RTCP_TYPE_SR)
    {
       uint32 ssrc, hi_ntp, lo_ntp, rtp_stamp, npac, noct;
       xrtp_rtcp_sr_t * sr = NULL;

       /* Create Sender Report */
       sr = rtcp_make_sr_packet(ver, padding, 0, length);
       if(!sr)
          return XRTP_FAIL;
       
       /* Parsing Senderinfo Block */

       if(buffer_next_uint32(rtcp->buffer, &ssrc) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &hi_ntp) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &lo_ntp) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &rtp_stamp) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &npac) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &noct) != sizeof(uint32)){ ret=XRTP_EFORMAT; }

       sr->$sender.SSRC = ssrc;
       sr->$sender.hi_ntp = hi_ntp;
       sr->$sender.lo_ntp = lo_ntp;
       sr->$sender.rtp_stamp = rtp_stamp;
       sr->$sender.n_packet_sent = npac;
       sr->$sender.n_octet_sent = noct;
       
       rtcp->heads[0] = rtcp->first_head = (xrtp_rtcp_head_t *)sr;
       
    }
	else if(type == RTCP_TYPE_RR)
	{
       uint32 SSRC;
       xrtp_rtcp_rr_t * rr = NULL;
       
       /* Unpack Receiver Report */
       if(buffer_next_uint32(rtcp->buffer, &SSRC) != sizeof(uint32)){ ret=XRTP_EFORMAT; }

       /* Create Sender Report */
       rr = rtcp_make_rr_packet(ver, padding, 0, length);
       if(!rr)
          return XRTP_EMEM;
       
       rr->SSRC = SSRC;

       rtcp->heads[0] = rtcp->first_head = (xrtp_rtcp_head_t *)rr;
    }
	else
	{
       return XRTP_EFORMAT;
    }

    rtcp->n_packet = 1;
    
    /* Unpack Report Block */
    packet_log(("rtcp_unpack: %d reports in total\n", count));
    for(i=0; i<count; i++)
	{
       if(buffer_next_uint32(rtcp->buffer, &SRC) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &dw) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       flost = (dw >> 24) & 0x0f;
       tlost = dw & 0x0fff;
       if(buffer_next_uint32(rtcp->buffer, &fseqn) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &jitter) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &lsrstamp) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &lsrdelay) != sizeof(uint32)){ ret=XRTP_EFORMAT; }

       ret = rtcp_add_report(rtcp, SRC, flost, tlost, fseqn, jitter, lsrstamp, lsrdelay);
       if(ret < XRTP_OK) return ret;
    }

    /* Unpack Report Extension by Profile Handler */
    len_pac = buffer_position(rtcp->buffer) - pos_start;
    if(len_pac < (length+1)*4)
	{
       if(hand && hand->parse_rtcp_ext)
	   {
          ret = hand->parse_rtcp_ext(hand, rtcp, rtcp->session);
          if(ret < XRTP_OK) return ret;
       }
	   else
	   {
          buffer_seek(rtcp->buffer, pos_start + (length+1) * 4); /* See RFC-1889 */
       }
    }

    /* Unpack following RTCP Packet */
    len_unscaned = buffer_datalen(rtcp->buffer) - buffer_position(rtcp->buffer);
    
    i = 1;     /* scan for head#1 or forewhile */    
    while(len_unscaned > sizeof(uint32)) /* More packet head to scan */
	{
       pos_start = buffer_position(rtcp->buffer);
       
       ret = rtcp_parse_head(rtcp, rtcp->heads[i], &ver, &padding, &count, &type, &length);
       if(ret < XRTP_OK) return ret;
       
       switch(type)
	   {
          case(RTCP_TYPE_SDES):
             packet_log(("rtcp_unpack: unpack sdes\n"));
             ret = rtcp_unpack_sdes(rtcp, ver, padding, count, length);
             if(ret < XRTP_OK) return ret;
             break;
             
          case(RTCP_TYPE_BYE):
             packet_log(("rtcp_unpack: unpack bye\n"));
             ret = _rtcp_unpack_bye(rtcp, ver, padding, count, length);
             if(ret < XRTP_OK) return ret;
             break;
             
          case(RTCP_TYPE_APP):
             packet_log(("rtcp_unpack: unpack app\n"));
             ret = rtcp_unpack_app(rtcp, ver, padding, count, length);
             if(ret < XRTP_OK)
			 {
				packet_log(("rtcp_unpack: skip APP packet\n"));
			 }
             break;
             
          default:
             packet_log(("rtcp_unpack: skip %d bytes UNKNOWN packet\n", (length+1) * 4));
             buffer_seek(rtcp->buffer, pos_start + (length+1) * 4); /* Skip unsupported packet */
       }
       
       i++;
       len_unscaned = buffer_datalen(rtcp->buffer) - buffer_position(rtcp->buffer);
    }
    
    return ret;
 }

