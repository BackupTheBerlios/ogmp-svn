/*
 * Copyright (C) 2000-2002 the timedia project
 *
 * This file is part of rtpsession, a modulized rtp library.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * timedia is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: payload_handler.h,v 0.1 11/12/2002 15:47:39 heming$
 *
 */
 
 #ifndef XRTP_HANDLER_H

 #define XRTP_HANDLER_H
 
 #include "packet.h"

 #include "media.h"
 
 #define XRTP_CAP_NONE 0

 #define XRTP_HANDLER_IDEL 0
 #define XRTP_HANDLER_UNLOAD_PACKET 1

 #define XRTP_HANDLER_TYPE_UNKNOWN   0
 #define XRTP_HANDLER_TYPE_MEDIA     1
 #define XRTP_HANDLER_TYPE_TRANS     2
 #define XRTP_HANDLER_TYPE_CONVERT   3
 #define XRTP_HANDLER_TYPE_TEST      4
 #define XRTP_HANDLER_TYPE_MAX       5

 typedef struct param_network_s{

    int family;
    int type;
    int protocol;
    struct sockaddr * address;

 } param_network_t;

 typedef struct profile_handler_s profile_handler_t;
 
 typedef struct profile_class_s profile_class_t;

 struct profile_class_s {

	 char* (*id)(profile_class_t *class);

   int (*type)(profile_class_t *class);

	 char* (*description)(profile_class_t *class);

	 int (*capacity)(profile_class_t *class);

    int (*handler_number)(profile_class_t *class);

    int (*done)(profile_class_t *class);

    profile_handler_t* (*new_handler)(profile_class_t *class, xrtp_session_t * session);

    int (*done_handler)(profile_handler_t * h);
 };

 struct profile_handler_s {

   profile_class_t* (*module)(profile_handler_t *handler);

   xrtp_media_t* (*media)(profile_handler_t *handler);

	 //int (*clockrate)(profile_handler_t *handler);

   session_connect_t* (*connect)(profile_handler_t *handler, xrtp_session_t * session, void* param);

   void* (*master)(profile_handler_t *handler);
   int (*set_master)(profile_handler_t *handler, void *master);

   int (*get_callback)(profile_handler_t *handler, int type, void **ret_callback, void **ret_data);   
   int (*set_callout)(profile_handler_t *handler, int type, void *callout, void *data);

   /* Must implement one of 2 kind of methods according to pipe type */
   int (*rtp_in)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp);
   
   int (*rtp_out)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp);

   int (*rtp_size)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp);
   
   int (*rtcp_in)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp);

   int (*rtcp_out)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp);

   int (*rtcp_size)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp);

   /* Pack Packet to byte buffer for send */
   int (*pack_rtp)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

	int (*pack_rtcp)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

   /* Unpack Packet from byte stream received */
   int (*unpack_rtp)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

	int (*unpack_rtcp)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

   /* Make RTP Data Packet */
   int (*make_rtp_head)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

	int (*make_rtp_headext)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session, int xinfo);

	int (*make_rtp_payload)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

	int (*pack_rtp_headext)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session, int xinfo, int dwlen);

   /* Parse RTP Data Packet */
	int (*parse_rtp_head)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

	int (*parse_rtp_headext)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

	int (*parse_rtp_payload)(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

   /* Make RTP Control Packet */
   int (*make_rtcp_head)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

	int (*make_rtcp_sender_info)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

	int (*make_rtcp_report_block)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

	int (*make_rtcp_ext)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

   /* Parse RTP Control Packet */
	int (*parse_rtcp_head)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

   int (*parse_rtcp_sender_info)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

   int (*parse_rtcp_report_block)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

   int (*parse_rtcp_ext)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

	int (*parse_rtcp_packet)(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_rtcp_head_t * head, xrtp_session_t *session);

 };

#endif
