/***************************************************************************
                          packet_const.h  -  Const value for RTP Protocol
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

 #define RTP_VERSION 2
 
 #define XRTP_MAX_CSRC    16
 #define XRTP_MAX_RTCP_COUNT    31
 #define XRTP_MAX_REPORTS    31
 #define XRTP_MAX_RTCP_PACKETS    31      /* Need consider again */
 #define XRTP_MAX_CHUNK_ITEM      8       /* Max type */
 #define XRTP_MAX_SDES_CHUNK      31      /* 5 bits count */ 
 #define XRTP_MAX_BYE_SRCS        31      /* 5 bits count */

 #define XRTP_RTCP_DEFAULTMAXBYTES  1024  /* Default RTCP Packet Length */
 
 #define RTP_MIN_PAD  1
 #define RTP_MAX_PAD  3

 #define RTP_HEAD_FIX_BYTE 12
 #define RTP_HEADEXT_FIX_BYTE 4
 #define RTP_CSRC_BYTE 4

 #define RTCP_SRC_BYTE 4
 #define RTCP_SDES_ID_BYTE 1
 #define RTCP_SDES_LEN_BYTE 1
 
 #define RTCP_HEAD_FIX_BYTE 4
 #define RTCP_SSRC_BYTE 4
 #define RTCP_SENDERINFO_BYTE 24
 #define RTCP_REPORT_BYTE 24
 #define RTCP_WHY_BYTE 1
 #define RTCP_APP_NAME_BYTE 4

 #define RTCP_SDES_END     0
 #define RTCP_SDES_CNAME   1
 #define RTCP_SDES_NAME    2
 #define RTCP_SDES_EMAIL   3
 #define RTCP_SDES_PHONE   4
 #define RTCP_SDES_LOG     5
 #define RTCP_SDES_TOOL    6
 #define RTCP_SDES_NOTE    7
 #define RTCP_SDES_PRIV    8
 
 #define RTCP_TYPE_SR    200
 #define RTCP_TYPE_RR    201
 #define RTCP_TYPE_SDES  202
 #define RTCP_TYPE_BYE   203
 #define RTCP_TYPE_APP   204

 #define XRTP_SRC_NULL  0x0

 #define RTP_PADDING_VALUE 0x0

 #define LITTLEEND_ORDER 0
 #define BIGEND_ORDER 1

 #define HOST_BYTEORDER LITTLEEND_ORDER
 #define NET_BYTEORDER BIGEND_ORDER
 
 
