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
 #include <timedia/xstring.h>
 
 #include "stdio.h"

#ifdef PACKET_LOG
 #define packet_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define packet_log(fmtargs)
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
 xrtp_rtp_packet_t * rtp_new_packet(xrtp_session_t * ses, int pt, enum xrtp_direct_e dir, session_connect_t * conn, xrtp_lrtime_t lrt_arrival, xrtp_hrtime_t hrt_arrival){

    xrtp_rtp_packet_t * rtp = (xrtp_rtp_packet_t *)malloc(sizeof(struct xrtp_rtp_packet_s));
    if(!rtp){
       
       packet_log(("< rtp_new_packet: Can't allocate resource >\n"));
       return NULL;
    }

    memset(rtp, 0, sizeof(struct xrtp_rtp_packet_s));
    
    rtp->is_sync = 0;               /* for sync rtp only */
    rtp->hi_ntp = rtp->lo_ntp = 0;
    
    rtp->session = ses;
    rtp->handler = NULL;
    rtp->valid_to_get = 0;
    rtp->local_mapped = 0; /* if the local time is calc'd */

    if(dir == RTP_RECEIVE){
      
        rtp->lrt_arrival = lrt_arrival;
        rtp->hrt_arrival = hrt_arrival;
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
 int rtp_packet_set_buffer(xrtp_rtp_packet_t * pac, xrtp_buffer_t * buf){

    if(pac->buffer){

       buffer_done(pac->buffer);
    }

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
 int rtp_packet_done_headext(xrtp_rtp_packet_t * pac, xrtp_rtp_headext_t * hext){

    if(pac->direct == RTP_SEND){

       packet_log(("_rtp_packet_done_headext: free data as its seperated from packet in sending\n"));
       free(hext->data);
    }
       
    free(hext);
    pac->headext = NULL;
    
    packet_log(("rtp_packet_done_headext: headext freed\n"));

    return XRTP_OK;
 }

/**
 * Release payload data
 */
int rtp_packet_done_payload(xrtp_rtp_packet_t * pac, xrtp_rtp_payload_t * pay){

    if(pac->direct == RTP_SEND){

        packet_log(("rtp_packet_done_payload: free payload as its seperated from packet in sending\n"));
        free(pac->$payload.data);
    }
       
    pac->$payload.data = NULL;
    pac->$payload.len = 0;
    packet_log(("rtp_packet_done_payload: payload data freed\n"));

    return XRTP_OK;
}

 /**
  * See packet.h
  *
  * FIXME: if the data is const value, cause segment faulty!
  */
 int rtp_packet_done(xrtp_rtp_packet_t * pac){

    if(pac->headext)
       rtp_packet_done_headext(pac, pac->headext);
    
    if(pac->$payload.len != 0)
       rtp_packet_done_payload(pac, &(pac->$payload));
    
    if(pac->connect)
        connect_done(pac->connect);
        
    if(pac->buffer)
        buffer_done(pac->buffer);
    
    free(pac);
    packet_log(("rtp_packet_done(): done packet\n"));
    
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

 uint rtp_packet_length(xrtp_rtp_packet_t * pac){

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
 xrtp_rtp_head_t * rtp_packet_set_head(xrtp_rtp_packet_t * pac, uint32 ssrc,
                                        uint16 seqno, uint32 ts){

    pac->$head.SSRC = ssrc;
    pac->$head.seqno = seqno;
    pac->$head.timestamp = ts;

    packet_log(("rtp_packet_set_head: ssrc=%u, seqno=%u, timestamp=%u\n", ssrc, seqno, ts));

    return &(pac->$head);
 }

 uint32 rtp_packet_ssrc(xrtp_rtp_packet_t * rtp){

    return rtp->$head.SSRC;
 }
 
 /**
  * See packet.h
  */
 xrtp_rtp_headext_t * _rtp_packet_new_headext(xrtp_rtp_packet_t * pac, uint16 info, uint16 len, char * xdat){

    xrtp_rtp_headext_t * hx = (xrtp_rtp_headext_t *)malloc(sizeof(struct xrtp_rtp_headext_s));
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

    xrtp_rtp_headext_t * hx = (xrtp_rtp_headext_t *)malloc(sizeof(struct xrtp_rtp_headext_s));
    if(!hx){

       packet_log(("rtp_packet_new_headext: Can't allocate resource\n"));

       return XRTP_EMEM;
    }

    if(pac->headext)
       free(pac->headext);

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
 int rtp_packet_set_payload(xrtp_rtp_packet_t *rtp, buffer_t *payload_buf){

    rtp->$payload.out_buffer = payload_buf;

    if(payload_buf == NULL)
	{
		rtp->$payload.data = NULL;
		rtp->$payload.len = 0;
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
    pac->$payload.data = malloc(len);
    
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
 uint rtp_packet_payload(xrtp_rtp_packet_t * pac, char **ret_payload){

    if(pac->$payload.len == 0){

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
      
        *r_payload = malloc(rtp->$payload.len);
        if(!*r_payload){

            *r_payload = NULL;
            return 0;
        }

        memcpy(*r_payload, rtp->$payload.data, len);
    }
    
    rtp->$payload.data = NULL;
    rtp->$payload.len = 0;
    
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

 /* --------------------- RTCP Compound interface ------------------------ */

 /**
  * Create a empty RTCP compound packet for recreate the rtcp structure.
  *
  * param in: max number of packets allowed
  */
 xrtp_rtcp_compound_t * rtcp_new_compound(xrtp_session_t * ses, uint npack, enum xrtp_direct_e dir, session_connect_t * conn, xrtp_hrtime_t hrts_arrival, xrtp_lrtime_t lrts_arrival){

    xrtp_rtcp_compound_t * comp = (xrtp_rtcp_compound_t *)malloc(sizeof(struct xrtp_rtcp_compound_s));
    if(!comp){
       return NULL;
    }
    
    memset(comp, 0, sizeof(struct xrtp_rtcp_compound_s));

    comp->session = ses;
    
    if(dir == RTP_RECEIVE)
        comp->hrt_arrival = hrts_arrival;
    else
        comp->hrt_arrival = HRTIME_INFINITY;

    comp->lrt_arrival = lrts_arrival;

    comp->connect = conn;

    comp->direct = dir;

    comp->valid_to_get = 0;
    comp->first_head = NULL;

    if(!npack) npack = XRTP_MAX_RTCP_PACKETS;
    
    comp->heads = (xrtp_rtcp_head_t **)malloc(sizeof(xrtp_rtcp_head_t *) * npack);
    if(!comp->heads){

       packet_log(("rtcp_new_compound: Can't allocate resource for packet list\n"));

       free(comp);
       return NULL;
    }
    
    comp->max_packets = npack;
    comp->bytes = 0;

    comp->maxlen = XRTP_RTCP_DEFAULTMAXBYTES;
    comp->buffer = NULL;

    return comp;
 }
 
 /**
  * The incoming data buffer for rtcp compound
  */
 int rtcp_set_buffer(xrtp_rtcp_compound_t * rtcp, xrtp_buffer_t * buf){

    if(rtcp->buffer)
       buffer_done(rtcp->buffer);

    rtcp->buffer = buf;

    return XRTP_OK;
 }

 xrtp_buffer_t * rtcp_buffer(xrtp_rtcp_compound_t * rtcp){

    return rtcp->buffer;
 }

 xrtp_rtcp_sr_t * _rtcp_new_sr_packet(uint8 ver, uint8 pad, uint8 count, uint16 len){

    xrtp_rtcp_sr_t * sr = (xrtp_rtcp_sr_t *)malloc(sizeof(struct xrtp_rtcp_sr_s));
    if(!sr){
       return NULL;
    }
    sr->$head.version = ver;
    sr->$head.padding = pad;
    sr->$head.count = count;
    sr->$head.type = RTCP_TYPE_SR;
    sr->$head.length = len;

    return sr;
 }
 
 xrtp_rtcp_rr_t * _rtcp_new_rr_packet(uint8 ver, uint8 pad, uint8 count, uint16 len){

    xrtp_rtcp_rr_t * rr = (xrtp_rtcp_rr_t *)malloc(sizeof(struct xrtp_rtcp_rr_s));
    if(!rr){
       return NULL;
    }
    rr->$head.version = ver;
    rr->$head.padding = pad;
    rr->$head.count = count;
    rr->$head.type = RTCP_TYPE_RR;
    rr->$head.length = len;

    return rr;
 }

 /**
  * See packet.h
  */
 xrtp_rtcp_compound_t * rtcp_sender_report_new(xrtp_session_t * ses, int size, int padding){

    xrtp_rtcp_sr_t * sr;

    xrtp_rtcp_compound_t * rtcp = (xrtp_rtcp_compound_t *)malloc(sizeof(struct xrtp_rtcp_compound_s));
    if(!rtcp){
       
       packet_log(("rtcp_sender_report_new: Can't allocate resource\n"));
       
       return NULL;
    }
    
    memset(rtcp, 0, sizeof(struct xrtp_rtcp_compound_s));
    
    if(size == 0) size = XRTP_MAX_RTCP_PACKETS;
    
    rtcp->heads = (xrtp_rtcp_head_t **)malloc(sizeof(xrtp_rtcp_head_t *) * size);
    if(!rtcp->heads){

       packet_log(("rtcp_sender_report_new: Can't allocate resource for packet list\n"));

       free(rtcp);
       return NULL;
    }    
    rtcp->max_packets = size;

    rtcp->session = ses;
    rtcp->handler = NULL;
    
    rtcp->valid_to_get = 0;
    rtcp->padding = padding;
    rtcp->direct = RTP_SEND;

    sr = (xrtp_rtcp_sr_t *)malloc(sizeof(struct xrtp_rtcp_sr_s));
    if(!sr){

       packet_log(("rtcp_sender_report_new: Can't allocate resource for sender report packet\n"));

       free(rtcp->heads);
       free(rtcp);
       return NULL;
    }

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
 xrtp_rtcp_compound_t * rtcp_receiver_report_new(xrtp_session_t * ses, uint32 SSRC, int size, int padding){
    
    xrtp_rtcp_rr_t * rr;
    
    xrtp_rtcp_compound_t * rtcp = (xrtp_rtcp_compound_t *)malloc(sizeof(struct xrtp_rtcp_compound_s));
    if(!rtcp){

       packet_log(("rtcp_reveiver_report_new: Can't allocate resource\n"));

       return NULL;
    }

    memset(rtcp, 0, sizeof(struct xrtp_rtcp_compound_s));

    if(size == 0) size = XRTP_MAX_RTCP_PACKETS;

    rtcp->heads = (xrtp_rtcp_head_t **)malloc(sizeof(xrtp_rtcp_head_t *) * size);
    if(!rtcp->heads){

       packet_log(("rtcp_receiver_report_new: Can't allocate resource for packet list\n"));

       free(rtcp);
       return NULL;
    }
    
    rtcp->max_packets = size;

    rtcp->session = ses;
    rtcp->handler = NULL;
    
    rtcp->valid_to_get = 0;
    rtcp->padding = padding;
    rtcp->direct = RTP_SEND;

    rr = (xrtp_rtcp_rr_t *)malloc(sizeof(struct xrtp_rtcp_rr_s));
    if(!rr){

       packet_log(("rtcp_receiver_report_new: Can't allocate resource for sender report packet\n"));

       free(rtcp->heads);
       free(rtcp);
       return NULL;
    }
    
    rr->$head.count = 0;
    rr->$head.version = 2;
    rr->$head.padding = 0;
    rr->$head.type = RTCP_TYPE_RR;

    rr->$head.bytes = RTCP_HEAD_FIX_BYTE + RTCP_SSRC_BYTE;
    rr->$head.length = rr->$head.bytes / 4 - 1 ;

    rr->SSRC = SSRC;

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
 xrtp_rtcp_sdes_t * rtcp_new_sdes(xrtp_rtcp_compound_t * com){

    xrtp_rtcp_sdes_t * sdes;
    
    if(com->n_packet == com->max_packets){

       packet_log(("rtcp_new_sdes: Maximum packet reached\n"));

       return NULL;
    }
    
    sdes = (xrtp_rtcp_sdes_t *)malloc(sizeof(struct xrtp_rtcp_sdes_s));
    if(!sdes){

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
    
    packet_log(("rtcp_new_sdes: SDES head = %db\n", RTCP_HEAD_FIX_BYTE));

    return sdes;
 }
 
 /***** Releasing SDES Packet.Chunk.Item *****/
 
 int _rtcp_done_sdes_item(xrtp_rtcp_sdes_item_t * item){

    free(item);
    
    packet_log(("_rtcp_done_sdes_item: item freed\n"));

    return XRTP_OK;
 }
 
 /***** Releasing SDES Packet.Chunk *****/
 
 int _rtcp_done_sdes_chunk(xrtp_rtcp_sdes_chunk_t * chunk){

    int i;
    for(i=0; i<chunk->n_item; i++){
       _rtcp_done_sdes_item(chunk->items[i]);
    }
    free(chunk);
    
    packet_log(("_rtcp_done_sdes_chunk: chunk freed\n"));

    return XRTP_OK;
 }
 
 /***** Releasing SDES Packet *****/
 
 int _rtcp_done_sdes(xrtp_rtcp_sdes_t * sdes){

    int i;
    for(i=0; i<sdes->$head.count; i++)
       _rtcp_done_sdes_chunk(sdes->chunks[i]);

    free(sdes);

    packet_log(("_rtcp_done_sdes: sdes freed\n"));

    return XRTP_OK;
 }
 
 /***** Add SDES Item to Packet *****/
 
 int rtcp_add_sdes(xrtp_rtcp_compound_t * com, uint32 SRC, uint8 type, uint8 len, char * v){

    int i;
    xrtp_rtcp_sdes_t * sdes = NULL;
    xrtp_rtcp_sdes_chunk_t * chunk = NULL;
    xrtp_rtcp_sdes_item_t *item = NULL;

    int new_sdes = 0, new_chunk = 0, new_item = 0;

    int _addlen = 0;
    int _room = com->maxlen - com->bytes;

    packet_log(("rtcp_add_sdes: %db free\n", _room));

    for(i=0; i<com->n_packet; i++){

       if(com->heads[i]->type == RTCP_TYPE_SDES){

          sdes = (xrtp_rtcp_sdes_t *)com->heads[i];
          
          packet_log(("rtcp_add_sdes: Found SDES packet\n"));

          break;
       }
    }

    if(!sdes){

       _room -= (RTCP_HEAD_FIX_BYTE + RTCP_SRC_BYTE + RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
       if(_room < len){

          return XRTP_EREFUSE;
       }

       new_sdes = new_chunk = new_item = 1;

    }else{

       chunk = NULL;

       for(i=0; i<sdes->$head.count; i++){

          if(sdes->chunks[i]->SRC == SRC){

             chunk = sdes->chunks[i];
             
             packet_log(("rtcp_add_sdes: Found Chunk for SRC(%d)\n", SRC));

             break;
          }
       }
       
       if(!chunk){

          if(sdes->$head.count == XRTP_MAX_SDES_CHUNK){

             packet_log(("rtcp_add_sdes(): No more chunk then %d\n", XRTP_MAX_SDES_CHUNK));

             return XRTP_EREFUSE;
          }

          _room -= (RTCP_SRC_BYTE + RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
          if(_room < len){

             packet_log(("rtcp_add_sdes: SDES Chunk needs more then %d bytes!\n", _room));

             return XRTP_EREFUSE;
          }

          new_chunk = new_item = 1;

       }else{

          item = NULL;
          
          if(chunk->ended) return XRTP_EREFUSE;

          for(i=0; i<chunk->n_item; i++){

             if(chunk->items[i]->id == type){

                item = chunk->items[i];
                
                packet_log(("rtcp_add_sdes: Found item type %d\n", type));

                break;
             }
          }

          if(!item){

             _room -= (RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
             if(_room < len){

                packet_log(("rtcp_add_sdes: SDES Item needs more then %d bytes!\n", _room));

                return XRTP_EREFUSE;
             }

             new_item = 1;

          }else{

             packet_log(("rtcp_add_sdes: Replace old item\n"));

             _addlen = len - item->len;

             item->len = len;
             memcpy(item->value, v, len);

             com->bytes += _addlen;

             return XRTP_OK;
          }
       }
    }

    if(new_chunk){

       chunk = (xrtp_rtcp_sdes_chunk_t *)malloc(sizeof(struct xrtp_rtcp_sdes_chunk_s));
       if(!chunk){
          if(new_sdes)
             _rtcp_done_sdes(sdes);
       }
       
       memset(chunk, 0, sizeof(struct xrtp_rtcp_sdes_chunk_s));

       chunk->SRC = SRC;

       _addlen += RTCP_SRC_BYTE;
       
       packet_log(("rtcp_add_sdes: New chunk(%d)=%db\n", SRC, RTCP_SRC_BYTE));
    }

    if(new_item){

       packet_log(("rtcp_add_sdes: may crash, get to malloc %d bytes memery\n", sizeof(struct xrtp_rtcp_sdes_item_s)));
       item = (xrtp_rtcp_sdes_item_t *)malloc(sizeof(struct xrtp_rtcp_sdes_item_s));
       packet_log(("rtcp_add_sdes: malloc succeed\n"));
       if(!item){

          packet_log(("rtcp_add_sdes: Can't allocate Item resource\n"));

          if(new_chunk) free(chunk);

          return XRTP_EMEM;
       }

       memset(item, 0, sizeof(struct xrtp_rtcp_sdes_item_s));

       item->id = type;
       item->len = len;
       memcpy(item->value, v, len);

       chunk->items[chunk->n_item++] = item;

       _addlen += RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE + len;
       
       packet_log(("rtcp_add_sdes: New item=%db; chunk(%d)=%db; rtcp+%db\n",  RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE + len, chunk->SRC, RTCP_SRC_BYTE, _addlen));
    }

    if(!new_sdes && new_chunk)
       sdes->chunks[sdes->$head.count++] = chunk;
       
    if(new_sdes){

       sdes = rtcp_new_sdes(com); /* Create a SDES and add to compound */
       if(!sdes){

          if(new_chunk) free(chunk);

          if(new_item){

             free(item);
          }

          return XRTP_EREFUSE;
       }

       sdes->chunks[sdes->$head.count++] = chunk;
    }
    
    sdes->$head.bytes += _addlen;
    
    packet_log(("rtcp_add_sdes: SDES = %dB\n", sdes->$head.bytes));

    return _addlen;
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

    for(i=0; i<com->n_packet; i++){

       if(com->heads[i]->type == RTCP_TYPE_SDES){

          sdes = (xrtp_rtcp_sdes_t *)com->heads[i];

          packet_log(("rtcp_add_priv_sdes: Found SDES packet\n"));

          break;
       }
    }

    if(!sdes){

       _room -= (RTCP_HEAD_FIX_BYTE + RTCP_SRC_BYTE + RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
       if(_room < len){

          return XRTP_EREFUSE;
       }

       packet_log(("rtcp_add_priv_sdes: Need a new SDES Packet\n"));

       new_sdes = new_chunk = new_item = 1;

    }else{

       chunk = NULL;

       for(i=0; i<sdes->$head.count; i++){

          if(sdes->chunks[i]->SRC == SRC){

             chunk = sdes->chunks[i];

             packet_log(("rtcp_add_priv_sdes: Found Chunk for SRC(%d)\n", SRC));

             break;
          }
       }

       if(!chunk){

          if(sdes->$head.count == XRTP_MAX_SDES_CHUNK){

             packet_log(("rtcp_add_priv_sdes: No more chunk then %d\n", XRTP_MAX_SDES_CHUNK));

             return XRTP_EREFUSE;
          }

          _room -= (RTCP_SRC_BYTE + RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
          if(_room < len){

             packet_log(("rtcp_add_priv_sdes: SDES Chunk needs more then %d bytes!\n", _room));

             return XRTP_EREFUSE;
          }

          packet_log(("rtcp_add_priv_sdes: Need a new SRC(%d) Chunk\n", SRC));

          new_chunk = new_item = 1;

       }else{

          item = NULL;

          if(chunk->ended) return XRTP_EREFUSE;

          for(i=0; i<chunk->n_item; i++){

             if(chunk->items[i]->id == RTCP_SDES_PRIV){

                item = chunk->items[i];

                packet_log(("rtcp_add_priv_sdes: Found private sdes item\n"));

                break;
             }
          }

          if(!item){

             _room -= (RTCP_SDES_ID_BYTE + RTCP_SDES_LEN_BYTE);
             if(_room < len){

                packet_log(("rtcp_add_priv_sdes: SDES Item needs more then %d bytes!\n", _room));

                return XRTP_EREFUSE;
             }

             packet_log(("rtcp_add_priv_sdes: Need a new private SDES item\n"));

             new_item = 1;

          }else{

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

    if(new_chunk){

       chunk = (xrtp_rtcp_sdes_chunk_t *)malloc(sizeof(struct xrtp_rtcp_sdes_chunk_s));
       if(!chunk){
          if(new_sdes)
             _rtcp_done_sdes(sdes);
       }

       chunk->SRC = SRC;
       chunk->n_item = 0;
       chunk->ended = 0;

       _addlen += RTCP_SRC_BYTE;
       
       packet_log(("rtcp_add_priv_sdes: New Chunk(%d) = %dB\n", SRC, RTCP_SRC_BYTE));
    }

    if(new_item){

       item = (xrtp_rtcp_sdes_item_t *)malloc(sizeof(struct xrtp_rtcp_sdes_item_s));
       if(!item){

          packet_log(("rtcp_add_priv_sdes: Can't allocate Item resource\n"));

          if(new_chunk) free(chunk);

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

    if(new_sdes){

       sdes = rtcp_new_sdes(com); /* Create a SDES and add to compound */
       if(!sdes){

          if(new_chunk) free(chunk);

          if(new_item){

             free(item->value);
             free(item);
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

 uint8 rtcp_sdes(xrtp_rtcp_compound_t * com, uint32 SRC, uint8 type, char **r_sdes){

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

    bye = (xrtp_rtcp_bye_t *)malloc(sizeof(struct xrtp_rtcp_bye_s));
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
 
 int _rtcp_done_bye(xrtp_rtcp_bye_t * bye){

    free(bye->why);
    free(bye);

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
 xrtp_rtcp_app_t * rtcp_new_app(xrtp_rtcp_compound_t * com, uint32 SSRC, uint8 subtype, uint32 name, uint len, char * appdata){
    
    xrtp_rtcp_app_t * app;

    if(com->n_packet == com->max_packets){

       packet_log(("rtcp_new_app: Maximum packet reached\n"));

       return NULL;
    }

    if(len % 4){

       packet_log(("rtcp_new_app: Data len MUST mutiple of 32bits\n"));

       return NULL;
    }

    app = (xrtp_rtcp_app_t *)malloc(sizeof(struct xrtp_rtcp_app_s));
    if(!app){

       packet_log(("rtcp_new_app: Can't allocate for BYE packet\n"));

       return NULL;
    }

    app->$head.count = subtype;
    app->$head.version = 2;
    app->$head.padding = 0;
    app->$head.type = RTCP_TYPE_APP;
    app->$head.bytes = RTCP_HEAD_FIX_BYTE + RTCP_SRC_BYTE + RTCP_APP_NAME_BYTE + len;
    app->$head.length = app->$head.bytes / 4 - 1;   /* RFC 1889 */
    
    app->SSRC = SSRC;
    app->name = name;
    app->len_data = len;
    app->data = appdata;

    com->heads[com->n_packet++] = (xrtp_rtcp_head_t *)app;

    packet_log(("rtcp_new_app: RTCP is %d bytes\n", rtcp_length(com)));

    return app;
 }
 
 int _rtcp_done_app(xrtp_rtcp_app_t * app){

    free(app->data);
    free(app);

    return XRTP_OK;
 }

 /**********************************************************/

 /*********************************************************
  * Release Packet
  */
 int _rtcp_done_packet(xrtp_rtcp_compound_t * com, xrtp_rtcp_head_t * head){

    switch(head->type){

       case(RTCP_TYPE_SDES):
          return _rtcp_done_sdes((xrtp_rtcp_sdes_t *)head);
          
       case(RTCP_TYPE_BYE):
          return _rtcp_done_bye((xrtp_rtcp_bye_t *)head);
          
       case(RTCP_TYPE_APP):
          return _rtcp_done_app((xrtp_rtcp_app_t *)head);
          
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

 int _rtcp_sr_add_report(xrtp_rtcp_sr_t * sr, uint32 SRC,
                         uint8 frac_lost, uint32 total_lost, uint32 full_seqn,
                         uint32 jitter, uint32 stamp_lsr, uint32 delay_lsr){

    xrtp_rtcp_report_t * repo;
    
    if(sr->$head.count == XRTP_MAX_REPORTS){

       return XRTP_EFULL;
    }

    repo = (xrtp_rtcp_report_t *)malloc(sizeof(struct xrtp_rtcp_report_s));
    if(!repo){

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

    packet_log(("_rtcp_sr_add_report: n_report=%d, bytes=%dB\n", sr->$head.count, RTCP_REPORT_BYTE));

    return XRTP_OK;
 }

 int _rtcp_rr_add_report(xrtp_rtcp_rr_t * rr, uint32 SRC,
                         uint8 frac_lost, uint32 total_lost, uint32 full_seqn,
                         uint32 jitter, uint32 stamp_lsr, uint32 delay_lsr){

    xrtp_rtcp_report_t * repo;
    
    packet_log(("_rtcp_rr_add_report: Add RR Report\n"));

    if(rr->$head.count == XRTP_MAX_REPORTS){

       return XRTP_EFULL;
    }

    repo = (xrtp_rtcp_report_t *)malloc(sizeof(struct xrtp_rtcp_report_s));
    if(!repo){

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

    return XRTP_OK;
 }

 int rtcp_add_report(xrtp_rtcp_compound_t * com, uint32 SRC,
                     uint8 frac_lost, uint32 total_lost, uint32 full_seqn,
                     uint32 jitter, uint32 stamp_lsr, uint32 delay_lsr){

    int ret;
    xrtp_rtcp_sr_t * sr;
    xrtp_rtcp_rr_t * rr;
    
    if((com->bytes + RTCP_REPORT_BYTE) > com->maxlen){

       return XRTP_EFULL;
    }

    switch(com->first_head->type){

       case(RTCP_TYPE_SR):

          sr = (xrtp_rtcp_sr_t *)com->first_head;
          
          ret = _rtcp_sr_add_report(sr, SRC, frac_lost, total_lost, full_seqn,
                                   jitter, stamp_lsr, delay_lsr);
          if(ret < XRTP_OK){

             return ret;
          }
          
          break;

       case(RTCP_TYPE_RR):
       
          rr = (xrtp_rtcp_rr_t *)com->first_head;

          ret = _rtcp_rr_add_report(rr, SRC, frac_lost, total_lost, full_seqn,
                                   jitter, stamp_lsr, delay_lsr);
          if(ret < XRTP_OK){

             return ret;
          }

          break;

       default:

          return XRTP_EFORMAT;
    }

    com->first_head->length = com->first_head->bytes / 4 - 1; /* RFC 1889 */
    
    packet_log(("rtcp_add_report: RTCP raw packet is %d bytes\n", rtcp_length(com)));

    return XRTP_OK;
 }

 int rtcp_done_all_reports(xrtp_rtcp_compound_t * com){

    int i = 0;
    xrtp_rtcp_sr_t * sr = NULL;
    xrtp_rtcp_rr_t * rr = NULL;

    switch(com->first_head->type){

       case(RTCP_TYPE_SR):

          sr = (xrtp_rtcp_sr_t *)com->first_head;

          for(i=0; i<sr->$head.count; i++){
             free(sr->reports[i]);
          }
          sr->$head.count = 0;

          break;

       case(RTCP_TYPE_RR):

          rr = (xrtp_rtcp_rr_t *)com->first_head;

          for(i=0; i<rr->$head.count; i++){
             free(rr->reports[i]);
          }
          rr->$head.count = 0;

          break;

       default:

          return XRTP_EFORMAT;
    }

    return XRTP_OK;
 }

 int rtcp_sender_info(xrtp_rtcp_compound_t * com, uint32 * r_SRC,
                      uint32 * r_hntp, uint32 * r_lntp, uint32 * r_ts,
                      uint32 * r_pnum, uint32 * r_onum){

    xrtp_rtcp_senderinfo_t * info;

    if(com->first_head->type != RTCP_TYPE_SR){

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

 int rtcp_report(xrtp_rtcp_compound_t * com, uint32 SRC,
                     uint8 * ret_flost, uint32 * ret_tlost,
                     uint32 * ret_seqn, uint32 * ret_jit,
                     uint32 * ret_ts, uint32 * ret_delay){
                     
    int i = 0;
    xrtp_rtcp_sr_t * sr;
    xrtp_rtcp_rr_t * rr;

    xrtp_rtcp_report_t * repo = NULL;
    
    switch(com->first_head->type){

       case(RTCP_TYPE_SR):

          sr = (xrtp_rtcp_sr_t *)com->first_head;

          for(i=0; i<sr->$head.count; i++){
             
             if(sr->reports[i]->SRC == SRC){

                repo = sr->reports[i];
                break;
             }
          }

          break;

       case(RTCP_TYPE_RR):

          rr = (xrtp_rtcp_rr_t *)com->first_head;

          for(i=0; i<rr->$head.count; i++){

                repo = rr->reports[i];
                break;
          }

          break;

       default:

          return XRTP_EFORMAT;
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
 int rtcp_compound_done(xrtp_rtcp_compound_t * com){

    int i;
    for(i=0; i<com->n_packet; i++){

       _rtcp_done_packet(com, com->heads[i]);
    }

    if(com->connect){
      
        packet_log(("rtcp_compound_done: free connect[@%d] in rtcp packet\n", (int)(com->connect)));
        connect_done(com->connect);
    }
        
    if(com->buffer){
      
        packet_log(("rtcp_compound_done: buffer freed\n"));
        free(com->buffer);
    }
    
    free(com);

    packet_log(("rtcp_compound_done: freed\n"));
    
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
 xrtp_buffer_t* rtp_pack(xrtp_rtp_packet_t * rtp){

    profile_handler_t * hand;
    xrtp_buffer_t * buf;

    uint16 w = 0;
    int i = 0;

    if(rtp->packet_bytes % 4){
      
        rtp->$head.padding = 1;
        buf = buffer_new(rtp->packet_bytes + (4 - rtp->packet_bytes % 4), NET_ORDER);

    }else{
      
        rtp->$head.padding = 0;
        buf = buffer_new(rtp->packet_bytes, NET_ORDER);
    }
    
    if(!buf){

        packet_log(("< rtp_pack: Fail to allocate buffer memery >\n"));
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

    for(i=0; i<rtp->$head.n_CSRC; i++){

       buffer_add_uint32(buf, rtp->$head.csrc_list[i]);
    }

    /* Pack RTP Head Extension */
    if(rtp->headext){

       /* NOTE! The length is counting number of 4-BYTE */
       uint len;
       
       packet_log(("rtp_pack: Packing RTP head extension\n"));
    
       len = rtp->headext->len;
       len = (len % 4) ? (len / 4 + 1) : (len / 4);
       
       
       buffer_add_uint16(buf, rtp->headext->info);
       buffer_add_uint16(buf, rtp->headext->len_dw);
       
       hand = rtp->handler;
       if(hand && hand->pack_rtp_headext){
          
          /* Profile specific method */
          packet_log(("rtp_pack: Call handler.pack_rtp_headext()\n"));
          hand->pack_rtp_headext(hand, rtp, rtp->session, rtp->headext->info, len);

       }else{
          
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
    if(rtp->$head.padding){

       int pad = 4 - rtp->packet_bytes % 4;
       
       for(i=0; i<pad-1; i++)
          buffer_add_uint8(buf, (char)RTP_PADDING_VALUE);

       buffer_add_uint8(buf, (char)pad);
       
       packet_log(("rtp_pack: %d padding byte RTP Payload\n", pad));
    }

    /* Cleaning house */
    if(rtp->buffer){

       buffer_done(rtp->buffer);
    }
    
    rtp->buffer = buf;

    return buf;
 }

 /**
  * Unpack RTP Packet to memory for parsing, invoken by Profile Handler
  */
 int rtp_unpack(xrtp_rtp_packet_t * rtp){

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
    if(rtph->version != RTP_VERSION){
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
    if(rtph->pt >= RTCP_TYPE_SR){        

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
    for(i=0; i<rtph->n_CSRC; i++){

       if(buffer_next_uint32(rtp->buffer, &dw) != sizeof(uint32)){ return XRTP_EFORMAT; }
       rtph->csrc_list[i] = dw;
       prefix_bytes += RTP_CSRC_BYTE;
    }
    
    /* Parse RTP Head by handler */
    if(hand && hand->parse_rtp_head){
       
       ret = hand->parse_rtp_head(hand, rtp, rtp->session);
       
       if(ret < XRTP_OK) return ret;
    }

    /* Unpack RTP Head Extension */
    if(rtph->ext){

       if(buffer_next_uint16(rtp->buffer, &extinfo) != sizeof(uint16)){ return XRTP_EFORMAT; }
       if(buffer_next_uint16(rtp->buffer, &extlen) != sizeof(uint16)){ return XRTP_EFORMAT; }
       
       extlen *= 4; /* Change 4-byte back to byte count */
       rtp->headext = (xrtp_rtp_headext_t *)malloc(sizeof(struct xrtp_rtp_headext_s));
       if(!rtp->headext){

          return XRTP_EMEM;
       }
       rtp->headext->info = extinfo;
       rtp->headext->len = extlen;
       prefix_bytes += extlen;
       
       rtp->headext->buf_pos = buffer_position(rtp->buffer);
       rtp->headext->data = buffer_address(rtp->buffer);
       
       if(buffer_skip(rtp->buffer, rtp->headext->len) != rtp->headext->len){
          
           free(rtp->headext);
           rtp->headext = NULL;
           return XRTP_EFORMAT;
       }

       if(hand && hand->parse_rtp_headext){
          
          ret = hand->parse_rtp_headext(hand, rtp, rtp->session);
          
          if(ret < XRTP_OK) return ret;
       }
    }

    /* Unpack RTP Payload, which handled by profile handler */
    rtp->$payload.buf_pos = buffer_position(rtp->buffer);
    rtp->$payload.data = buffer_address(rtp->buffer);
    rtp->$payload.len = rtp->packet_bytes - prefix_bytes;
    packet_log(("rtp_unpack: payload + padding is %d bytes\n", rtp->$payload.len));

    if(rtph->padding){
      
        char b;
        
        buffer_skip(rtp->buffer, rtp->$payload.len-1);
        buffer_next_int8(rtp->buffer, &b);
        
        if(b <= RTP_MAX_PAD && b >= RTP_MIN_PAD)
            rtp->$payload.len -= b;
        else
            packet_log(("< rtp_unpack: %d is illegal padding value >\n", b));
    }
    
    if(hand && hand->parse_rtp_payload){
       
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

    printf("<name = %d/>\n", app->name);
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
 
 int _rtcp_pack_head(xrtp_rtcp_head_t * head, xrtp_buffer_t * buf){

    uint16 w = 0;

    /* Pack RTCP Fixed Head */

    w = (head->version << 14) | (head->padding << 13) | (head->count << 8) | head->type;
    buffer_add_uint16(buf, w);
    head->length = head->bytes / 4 - 1;
    buffer_add_uint16(buf, head->length);

    return XRTP_OK;
 }

 int _rtcp_pack_sdes(xrtp_rtcp_sdes_t * sdes, xrtp_buffer_t * buf){

    int ret = _rtcp_pack_head(&sdes->$head, buf);
        
    xrtp_rtcp_sdes_chunk_t * chk = NULL;
    xrtp_rtcp_sdes_item_t *item = NULL;
    
    int i, j;
    for(i=0; i<sdes->$head.count; i++){
       
       /* Pack SDES Chunk */
       chk = sdes->chunks[i];
       buffer_add_uint32(buf, chk->SRC);
       
       for(j=0; j<chk->n_item; j++){

          /* Pack SDES Item */
          item = chk->items[j];
          
          buffer_add_uint8(buf, item->id);
          buffer_add_uint8(buf, item->len);
          switch(item->id){

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
    
 int _rtcp_pack_bye(xrtp_rtcp_bye_t * bye, xrtp_buffer_t * buf){

    int ret = _rtcp_pack_head(&bye->$head, buf);
    
    int i;
    for(i=0; i<bye->$head.count; i++){
       
       buffer_add_uint32(buf, bye->srcs[i]);
    }

    buffer_add_uint8(buf, bye->len_why);
    buffer_add_data(buf, bye->why, bye->len_why);

    return ret;
 }
 
 int _rtcp_pack_app(xrtp_rtcp_app_t * app, xrtp_buffer_t * buf){

    int ret = _rtcp_pack_head(&app->$head, buf);

    buffer_add_uint32(buf, app->SSRC);
    buffer_add_uint32(buf, app->name);
    buffer_add_data(buf, app->data, app->len_data);
    
    return ret;
 }
 
 xrtp_buffer_t * rtcp_pack(xrtp_rtcp_compound_t * com){

    int ret;
    int i = 0;

    xrtp_rtcp_head_t * rtcphead = com->first_head;
    
    uint32 rtcp_bytes = rtcp_length(com);

    xrtp_buffer_t * buf = buffer_new(rtcp_bytes, NET_ORDER);
    if(!buf){

       return NULL;
    }

    //uint16 w = 0;
    
    /* Pack RTCP Fixed Head */
    ret = _rtcp_pack_head(rtcphead, buf);
        
    /* Pack Report Packet */
    if(rtcphead->type == RTCP_TYPE_SR){
       
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
       if(sr->ext){

          buffer_add_data(buf, sr->ext->data, sr->ext->len);
       }

    }else{  /* rtcphead->type == RTCP_TYPE_SR */

       xrtp_rtcp_rr_t * rr = (xrtp_rtcp_rr_t *)rtcphead;
       
       buffer_add_uint32(buf, rr->SSRC);
       
       /* Pack RR Report Blocks */
       for(i=0; i<rr->$head.count; i++)
          _rtcp_pack_report(rr->reports[i], buf);
          
       /* Pack RR Extension Block */
       if(rr->ext){

          buffer_add_data(buf, rr->ext->data, rr->ext->len);
       }
    }

    /* Pack Rest Packets */
    for(i=1; i<com->n_packet; i++){

       switch(com->heads[i]->type){

          case(RTCP_TYPE_SDES):
             ret = _rtcp_pack_sdes((xrtp_rtcp_sdes_t *)com->heads[i], buf);
             break;
          case(RTCP_TYPE_BYE):
             ret = _rtcp_pack_bye((xrtp_rtcp_bye_t *)com->heads[i], buf);
             break;
          case(RTCP_TYPE_APP):
             ret = _rtcp_pack_app((xrtp_rtcp_app_t *)com->heads[i], buf);
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
 int rtcp_parse_head(xrtp_rtcp_compound_t * rtcp, xrtp_rtcp_head_t * head, uint8 * r_ver, uint8 * r_padding, uint8 * r_count, uint8 * r_type, uint16 * r_len){

    uint16 w;

    xrtp_buffer_t * buf = rtcp->buffer;

    /* Unpack RTP Fixed head */
    if(buffer_next_uint16(buf, &w) != sizeof(uint16)){ return XRTP_EFORMAT; }

    *r_ver = (w >> 14) & 0x3;       /* '00000011' */
    *r_padding = (w >> 13) & 0x1;   /* '00000001' */
    *r_count = (w >> 8) & 0x1F;     /* '00011111' */
    *r_type = w & 0xFF;             /* '11111111' */

    if(buffer_next_uint16(buf, r_len) != sizeof(uint16)){ return XRTP_EFORMAT; }

    packet_log(("rtcp_parse_head: %d 32-bit RTCP #%d Packet found\n", *r_len, *r_type));

    return XRTP_OK;
 }
    
 xrtp_rtcp_sdes_item_t * _rtcp_sdes_new_item(uint8 id, uint8 vlen, char * value){

    xrtp_rtcp_sdes_item_t * item = (xrtp_rtcp_sdes_item_t *)malloc(sizeof(struct xrtp_rtcp_sdes_item_s));
    if(item){

       item->id = id;
       item->len = vlen;
       memcpy(item->value, value, vlen);
    }

    return item;
 }

 xrtp_rtcp_sdes_item_t * _rtcp_sdes_new_priv_item(uint8 vlen, uint8 prelen, char * prefix, uint8 privlen, char * prival){

    xrtp_rtcp_sdes_item_t * item = (xrtp_rtcp_sdes_item_t *)malloc(sizeof(struct xrtp_rtcp_sdes_item_s));
    if(item){

       item->id = RTCP_SDES_PRIV;
       item->len = vlen;
       item->$priv.len_prefix = prelen;
       memcpy(item->$priv.prefix, prefix, prelen);
       item->$priv.len_value = privlen;
       memcpy(item->$priv.value, prival, privlen);
    }

    return item;
 }

 xrtp_rtcp_sdes_t * _rtcp_new_sdes(uint8 ver, uint8 pad, uint8 count, uint16 len){

    xrtp_rtcp_sdes_t * sdes = (xrtp_rtcp_sdes_t *)malloc(sizeof(struct xrtp_rtcp_sdes_s));
    if(sdes){

       sdes->$head.version = ver;
       sdes->$head.padding = pad;
       sdes->$head.count = 0;  /* update late */
       sdes->$head.type = RTCP_TYPE_SDES;
       sdes->$head.length = len;
       sdes->$head.bytes = RTCP_HEAD_FIX_BYTE;
    }

    return sdes;
 }

 int _rtcp_sdes_add_item(xrtp_rtcp_sdes_t * sdes, uint32 SRC, xrtp_rtcp_sdes_item_t * item){

    int i;
    int chk_exist = 0;
    
    for(i=0; i<sdes->$head.count; i++){

       if(sdes->chunks[i]->SRC == SRC){

          chk_exist = 1;
          
          /* As the function usually for rebuilding from byte stream
           * Assume no same id items will came at same time */
          sdes->chunks[i]->items[sdes->chunks[i]->n_item] = item;
          packet_log(("_rtcp_sdes_add_item: chunk(%d).items[%d]\n", sdes->chunks[i]->SRC, sdes->chunks[i]->n_item));
          sdes->chunks[i]->n_item++;
       }
    }

    if(!chk_exist){

       xrtp_rtcp_sdes_chunk_t * chunk = (xrtp_rtcp_sdes_chunk_t *)malloc(sizeof(struct xrtp_rtcp_sdes_chunk_s));
       if(!chunk){

          return XRTP_EMEM;
       }

       chunk->SRC = SRC;
       chunk->items[0] = item;
       chunk->n_item = 1;
       sdes->chunks[sdes->$head.count++] = chunk;
       
       packet_log(("_rtcp_sdes_add_item: new %dth chunk(%d).items[0]\n", sdes->$head.count, SRC));
    }

    return XRTP_OK;
 }

 /**
  * Unpack RTCP SDES packet
  */
 int _rtcp_unpack_sdes(xrtp_rtcp_compound_t * rtcp, uint8 ver, uint8 pad, uint8 count, uint16 len){

    int ret = XRTP_OK;
    uint32 SRC;
    uint8 id, vlen;
    uint chk_start, chk_pad;
    
    xrtp_rtcp_sdes_item_t * item = NULL;
        
    int i = 0;
    int end = 0;
    xrtp_buffer_t * buf = rtcp->buffer;

    xrtp_rtcp_sdes_t * sdes = _rtcp_new_sdes(ver, pad, count, len);

	if(!sdes){
       return XRTP_EMEM;
    }
    
    for(i=0; i<count; i++){

       chk_start = buffer_position(buf);
       
       /* Unpack a chunk */
       if(buffer_next_uint32(buf, &SRC) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       
       while(!end){
          
          /* Unpack an item */
          if(buffer_next_uint8(buf, &id) != sizeof(uint8)){ ret=XRTP_EFORMAT; }

          if(id == RTCP_SDES_END){

             uint pos = buffer_position(buf);
             
             end = 1;
             chk_pad = 4 - (pos - chk_start) % 4;
             sdes->$head.bytes += chk_pad + pos - chk_start;
             if(pos % 4)
                buffer_seek(buf, buffer_position(buf) + chk_pad);
             break;
          
          }else if(id == RTCP_SDES_PRIV){
          
             uint8 prelen;
             char* prefix;
             uint8 privlen;
			 char *prival;

             if(buffer_next_uint8(buf, &vlen) != sizeof(uint8)){ ret=XRTP_EFORMAT; }
             if(buffer_next_uint8(buf, &prelen) != sizeof(uint8)){ ret=XRTP_EFORMAT; }

			 prefix = (char *)malloc(prelen);
             if(!prefix)  return XRTP_EMEM;
             if(buffer_next_data(buf, prefix, prelen) != prelen){ ret=XRTP_EFORMAT; }

			 privlen = vlen - prelen - sizeof(uint8);
             prival = (char *)malloc(privlen);
             if(buffer_next_data(buf, prival, privlen) != privlen){ ret=XRTP_EFORMAT; }

             item = _rtcp_sdes_new_priv_item(vlen, prelen, prefix, privlen, prival);
          
          }else{   /* id != RTCP_SDES_PRIV */

             char * val;

			 if(buffer_next_uint8(buf, &vlen) != sizeof(uint8)){ ret=XRTP_EFORMAT; }
             val = (char *)malloc(vlen);
             if(buffer_next_data(buf, val, vlen) != vlen){ ret=XRTP_EFORMAT; }

             item = _rtcp_sdes_new_item(id, vlen, val);
          }
          
          ret = _rtcp_sdes_add_item(sdes, SRC, item);
       }
    }

    rtcp->heads[rtcp->n_packet++] = (xrtp_rtcp_head_t *)sdes;
    packet_log(("_rtcp_unpack_sdes: Add %dth packet(SDES)\n", rtcp->n_packet));
    
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
       
       why = (char *)malloc(sizeof(char)*wlen);
       if(!why) return XRTP_EMEM;

       ret = buffer_next_data(buf, why, wlen);
       if(ret < XRTP_OK){

          free(why);
          return ret;
       }
    }

    bye = rtcp_new_bye(rtcp, srcs, count, wlen, why);
    if(!bye){

       if(why) free(why);
       return XRTP_FAIL;
    }
    packet_log(("_rtcp_unpack_bye: Add %dth packet(BYE)\n", rtcp->n_packet));

    return ret;
 }

 /**
  * Unpack RTCP APP packet
  */
 int _rtcp_unpack_app(xrtp_rtcp_compound_t * rtcp, uint8 ver, uint8 padding, uint8 count, uint16 length){

    int ret = XRTP_OK;
    
    uint32 SSRC, name;
	int dlen = 0;
    xrtp_buffer_t * buf = rtcp->buffer;
	char * data = NULL;
	xrtp_rtcp_app_t * app = NULL;
    
    if(buffer_next_uint32(buf, &SSRC) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
    if(buffer_next_uint32(buf, &name) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
    
    dlen = (length * 4) - 8;

    data = (char *)malloc(sizeof(char) * dlen);
    if(!data){

       return XRTP_EMEM;
    }
    if(buffer_next_data(buf, data, dlen) != dlen){ ret=XRTP_EFORMAT; }
    
    app = rtcp_new_app(rtcp, SSRC, count, name, dlen, data);
    if(!app){

       free(data);
       return XRTP_FAIL;
    }
    packet_log(("_rtcp_unpack_app: Add %dth packet(APP)\n", rtcp->n_packet));

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

    /** Don't need, as allocation is assumed.
    
    rtcp->heads = (xrtp_rtcp_head_t* *)malloc(sizeof(xrtp_rtcp_head_t *) * XRTP_MAX_RTCP_PACKETS);
    if(!rtcp->heads){

       return XRTP_EMEM;
    }
    
    rtcp->max_packets = XRTP_MAX_RTCP_PACKETS;
    rtcp->n_packet = 0;
    rtcp->valid_to_get = 0;
    
    **/
    pos_start = buffer_position(rtcp->buffer); /* Record the start point */
    
    /* Unpack RTCP Fixed head */
    ret = rtcp_parse_head(rtcp, rtcp->first_head, &ver, &padding, &count, &type, &length);
    if(ret < XRTP_OK) return ret;

    /* Unpack RTCP first Packet, MUST be SR or RR */
    if(type == RTCP_TYPE_SR){
       
       uint32 SSRC, hi_ntp, lo_ntp, rtp_stamp, npac, noct;
		xrtp_rtcp_sr_t * sr = NULL;

       packet_log(("rtcp_unpack: Sender Report\n"));
       /* Create Sender Report */
       sr = _rtcp_new_sr_packet(ver, padding, count, length);
       if(!sr){

          /** Don't need, as abrove reason
          free(rtcp->heads);
          **/
          return XRTP_EMEM;
       }
       sr->$head.version = ver;
       sr->$head.padding = padding;
       sr->$head.count = 0;    /* update later */
       sr->$head.type = type;
       sr->$head.length = length;
       sr->$head.bytes = length * 4; /* without head bytes */
       if(sr->$head.bytes < RTCP_SENDERINFO_BYTE + sr->$head.count * RTCP_REPORT_BYTE){

          return XRTP_EFORMAT;
       }
       
       /* Parsing Senderinfo Block */
       if(buffer_next_uint32(rtcp->buffer, &SSRC) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &hi_ntp) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &lo_ntp) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &rtp_stamp) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &npac) != sizeof(uint32)){ ret=XRTP_EFORMAT; }
       if(buffer_next_uint32(rtcp->buffer, &noct) != sizeof(uint32)){ ret=XRTP_EFORMAT; }

       sr->$sender.SSRC = SSRC;
       sr->$sender.hi_ntp = hi_ntp;
       sr->$sender.lo_ntp = lo_ntp;
       sr->$sender.rtp_stamp = rtp_stamp;
       sr->$sender.n_packet_sent = npac;
       sr->$sender.n_octet_sent = noct;
       
       rtcp->heads[0] = rtcp->first_head = (xrtp_rtcp_head_t *)sr;
       
    }else if(type == RTCP_TYPE_RR){  /* type == RTCP_TYPE_RR */
       
       uint32 SSRC;
	   xrtp_rtcp_rr_t * rr = NULL;
       
       packet_log(("rtcp_unpack: Receiver Report\n"));
       /* Unpack Receiver Report */
       if(buffer_next_uint32(rtcp->buffer, &SSRC) != sizeof(uint32)){ ret=XRTP_EFORMAT; }

       /* Create Sender Report */
       rr = _rtcp_new_rr_packet(ver, padding, count, length);
       if(!rr){

          /** Don't need, as abrove reason
          free(rtcp->heads);
          **/
          return XRTP_EMEM;
       }
       rr->$head.version = ver;
       rr->$head.padding = padding;
       rr->$head.count = 0;    /* update later */
       rr->$head.type = type;
       rr->$head.length = length;
       rr->$head.bytes = length * 4; /* without head bytes */
       if(rr->$head.bytes < RTCP_SSRC_BYTE + rr->$head.count * RTCP_REPORT_BYTE){

          return XRTP_EFORMAT;
       }
       rr->SSRC = SSRC;

       rtcp->heads[0] = rtcp->first_head = (xrtp_rtcp_head_t *)rr;

    }else{

       return XRTP_EFORMAT;
    }

    rtcp->n_packet = 1;
    /* Unpack Report Block */
    packet_log(("rtcp_unpack: %d reports in rtcp\n", count));
    for(i=0; i<count; i++){

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
    if(len_pac < (length+1)*4){

       if(hand && hand->parse_rtcp_ext){
          
          ret = hand->parse_rtcp_ext(hand, rtcp, rtcp->session);
          if(ret < XRTP_OK) return ret;
          
       }else{

          buffer_seek(rtcp->buffer, pos_start + (length+1) * 4); /* See RFC-1889 */
       }
    }

    /* Callback handler to handle Report Packet
    if(hand && hand->parse_rtcp_packet){
       ret = hand->parse_rtcp_packet(hand, rtcp, rtcp->first_head, rtcp->session);
       if(ret < XRTP_OK) return ret;
    } */
    
    /* Unpack following RTCP Packet */
    len_unscaned = buffer_datalen(rtcp->buffer) - buffer_position(rtcp->buffer);
    
    i = 1;     /* scan for head#1 or forewhile */    
    while(len_unscaned > sizeof(uint32)){ /* More packet head to scan */

       pos_start = buffer_position(rtcp->buffer);
       
       ret = rtcp_parse_head(rtcp, rtcp->heads[i], &ver, &padding, &count, &type, &length);
       if(ret < XRTP_OK) return ret;
       switch(type){

          case(RTCP_TYPE_SDES):
             packet_log(("rtcp_unpack: unpack sdes\n"));
             ret = _rtcp_unpack_sdes(rtcp, ver, padding, count, length);
             if(ret < XRTP_OK) return ret;
             break;
          case(RTCP_TYPE_BYE):
             packet_log(("rtcp_unpack: unpack bye\n"));
             ret = _rtcp_unpack_bye(rtcp, ver, padding, count, length);
             if(ret < XRTP_OK) return ret;
             break;
          case(RTCP_TYPE_APP):
             packet_log(("rtcp_unpack: unpack app\n"));
             ret = _rtcp_unpack_app(rtcp, ver, padding, count, length);
             if(ret < XRTP_OK) return ret;
             break;
          default:
             packet_log(("rtcp_unpack: skip %d bytes\n", (length+1) * 4));
             buffer_seek(rtcp->buffer, pos_start + (length+1) * 4); /* Skip unsupported packet */
       }
       
       /* Callback handler to handle RTCP Packet
       if(hand && hand->parse_rtcp_packet){
          packet_log(("rtcp_unpack: callback to handler\n"));
          ret = hand->parse_rtcp_packet(hand, rtcp, rtcp->heads[i], rtcp->session);
          if(ret < XRTP_OK) return ret;
       } */

       i++;
       len_unscaned = buffer_datalen(rtcp->buffer) - buffer_position(rtcp->buffer);
    }
    
    return ret;
 }

