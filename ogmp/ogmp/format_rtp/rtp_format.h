/***************************************************************************
                          rtp_format.h  -  Format defined by rtp capabilities
                             -------------------
    begin                : Web Jul 22 2004
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

#ifndef RTP_FORMAT_H

#define RTP_FORMAT_H

#include "../devices/dev_rtp.h"

#include <stdio.h>
#include <timedia/timer.h>
#include <xrtp/xrtp.h>

typedef struct rtp_stream_s rtp_stream_t;
typedef struct rtp_format_s rtp_format_t;
typedef struct rtp_media_s rtp_media_t;

struct rtp_stream_s
{
   struct  media_stream_s  stream;
   
   int						sno;
   char						*source_cname;

   rtpcap_descript_t		*rtp_cap;
   xrtp_session_t			*session;
   int						bandwidth;

   rtp_format_t			*rtp_format;
};

struct rtp_format_s
{
	struct  media_format_s  format;

	/**
	 * Open rtp format according to rtcap set from SDP answered, and add participant of SDP
	 * So is ready to receive remote media.
	 * return used bandwidth
	 */
	int (*open_capables) (media_format_t *mf, rtpcap_set_t *rtpcapset, media_control_t *ctrl, char *mode, int bandwidth);

   /**
	 * Open local media source for sending to remote end.
	 *
	 * Retrieve the rtp media of the format, rtp_sender use it for sendout.
	 * Actually, for each rtp member there will be a player associated to them in each rtp stream:
	 * The player of local member(self) is a sender. And sender need to give a media source.
	 * All other player recieve remote media for each member.
	 */
	dev_rtp_t *rtp_in;
   
	int millisec;
	int bandwidth;
};

/* New detected stream add to group */
int rtp_add_stream(media_format_t * rtp, media_stream_t *strm, int strmno, unsigned char type);

#endif
