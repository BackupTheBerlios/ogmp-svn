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

rtp_profile_setting_t rtp_profile_setting[] = 
{
	/*
	{profile_mime, profile_no, rtp_portno, rtcp_portno, total_bandwidth, rtp_bandwidth}
	*/
	{"audio/vorbis", 96, 0, 0, 256*1024, 128*1024}
};

/* ui to config rtp module */
int server_config_rtp(void *conf, control_setting_t *setting)
{
   rtp_setting_t *rset = (rtp_setting_t*)setting;

   rset->cname = SEND_CNAME;
   rset->cnlen = strlen(SEND_CNAME)+1;

   rset->ipaddr = "127.0.0.1";
   rset->default_rtp_portno = 3000;
   rset->default_rtcp_portno = 3001;

   rset->nprofile = 1;
   rset->profiles = rtp_profile_setting;

   return MP_OK;
}

int client_config_rtp(void *conf, control_setting_t *setting)
{
   rtp_setting_t *rset = (rtp_setting_t*)setting;

   rset->cname = RECV_CNAME;
   rset->cnlen = strlen(RECV_CNAME)+1;

   rset->ipaddr = "127.0.0.1";
   rset->default_rtp_portno = 4000;
   rset->default_rtcp_portno = 4001;

   rset->nprofile = 1;
   rset->profiles = rtp_profile_setting;

   return MP_OK;
}
