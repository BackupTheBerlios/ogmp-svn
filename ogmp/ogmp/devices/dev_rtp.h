/***************************************************************************
                          dev_rtp.h  -  rtp device
                             -------------------
    begin                : Mon Jan 19 2004
    copyright            : (C) 2004 by Heming Ling
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

#ifndef DEV_RTP_H
#define DEV_RTP_H

#include <timedia/timer.h>
#include <xrtp/xrtp.h>

#include "media_format.h"
#include "rtp_cap.h"

#define MP_RTP_NONEFLAG 0x0
#define MP_RTP_LAST		0x1
#define MP_RTP_END		0x2

#define MAX_NPROF		256

typedef struct rtp_pipe_s rtp_pipe_t;
typedef struct rtp_callback_s rtp_callback_t;

typedef struct dev_rtp_s dev_rtp_t;
struct dev_rtp_s
{
   struct media_device_s dev;

   xrtp_set_t *session_set;

   /* the profile module searched in catalog */
   xrtp_session_t* (*rtp_session)(dev_rtp_t *rtp,
				module_catalog_t *cata, media_control_t *ctrl,
				char *cname, int cnlen,
				char *nettype, char *addrtype, char *netaddr, 
				uint16 rtp_portno, uint16 rtcp_portno,
				uint8 profile_no, char *profile_mime, 
				int clockrate, int coding_param,
				int bw_budget);

   media_pipe_t *frame_maker;
};

typedef struct rtp_frame_s rtp_frame_t;

struct rtp_callback_s
{
   int type;
   
   int (*callback)();
   void *callback_user;
};

struct rtp_frame_s
{
   struct media_frame_s frame;

   /* group stamp:
    * For the first frame in the group, stamp is the msec of the sender time;
	* For the following packet, stamp is relative msec from the first packet;
   rtime_t usec_group_start;
   rtime_t usec_group;
    */

   /* global timestamp */
   int64 samplestamp;

   int grpno;

   int samples;

   void *media_info;
   
   long  unit_bytes;
   char *media_unit;
};

#endif
