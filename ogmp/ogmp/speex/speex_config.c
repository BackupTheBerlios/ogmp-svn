/***************************************************************************
                          ogmp_server.c  -  description
                             -------------------
    begin                : Wed May 19 2004
    copyright            : (C) 2004 by Heming
    email                : heming@bigfoot.com; heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "speex_info.h"

speex_setting_t spx_setting = 
{
	/* rtp_portno, rtcp_portno, total_bandwidth, rtp_bandwidth */
	{3080, 3081, 32*1024, 16*1024},

	/* cng, penh, ptime_max */
	0,1,100
};

speex_setting_t* speex_setting(media_control_t *control)
{
	return &spx_setting;
}
