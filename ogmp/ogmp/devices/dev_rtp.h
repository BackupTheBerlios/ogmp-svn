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

#include <xrtp/xrtp.h>

#include "../media_format.h"

typedef struct rtp_setting_s rtp_setting_t;
typedef struct rtp_callback_s rtp_callback_t;
typedef struct rtp_frame_s rtp_frame_t;
typedef struct rtp_pipe_s rtp_pipe_t;

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
   
   int rtp_portno;
   int rtcp_portno;

   int total_bw;
   int rtp_bw;

   int ncallback;
   rtp_callback_t *callbacks;

   int profile_no;

   char *profile_mime;
};

control_setting_t *rtp_new_setting();

struct rtp_frame_s {

   struct media_frame_s frame;

   int samplestamp;
   void *media_info;
   
   long  unit_bytes;
   char *media_unit;
};

struct rtp_pipe_s {

   struct media_pipe_s pipe;

   xrtp_media_t *rtp_media;
};
