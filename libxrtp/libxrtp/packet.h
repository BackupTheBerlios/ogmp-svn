/***************************************************************************
                          xrtp_packet.h  -  description
                             -------------------
    begin                : Mon Mar 10 2003
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

 #ifndef XRTP_PACKET_H

 #define XRTP_PACKET_H
 
 #include <timedia/buffer.h>
 
 #include "packet_const.h"
 #include "connect.h"

 typedef struct xrtp_rtp_packet_s xrtp_rtp_packet_t; 
 typedef struct xrtp_rtp_head_s xrtp_rtp_head_t;
 typedef struct xrtp_rtp_headext_s xrtp_rtp_headext_t;
 typedef struct xrtp_rtp_payload_s xrtp_rtp_payload_t;

 typedef struct xrtp_rtcp_compound_s xrtp_rtcp_compound_t; 
 typedef struct xrtp_rtcp_sr_s xrtp_rtcp_sr_t;
 typedef struct xrtp_rtcp_rr_s xrtp_rtcp_rr_t;
 
 typedef struct xrtp_rtcp_head_s xrtp_rtcp_head_t;
 typedef struct xrtp_rtcp_senderinfo_s xrtp_rtcp_senderinfo_t;
 typedef struct xrtp_rtcp_report_s xrtp_rtcp_report_t;
 typedef struct xrtp_rtcp_ext_s xrtp_rtcp_ext_t;
 
 typedef struct xrtp_rtcp_sdes_s xrtp_rtcp_sdes_t;
 typedef struct xrtp_rtcp_sdes_chunk_s xrtp_rtcp_sdes_chunk_t;
 typedef struct xrtp_rtcp_sdes_item_s xrtp_rtcp_sdes_item_t;
 
 typedef struct xrtp_rtcp_bye_s xrtp_rtcp_bye_t;
 typedef struct xrtp_rtcp_app_s xrtp_rtcp_app_t;
 
 /**
  * RTP Packet head
  */
 struct xrtp_rtp_head_s {

    uint8 version  :2;
    uint8 padding  :1;
    uint8 ext      :1;
    uint8 n_CSRC   :4;
    uint8 mark     :1;
    uint8 pt         :7;
    uint16 seqno      :16;

    uint32 timestamp :32;
    uint32 SSRC      :32;

    unsigned int csrc_list[XRTP_MAX_CSRC];
 };

 /**
  * RTP Payload
  */
 struct xrtp_rtp_payload_s
 {
   /* info for incoming payload, data could be part of the whole incoming packet buffer */
    char *data;		/* pointer to the data in the buffer */
    int  len;		/* payload bytes */
    int  buf_pos;	/* payload position in buffer if parsed from buffer */

   /* the buffer for outgoing payload */
   buffer_t *out_buffer;
 };

 enum xrtp_direct_e{

    RTP_SEND,
    RTP_RECEIVE
 };
 
 /**
  * RTP Packet
  */
 struct xrtp_rtp_packet_s
 {
    void * handler;

    xrtp_session_t * session;
    
    rtime_t msec_arrival;
    rtime_t usec_arrival;
    rtime_t nsec_arrival;

    session_connect_t * connect;

    int valid_to_get;
    
    enum xrtp_direct_e direct;

    struct xrtp_rtp_head_s $head;
    
    xrtp_rtp_headext_t * headext;
    
    struct xrtp_rtp_payload_s $payload;

    uint packet_bytes;
    
    xrtp_buffer_t * buffer;

    int member_state_updated;

    uint32 exseqno;

    /* Tag for Scheduling */
    int info_bytes;
 };
 
 /**
  * RTP Packet head extension
  */
 struct xrtp_rtp_headext_s {

    uint16 info;         /*:16;*/
    uint16 len;          /*:16;*/
    
    char * data;     

    int buf_pos;         /* headext position in packet buffer if parse from buffer */
    uint16 len_dw;        /* len of 4bytes */
    uint8  len_pad;
 };
 
 struct xrtp_rtcp_compound_s {

    void * handler;

    xrtp_session_t * session;

    rtime_t msec,usec,nsec;  /* arrival time */

    int bytes_received;

    session_connect_t * connect;

    int valid_to_get;
    
    uint8 padding;   /* store number of padding */
    
    enum xrtp_direct_e direct;

    xrtp_rtcp_head_t * first_head;
    
    int max_packets;
    /* Point to an array of pointer to all rtcp packets head include first */
    xrtp_rtcp_head_t* *heads;    
    /* Number of all packets */
    int n_packet;
    
    uint32 bytes;
    //uint len_buffer;
    uint maxlen;

    xrtp_buffer_t * buffer;

    int member_state_updated;
 };
 
 /**
  * RTCP packet head, 4 bytes
  */
 struct xrtp_rtcp_head_s {

    uint8 version; /*2 bits*/
    uint8 padding; /*1 bit*/
    uint8 count;    /*5 bits*/
    uint8 type;
    uint16 length;
    
    int32 bytes; /* count bytes of each packet */
 };

 /**
  * Report Block, 24 bytes
  */
 struct xrtp_rtcp_report_s{

    uint32 SRC       :32;
    
    uint8 frac_lost  :8;  /* lost/expect * 256 */
    uint32 total_lost :24;
    
    uint32 full_seqn :32;
    /*
     * D(i,j) = (Rj-Sj)-(Ri-Si)
     * J = j + (|D(i-1,i)| - J)/16
     */
    uint32 jitter     :32;
    uint32 stamp_lsr  :32; /* Last SR timestamp, middle 32 bits of NTP */
    uint32 delay_lsr  :32; /* 1/65536 seconds unit */
 };
 
 /**
  * Profile-specific extension for the Report
  */
 struct xrtp_rtcp_ext_s{
    
    int len;
    char * data;
 };
 
 /**
  * Sender info Block, 24 bytes
  */
 struct xrtp_rtcp_senderinfo_s{

    uint32 SSRC            :32;
    uint32 hi_ntp          :32;
    uint32 lo_ntp          :32;
    uint32 rtp_stamp       :32;
    uint32 n_packet_sent   :32;
    uint32 n_octet_sent    :32;
 };

 /**
  * Sender Report
  */
 struct xrtp_rtcp_sr_s {
    
    struct xrtp_rtcp_head_s $head;
    
    struct xrtp_rtcp_senderinfo_s $sender;
    
    xrtp_rtcp_report_t * reports[XRTP_MAX_REPORTS];

    xrtp_rtcp_ext_t * ext;
 };
 
 /**
  * Receiver Report
  */
 struct xrtp_rtcp_rr_s {

    struct xrtp_rtcp_head_s $head;
    
    uint32 SSRC;
    
    xrtp_rtcp_report_t * reports[XRTP_MAX_REPORTS];

    xrtp_rtcp_ext_t *ext;
 };

 #define RTCP_SDES_MAX_LEN 256
 
 struct xrtp_rtcp_sdes_item_s
 {
    uint8 id    :8;
    uint8 len   :8;
    
    union
    {
       char value[RTCP_SDES_MAX_LEN];
       
       struct
       {
          uint8 len_prefix :8;
          char prefix[RTCP_SDES_MAX_LEN];
          uint8 len_value  :8;
          char value[RTCP_SDES_MAX_LEN];
          
       }$priv;
    };
 };

 struct xrtp_rtcp_sdes_chunk_s{

    uint32 SRC;
    int n_item;
    int ended;
    uint8 padlen;
    uint max_item;
    xrtp_rtcp_sdes_item_t* items[XRTP_MAX_CHUNK_ITEM];
 };
 
 struct xrtp_rtcp_sdes_s{

    struct xrtp_rtcp_head_s $head;

    xrtp_rtcp_sdes_chunk_t * chunks[XRTP_MAX_SDES_CHUNK];
 };

 struct xrtp_rtcp_bye_s{

    struct xrtp_rtcp_head_s $head;

    uint32 srcs[XRTP_MAX_BYE_SRCS];
    
    uint8  pad_why;
    uint8  len_why;
    char * why;
 };

 struct xrtp_rtcp_app_s{

    struct xrtp_rtcp_head_s $head;

    uint32 SSRC;
    
    uint32 name; /* ASCII string */

    int len_data;
    char * data;
 };

 /* -------------------- End of structure claim ----------------------- */

#ifdef __cplusplus
extern "C"
{
#endif

 /* ------------------- General Packet interface ------------------------ */

 int rtp_packet_set_handler(xrtp_rtp_packet_t * packet, void * handler);

 int rtcp_compound_set_handler(xrtp_rtcp_compound_t * compound, void * handler);
 
 /* --------------------- RTP Packet interface ------------------------ */

 /**
  * Create an empty RTP packet associated to the given RTP session, detail done by Session.
  */
 xrtp_rtp_packet_t * rtp_new_packet(xrtp_session_t * session, int pt, enum xrtp_direct_e dir, session_connect_t * connect
		, rtime_t ms_arrival, rtime_t us_arrival, rtime_t ns_arrival);

 /**
  * Get rtp buffer
  */
 xrtp_buffer_t * rtp_packet_buffer(xrtp_rtp_packet_t * packet);

 /**
  * Associate a buffer to packet, return old buffer associated before
  */
 int rtp_packet_set_buffer(xrtp_rtp_packet_t * packet, xrtp_buffer_t * buffer);

/**
 * Release the packet resource.
 */
extern DECLSPEC
int 
rtp_packet_done(xrtp_rtp_packet_t * packet);

/**
 * Set head info related to the given RTP packet.
 */
xrtp_rtp_head_t * rtp_packet_set_head(xrtp_rtp_packet_t * packet, uint32 ssrc, uint16 seqno);

extern DECLSPEC
uint32 
rtp_packet_ssrc(xrtp_rtp_packet_t * packet);
 
/**
 * Specify the maximum length of a rtp packet 
 *
 * param in: value zero means default max length.
 */
int rtp_packet_set_maxlen(xrtp_rtp_packet_t * packet, uint maxlen);
uint rtp_packet_maxlen(xrtp_rtp_packet_t * packet);
uint rtp_packet_payload_maxlen(xrtp_rtp_packet_t * packet);
 
extern DECLSPEC
uint 
rtp_packet_bytes(xrtp_rtp_packet_t * packet);
 
 /* Validate the incoming packet is OK to access data */
 int rtp_packet_validate(xrtp_rtp_packet_t * pac);
 /* Check the incoming packet is valid to access data */
 int rtp_packet_valid(xrtp_rtp_packet_t * pac);

extern DECLSPEC
int 
rtp_packet_set_mark(xrtp_rtp_packet_t * packet, int boolen);

int rtp_packet_mark(xrtp_rtp_packet_t * packet);

int rtp_packet_set_seqno(xrtp_rtp_packet_t * packet, uint16 seqno);

extern DECLSPEC
uint16 
rtp_packet_seqno(xrtp_rtp_packet_t * packet);

extern DECLSPEC
int 
rtp_packet_set_timestamp(xrtp_rtp_packet_t * packet, uint32 ts);

extern DECLSPEC
uint32
rtp_packet_timestamp(xrtp_rtp_packet_t * packet);

/**
 * Add a CSRC and return num of CSRC in the packet.
 */
int rtp_packet_add_csrc(xrtp_rtp_packet_t * packet, uint32 CSRC);

/**
 * Set head extension in the packet.
 */
int rtp_packet_set_headext(xrtp_rtp_packet_t * packet, uint16 info, uint16 len, char * xdata);

uint16 rtp_packet_headext_info(xrtp_rtp_packet_t * packet);
 
uint16 rtp_packet_headext(xrtp_rtp_packet_t * packet, char* *ret_data);
 
/* Just point payload to source data, no copying involved */
extern DECLSPEC
int 
rtp_packet_set_payload(xrtp_rtp_packet_t *packet, buffer_t *payload);

/* Copy payload data from source */
extern DECLSPEC
xrtp_rtp_payload_t * 
rtp_packet_new_payload(xrtp_rtp_packet_t * pac, int len, char * pay);
 
/**
 * Get payload data.
 */
extern DECLSPEC
uint 
rtp_packet_payload(xrtp_rtp_packet_t * packet, char **ret_payload);

extern DECLSPEC
uint 
rtp_packet_dump_payload(xrtp_rtp_packet_t * packet, char **ret_payload);

extern DECLSPEC
uint 
rtp_packet_payload_bytes(xrtp_rtp_packet_t * packet);

 /* Tag for Scheduling proposal */
 int rtp_packet_set_info(xrtp_rtp_packet_t * packet, int bytes); 
 int rtp_packet_info(xrtp_rtp_packet_t * packet, int *bytes);
 
extern DECLSPEC
int
rtp_packet_arrival_time(xrtp_rtp_packet_t * rtp, rtime_t *ms, rtime_t *us, rtime_t *ns);
 
 /* --------------------- RTCP Compound interface ------------------------ */

 /**
  * Display rtcp compound
  */
 void rtcp_show(xrtp_rtcp_compound_t * rtcp);

 /**
  * Create a empty RTCP compound packet for recreate the rtcp structure.
  *
  * param in: Session that rtcp belonged to, if npacket = 0, set to default value
  */
 xrtp_rtcp_compound_t * rtcp_new_compound(xrtp_session_t * session, uint npacket, enum xrtp_direct_e dir, session_connect_t * conn
											, rtime_t ms_arrival, rtime_t us_arrival, rtime_t ns_arrival);

 /**
  * The incoming data buffer for rtcp compound
  */
 int rtcp_set_buffer(xrtp_rtcp_compound_t * rtcp, xrtp_buffer_t * buf);

 xrtp_buffer_t * rtcp_buffer(xrtp_rtcp_compound_t * rtcp);

 xrtp_rtcp_compound_t * rtcp_sender_report_new(xrtp_session_t * session, int size, int padding);

 /**
  * Create a RTCP compound packet used for receiver report.
  *
  * param in: max number of packets allowed
  */
 xrtp_rtcp_compound_t * rtcp_receiver_report_new(xrtp_session_t * session, uint32 SSRC, int size, int padding);

 /**
  * Validate the incoming RTCP packet OK to access data
  */
 int rtcp_validate(xrtp_rtcp_compound_t * compound);
 /**
  * Check If the incoming RTCP packet is valid to access data
  */
 int rtcp_valid(xrtp_rtcp_compound_t * compound);
 
 uint8 rtcp_type(xrtp_rtcp_compound_t * compound);

 /**
  * If the RTCP packet is padding
  */
 int rtcp_padding(xrtp_rtcp_compound_t * compound);

/**
 * Get the RTCP Packet length
 */
extern DECLSPEC
uint 
rtcp_length(xrtp_rtcp_compound_t * compound);
 
int rtcp_set_maxlen(xrtp_rtp_packet_t * packet, uint maxlen);

int rtcp_set_sender_info(xrtp_rtcp_compound_t * compound, uint32 SSRC,
                          uint32 hi_ntp, uint32 lo_ntp, uint32 rtp_ts,
                          uint32 packet_sent, uint32 octet_sent);

extern DECLSPEC
int
rtcp_arrival_time(xrtp_rtcp_compound_t * compound, rtime_t *ms, rtime_t *us, rtime_t *ns);
 
extern DECLSPEC
int 
rtcp_sender_info(xrtp_rtcp_compound_t * compound, uint32 * r_SSRC,
                          uint32 * r_hi_ntp, uint32 * r_lo_ntp, uint32 * r_rtp_ts,
                          uint32 * r_packet_sent, uint32 * r_octet_sent);

extern DECLSPEC
uint32 
rtcp_sender(xrtp_rtcp_compound_t * compound);

 int rtcp_add_report(xrtp_rtcp_compound_t * compound, uint32 SSRC,
                     uint8 frac_lost, uint32 total_lost, uint32 seqn_ext,
                     uint32 jitter, uint32 lsr_stamp,uint32 delay_lsr);

extern DECLSPEC
int 
rtcp_report(xrtp_rtcp_compound_t * compound, uint32 SRC,
                     uint8 * ret_frac_lost, uint32 * ret_total_lost,
                     uint32 * ret_full_seqno, uint32 * ret_jitter,
                     uint32 * ret_lsr_stamp, uint32 * ret_lsr_delay);

 int rtcp_done_all_reports(xrtp_rtcp_compound_t * compound);

 /**
  * Create the rtcp packet,
  */
 xrtp_rtcp_sdes_t * rtcp_new_sdes(xrtp_rtcp_compound_t * compound);

 int rtcp_add_sdes(xrtp_rtcp_compound_t * compound, uint32 SRC, uint8 type, uint8 len, char * value);
 int rtcp_add_priv_sdes(xrtp_rtcp_compound_t * compound, uint32 SRC, uint8 prefix_len, char * prefix_value, uint8 len, char * value);
 /**
  * Indicate an end of a SDES chunk, Need to be call after add all SDES item of a SRC
  */
 int rtcp_end_sdes(xrtp_rtcp_compound_t * compound, uint32 SRC);

 extern DECLSPEC
 uint8
 rtcp_sdes(xrtp_rtcp_compound_t * compound, uint32 SRC, uint8 type, char **r_sdes);

 extern DECLSPEC
 uint8
 rtcp_priv_sdes(xrtp_rtcp_compound_t * compound, uint32 SRC, uint8 prefix_len, char * prefix_value, char* *ret_value);

 xrtp_rtcp_bye_t * rtcp_new_bye(xrtp_rtcp_compound_t * compound, uint32 SRCs[], uint8 n_SRC, uint8 len, char * why);

 uint8 rtcp_bye_why(xrtp_rtcp_compound_t * compound, uint32 SSRC, uint32 SRC, char* *ret_why);
 uint rtcp_bye_from(xrtp_rtcp_compound_t * compound, uint32 SSRCs[], uint nbuf);

extern DECLSPEC
xrtp_rtcp_app_t * 
rtcp_new_app(xrtp_rtcp_compound_t * compound, uint32 SRC, uint8 subtype, uint32 name, uint len, char * appdata);
 
extern DECLSPEC
int 
rtcp_app(xrtp_rtcp_compound_t * compound, uint32 SRC, uint8 subtype, uint32 name, char **appdata);

 /**
  * Release the compound packet
  */
extern DECLSPEC
int 
rtcp_compound_done(xrtp_rtcp_compound_t * compound);

 /**
  * Display content of RTP Packet
  */
 void rtp_packet_show(xrtp_rtp_packet_t * rtp);
 /**
  * Pack RTP Packet to byte stream for send, invoken by Profile Handler
  */
extern DECLSPEC
xrtp_buffer_t * 
rtp_pack(xrtp_rtp_packet_t * packet);
 /**
  * Unpack RTP Packet to memory for parsing, invoken by Profile Handler
  */
extern DECLSPEC
int 
rtp_unpack(xrtp_rtp_packet_t * packet);

 /**
  * Pack RTCP Compound to byte stream for send, invoken by Profile Handler
  */
extern DECLSPEC
xrtp_buffer_t * 
rtcp_pack(xrtp_rtcp_compound_t * compound);
 /**
  * Unpack RTCP Compound to memory for parsing, invoken by Profile Handler
  */
extern DECLSPEC
int 
rtcp_unpack(xrtp_rtcp_compound_t * compound);
 /**
  * Unpack RTCP Packet Head
  */
 int rtcp_parse_head(xrtp_rtcp_compound_t * rtcp, xrtp_rtcp_head_t * head,
                     uint8 * r_ver, uint8 * r_padding, uint8 * r_count,
                     uint8 * r_type, uint16 * r_len);

#ifdef __cplusplus
}
#endif

#endif
 

 
