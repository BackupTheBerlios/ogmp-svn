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

#include "media_format.h"

typedef struct rtpcap_descript_s rtpcap_descript_t; 
struct rtpcap_descript_s
{
	struct capable_descript_s descript;

	char *profile_mime;
	uint8 profile_no;

	char *ipaddr;

	uint16 rtp_portno;
	uint16 rtcp_portno;

	int clockrate;
	int coding_param;
};

rtpcap_descript_t* rtp_capable_descript(int payload_no, char *ip, uint media_port, uint control_port, char *mime, int clockrate, int coding_param);
