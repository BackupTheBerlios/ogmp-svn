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

#include "ogmp.h"

rtp_profile_set_t profile_list = 
{
	/*
	{profile_mime, profile_no, rtp_portno, rtcp_portno, total_bandwidth, rtp_bandwidth}
	{"audio/vorbis", 97, 0, 0, 256*1024, 128*1024}
	*/
	1, {"audio/speex", 8000}
};

/* ui to config rtp module */
int server_config_rtp(void *conf, control_setting_t *setting)
{
   rtp_setting_t *rset = (rtp_setting_t*)setting;

   rset->default_rtp_portno = 3004;
   rset->default_rtcp_portno = 3005;

   rset->default_profiles = profile_list;

   return MP_OK;
}

int client_config_rtp(void *conf, control_setting_t *setting)
{
   rtp_setting_t *rset = (rtp_setting_t*)setting;

   rset->default_rtp_portno = 4004;
   rset->default_rtcp_portno = 4005;

   rset->default_profiles = profile_list;

   return MP_OK;
}

ogmp_setting_t serv_setting = 
{
	"IN",		/* nettype */
	"IP4",		/* addrtype */
	3004,		/* default rtp portno */
	3005,		/* default rtcp portno */
	1, 
	{"audio/speex", 8000, 1}
};

ogmp_setting_t clie_setting = 
{
	"IN",		/* nettype */
	"IP4",		/* addrtype */
	4004,		/* default rtp portno */
	4005,		/* default rtcp portno */
	1, 
	{"audio/speex", 8000, 1}
};

ogmp_setting_t* server_setting(media_control_t *control)
{
	return &serv_setting;
}

ogmp_setting_t* client_setting(media_control_t *control)
{
	return &clie_setting;
}
