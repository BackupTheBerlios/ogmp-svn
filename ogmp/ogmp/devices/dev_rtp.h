/***************************************************************************
                          vorbis_sender.h  -  Vorbis rtp stream
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

#include <timedia/timer.h>
#include <xrtp/xrtp.h>

#include "../media_format.h"

#define MP_RTP_NONEFLAG 0x0
#define MP_RTP_LAST		0x1
#define MP_RTP_END		0x2

typedef struct rtp_setting_s rtp_setting_t;
typedef struct rtp_pipe_s rtp_pipe_t;

typedef struct dev_rtp_s dev_rtp_t;
struct dev_rtp_s{

   struct media_device_s dev;

   /* the profile module searched in catalog */
   xrtp_session_t* (*new_session)(dev_rtp_t *dev_rtp, rtp_setting_t *setting, module_catalog_t *catalog);

   int (*done_session)(dev_rtp_t *dev_rtp, xrtp_session_t * session);

   int (*session_id)(dev_rtp_t *dev_rtp, xrtp_session_t * session);
   
   xrtp_session_t* (*find_session)(dev_rtp_t *dev_rtp, int id); 

   /*
   char *cname;
   int cnlen;

   xrtp_session_t *session;
   xrtp_media_t *media;
   */
   media_pipe_t *frame_maker;
};

typedef struct rtp_callback_s rtp_callback_t;
typedef struct rtp_frame_s rtp_frame_t;

struct rtp_callback_s {

   int type;
   
   int (*callback)();
   void *callback_user;
};

struct rtp_setting_s {

   struct control_setting_s setting;

   char *cname;
   int cnlen;
   
   char *ipaddr;
   
   uint16 rtp_portno;
   uint16 rtcp_portno;

   int total_bw;
   int rtp_bw;

   int ncallback;
   rtp_callback_t *callbacks;

   uint8 profile_no;

   char *profile_mime;
};

//control_setting_t *rtp_new_setting();

struct rtp_frame_s {

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
