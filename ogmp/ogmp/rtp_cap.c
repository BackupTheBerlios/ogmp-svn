/*
 * josua - Jack's open sip User Agent
 *
 * Copyright (C) 2002,2003   Aymeric Moizard <jack@atosc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <timedia/xstring.h>

#include "rtp_cap.h"

#define CAP_LOG

#ifdef CAP_LOG
 #define cap_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define cap_log(fmtargs)
#endif

/**************************************************************
 * rtp capable descript module
 */
int rtp_descript_done(capable_descript_t *cap)
{
	rtpcap_descript_t *rtpcap = (rtpcap_descript_t*)cap;

	xstr_done_string(rtpcap->profile_mime);
	xstr_done_string(rtpcap->ipaddr);
	free(rtpcap);


	return MP_OK;
}

int rtp_descript_match_value(capable_descript_t *cap, char *type, char *value)
{
	rtpcap_descript_t *rtpcap = (rtpcap_descript_t*)cap;

	if(0 != strncmp("mime", type, 4))
		return 0;

	return (0 == strcmp(rtpcap->profile_mime, value));
}

int rtp_descript_match(capable_descript_t *me, capable_descript_t *oth)
{
	rtpcap_descript_t *rtpoth;
	rtpcap_descript_t *rtpme = (rtpcap_descript_t*)me;

	rtpoth = (rtpcap_descript_t*)oth;

	return oth->match_value(oth, "mime", rtpme->profile_mime);
}

rtpcap_descript_t* rtp_capable_descript(int payload_no, char *ip, uint media_port, uint control_port, char *mime, int clockrate, int coding_param)
{
	rtpcap_descript_t *rtpcap;

	rtpcap = malloc(sizeof(rtpcap_descript_t));
	if(!rtpcap)
	{
		return NULL;
	}
	memset(rtpcap, 0, sizeof(rtpcap_descript_t));

	rtpcap->descript.done = rtp_descript_done;
	rtpcap->descript.match = rtp_descript_match;
	rtpcap->descript.match_value = rtp_descript_match_value;

	rtpcap->profile_mime = xstr_clone(mime);
	rtpcap->profile_no = payload_no;

	rtpcap->ipaddr = xstr_clone(ip);

	rtpcap->rtp_portno = media_port;
	rtpcap->rtcp_portno = control_port;

	rtpcap->clockrate = clockrate;
	rtpcap->coding_param = coding_param;

	return rtpcap;
}

