/***************************************************************************
                          plug_dummy.h  -  Dummy plugin do nothing
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
 
 /**
  * Main Methods MUST BE implemented
  */
 int dummy_handle_rtp_packet_in(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

 int dummy_handle_rtp_packet_out(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

 int dummy_handle_rtcp_compound_in(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 int dummy_handle_rtcp_compound_out(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 /**
  * Methods for making RTP Data Packet (RTP)
  */
 int dummy_make_rtp_packet(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

 int dummy_make_rtp_head(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

 int dummy_make_rtp_headext(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t * session, int sixteen);

 int dummy_make_rtp_payload(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

 /**
  * Methods for parsing RTP Data Packet (RTP)
  */
 int dummy_parse_rtp_packet(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

 int dummy_parse_rtp_head(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

 int dummy_parse_rtp_headext(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

 int dummy_parse_rtp_payload(profile_handler_t *handler, xrtp_rtp_packet_t *rtp, xrtp_session_t *session);

 /**
  * Methods for making RTP Control Packet (RTCP)
  */
 int dummy_make_rtcp_compound(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 int dummy_make_rtcp_head(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 int dummy_make_rtcp_sender_info(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 int dummy_make_rtcp_report_block(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 int dummy_make_rtcp_ext(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 /**
  * Methods for parsing RTP Control Packet (RTCP)
  */
 int dummy_parse_rtcp_compound(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 int dummy_parse_rtcp_head(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 int dummy_parse_rtcp_sender_info(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 int dummy_parse_rtcp_report_block(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 int dummy_parse_rtcp_ext(profile_handler_t *handler, xrtp_rtcp_compound_t *rtcp, xrtp_session_t *session);

 /**
  * Methods for module class
  */
 profile_class_t * dummy_get_module(profile_handler_t * handler);

 int dummy_set_master(profile_handler_t *handler, void *master);

 void * dummy_get_master(profile_handler_t *handler);

 int dummy_get_callback(profile_handler_t *handler, int type, void** callout, void** data);

 int dummy_set_callout(profile_handler_t *handler, int type, void* callout, void* data);

 char * dummy_id(profile_class_t * clazz);

 char * dummy_description(profile_class_t * clazz);

 int dummy_capacity(profile_class_t * clazz);

 int dummy_get_handler_number(profile_class_t *clazz);

 int dummy_clockrate(profile_class_t * clazz);

 int dummy_free(profile_class_t * clazz);

 profile_handler_t * dummy_new_handler(profile_class_t * clazz, int pt);

 int dummy_free_handler(profile_handler_t * h);
